#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
// #include <exception>
//?

#include "http_server.h"
#include "json_loader.h"
#include "my_logger.h"
#include "request_handler.h"
#include "sdk.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

template <class Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler) : handler_(handler) {}

    template <class Request, class Send>
    void operator()(Request&& req, Send&& send) {
        LogRequest(req);

        // Wrap send to intercept the response
        auto logged_send = [this, &send](auto&& response) {
            LogResponse(response);
            send(std::forward<decltype(response)>(response));
        };

        handler_(std::forward<Request>(req), logged_send);
    }

private:
    Handler& handler_;
    std::chrono::system_clock::time_point start_ts_{};
    template <class Request>
    void LogRequest(const Request& req) {
        start_ts_ = std::chrono::system_clock::now();
    }

    template <class Response>
    void LogResponse(const Response& resp) {
        auto elapsed = std::chrono::system_clock::now() - start_ts_;

        std::string content_type = "unknown";
        if (resp.find(boost::beast::http::field::content_type) != resp.end()) {
            content_type = std::string(resp[boost::beast::http::field::content_type]);
        }

        logger::LogServerResponse(std::to_string(elapsed.count()), resp.result_int(), content_type);
    }
};

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    logger::InitBoostLogFilter();
    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        // Подписываемся на сигналы и при их получении завершаем работу сервера
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                // Choose reason based on signal
                const char* reason = nullptr;
                switch (signal_number) {
                    case SIGINT:
                        reason = "SIGINT received";
                        break;
                    case SIGTERM:
                        reason = "SIGTERM received";
                        break;
                    default:
                        reason = "Unknown termination signal";
                        break;
                }
                logger::LogServerStop(signal_number, reason);
                ioc.stop();
            }
        });
        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game, argv[2]};
        LoggingRequestHandler logging_handler{handler};
        //  5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        logger::LogServerLaunch({address, port});
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        std::cout << "Server has started..."sv << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });
    } catch (const std::exception& ex) {
        // std::cerr << ex.what() << std::endl;std::exception::what()
        logger::LogServerStop(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
}
