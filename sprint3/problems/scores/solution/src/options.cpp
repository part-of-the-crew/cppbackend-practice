#include "options.h"

#include <boost/program_options.hpp>
#include <iostream>

using namespace std::literals;

namespace options {
[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    Args args;
    po::options_description desc{"All options"s};
    auto add = desc.add_options();
    add("help,h", "produce help message");
    add("tick-period,t", po::value(&args.tickPeriod)->value_name("milliseconds"s), "set tick period");
    add("config-file,c", po::value(&args.pathToConfig)->value_name("file"s), "set config file path");
    add("www-root,w", po::value(&args.pathToStatic)->value_name("dir"s), "set static files root");
    add("randomize-spawn-points", po::bool_switch(&args.randomizeSpawnPoints),
        "spawn dogs at random positions");

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

}  // namespace options
