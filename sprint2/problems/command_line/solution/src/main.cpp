#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
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

struct Args {
    std::uint64_t tickPeriod{};
    std::filesystem::path pathToConfig;
    std::filesystem::path pathToStatic;
    bool randomizeSpawnPoints{};
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    Args args;
    po::options_description desc{"All options"s};
    auto add = desc.add_options();
    add("help,h", "produce help message");
    add("tick-period,t", po::value(&args.tickPeriod)->value_name("milliseconds"s), "set tick period");
    add("config-file,c", po::value(&args.pathToConfig)->value_name("file"s), "set config file path");
    add("www-root,w", po::value(&args.pathToStatic)->value_name("dir"s), "set static files root");
    add("randomize-spawn-points", po::bool_switch(&args.randomizeSpawnPoints), "spawn dogs at random positions");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.contains("help")) {
            std::cout << desc << '\n';
            return std::nullopt;
        }
        po::notify(vm);

    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << '\n';
        std::cerr << desc << '\n';
        return std::nullopt;
    }
    return args;
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}, period_{period}, handler_{std::move(handler)} {}

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] { self->ScheduleTick(); });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) { self->OnTick(ec); });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

int main(int argc, const char* argv[]) {
    auto args = ParseCommandLine(argc, argv);
    if (!args) {
        return EXIT_SUCCESS;
    }
    if (args->pathToConfig.empty()) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    if (args->pathToStatic.empty()) {
        args->pathToStatic = "/app/static";
    }

    logger::InitBoostLogFilter();
    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args->pathToConfig);
        game.SetRandomSpawn(args->randomizeSpawnPoints);

        app::Application application{std::move(game)};
        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_handler::RequestHandler handler{args->pathToStatic, api_strand, application};

        logger::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        if (args->tickPeriod > 0) {
            auto ticker = std::make_shared<Ticker>(
                api_strand, std::chrono::milliseconds{args->tickPeriod}, [&application](std::chrono::milliseconds ms) {
                    application.MakeTick(static_cast<std::uint64_t>(ms.count()));
                });

            ticker->Start();
        }
        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        // Подписываемся на сигналы и при их полчении завершаем работу сервера
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
