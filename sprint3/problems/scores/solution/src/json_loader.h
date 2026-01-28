#pragma once

#include <filesystem>

#include "extra_data.h"
#include "loot_generator.h"
#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);
loot_gen::LootGenerator LoadGenerator(const std::filesystem::path& json_path);
extra_data::ExtraData LoadExtra(const std::filesystem::path& json_path);

}  // namespace json_loader
