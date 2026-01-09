#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>
#include <iostream>
#include <thread>

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

int main(int argc, const char* argv[]) {
    if (argc == 1) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    std::filesystem::path path_to_static{"/app/static"};
    if (argc == 3) {
        path_to_static = argv[2];
    }

    logger::InitBoostLogFilter();
    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);
        app::Application application{std::move(game)};
        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_handler::RequestHandler handler{path_to_static, api_strand, application};

        logger::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);
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
                boost::log::core::get()->flush();  // Force write to console
                ioc.stop();
            }
        });

        logger::LogServerLaunch({address, port});
        // 6. Запускаем обработку асинхронных операций
        RunWorkers(num_threads, [&ioc] { ioc.run(); });
    } catch (const std::exception& ex) {
        logger::LogServerStop(EXIT_FAILURE, ex.what());
        boost::log::core::get()->flush();  // Force write to console
        return EXIT_FAILURE;
    }
}
