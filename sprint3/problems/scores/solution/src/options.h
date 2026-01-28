#pragma once
#include <filesystem>
#include <optional>

namespace options {

struct Args {
    std::uint64_t tickPeriod{};
    std::filesystem::path pathToConfig;
    std::filesystem::path pathToStatic;
    bool randomizeSpawnPoints{};
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]);
}  // namespace options