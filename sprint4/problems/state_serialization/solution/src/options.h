#pragma once
#include <filesystem>
#include <optional>

namespace options {

struct Args {
    std::uint64_t tickPeriod{};
    std::filesystem::path pathToConfig;
    std::filesystem::path pathToStatic;
    std::filesystem::path pathToStateFile;
    std::uint64_t saveStatePeriod{};
    bool randomizeSpawnPoints{};
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]);
}  // namespace options