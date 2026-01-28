#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "my_logger.h"

namespace http_server {

using namespace std::literals;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

inline void ReportError(beast::error_code ec, std::string_view what) {
    logger::LogNetError(ec.value(), ec.message(), what);
}

class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    // Запрещаем копирование и присваивание объектов SessionBase и его наследников
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run();

    // protected:
    explicit SessionBase(tcp::socket&& socket) : stream_(std::move(socket)) {}

    using HttpRequest = http::request<http::string_body>;

    virtual ~SessionBase() = default;

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

        auto self = shared_from_this();

        http::async_write(
            stream_, *safe_response, [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                self->OnWrite(safe_response->need_eof(), ec, bytes_written);
            });
    }

private:
    void Read() {
        using namespace std::literals;
        // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
        request_ = {};
        stream_.expires_after(30s);
        // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
        http::async_read(
            stream_, buffer_, request_, beast::bind_front_handler(&SessionBase::OnRead, shared_from_this()));
    }

    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        using namespace std::literals;
        if (ec == http::error::end_of_stream) {
            // Нормальная ситуация - клиент закрыл соединение
            return Close();
        }
        if (ec) {
            return ReportError(ec, "read"sv);
        }
        HandleRequest(std::move(request_));
    }
    void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        if (ec) {
            return ReportError(ec, "write"sv);
        }

        if (close) {
            // Семантика ответа требует закрыть соединение
            return Close();
        }

        // Считываем следующий запрос
        Read();
    }
    void Close() {
        beast::error_code ec;
        ec = stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        ReportError(ec, "tcp::socket::shutdown_send"sv);
    }

    // Обработку запроса делегируем подклассу
    virtual void HandleRequest(HttpRequest&& request) = 0;

    // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
protected:
    beast::tcp_stream stream_;

private:
    beast::flat_buffer buffer_;
    HttpRequest request_;
    // virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket)), request_handler_(std::forward<Handler>(request_handler)) {
        endpoint_ = SessionBase::stream_.socket().remote_endpoint();
    }

private:
    // FIX 2: Implement the pure virtual method to handle the request.
    void HandleRequest(HttpRequest&& request) override {
        tcp::endpoint remote = SessionBase::stream_.socket().remote_endpoint();
        std::string ip = endpoint_.address().to_string();
        std::string url(request.target());
        std::string method(request.method_string());

        logger::LogServerRequest(ip, url, method);
        request_handler_(remote, std::move(request), [self = shared_from_this()](auto&& response) {
            self->Write(std::forward<decltype(response)>(response));
        });
    }
    tcp::endpoint endpoint_;
    RequestHandler& request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    void Run() { DoAccept(); }

    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(request_handler)) {
        // Открываем acceptor, используя протокол (IPv4 или IPv6), указанный в endpoint
        acceptor_.open(endpoint.protocol());

        // После закрытия TCP-соединения сокет некоторое время может считаться занятым,
        // чтобы компьютеры могли обменяться завершающими пакетами данных.
        // Однако это может помешать повторно открыть сокет в полузакрытом состоянии.
        // Флаг reuse_address разрешает открыть сокет, когда он "наполовину закрыт"
        acceptor_.set_option(net::socket_base::reuse_address(true));
        // Привязываем acceptor к адресу и порту endpoint
        acceptor_.bind(endpoint);
        // Переводим acceptor в состояние, в котором он способен принимать новые соединения
        // Благодаря этому новые подключения будут помещаться в очередь ожидающих соединений
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            // Передаём последовательный исполнитель, в котором будут вызываться обработчики
            // асинхронных операций сокета
            net::make_strand(ioc_),
            // С помощью bind_front_handler создаём обработчик, привязанный к методу OnAccept
            // текущего объекта.
            // Так как Listener — шаблонный класс, нужно подсказать компилятору, что
            // shared_from_this — метод класса, а не свободная функция.
            // Для этого вызываем его, используя this
            // Этот вызов bind_front_handler аналогичен
            // namespace ph = std::placeholders;
            // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    // Метод socket::async_accept создаст сокет и передаст его в OnAccept
    void OnAccept(sys::error_code ec, tcp::socket socket) {
        using namespace std::literals;

        if (ec) {
            return ReportError(ec, "accept"sv);
        }

        // Асинхронно обрабатываем сессию
        AsyncRunSession(std::move(socket));

        // Принимаем новое соединение
        DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler& request_handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    // При помощи decay_t исключим ссылки из типа RequestHandler,
    // чтобы Listener хранил RequestHandler по значению
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server
