#include "json_loader.h"

#include <boost/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "extra_data_serialization.h"

using namespace std::string_literals;

namespace json_loader {
namespace json = boost::json;

auto coord = [](const boost::json::value& v) { return model::Coord(v.as_int64()); };

std::string ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open loot configuration file: "s + json_path.string());
    }

    // Read the entire file into a string
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

model::Road ParseRoad(const json::object& obj) {
    model::Point start{coord(obj.at("x0"s).as_int64()), coord(obj.at("y0"s).as_int64())};
    if (const auto it = obj.find("x1"s); it != obj.cend()) {
        return {model::Road::HORIZONTAL, start, static_cast<int>(it->value().as_int64())};
    }
    return {model::Road::VERTICAL, start, static_cast<int>(obj.at("y1"s).as_int64())};
}

model::Building ParseBuilding(const json::object& obj) {
    return model::Building{model::Rectangle{.position = {coord(obj.at("x"s)), coord(obj.at("y"s))},
        .size = {coord(obj.at("w"s)), coord(obj.at("h"s))}}};
}

model::Office ParseOffice(const json::object& obj) {
    return model::Office{model::Office::Id{std::string(obj.at("id"s).as_string())},
        model::Point{coord(obj.at("x"s)), coord(obj.at("y"s))},
        model::Offset{coord(obj.at("offsetX"s)), coord(obj.at("offsetY"s))}};
}

model::Map ParseMap(const json::value& map_json) {
    const auto& desc = map_json.as_object();
    model::Map map(
        model::Map::Id{std::string(desc.at("id"s).as_string())}, std::string(desc.at("name"s).as_string()));

    if (const auto it = desc.find("dogSpeed"s); it != desc.cend())
        map.SetDogSpeed(it->value().as_double());

    for (const auto& r : desc.at("roads"s).as_array()) {
        map.AddRoad(ParseRoad(r.as_object()));
    }
    for (const auto& b : desc.at("buildings"s).as_array()) {
        map.AddBuilding(ParseBuilding(b.as_object()));
    }
    for (const auto& o : desc.at("offices"s).as_array()) {
        map.AddOffice(ParseOffice(o.as_object()));
    }
    return map;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path и Распарсить строку как JSON, используя boost::json::parse
    auto content = json::parse(ReadFile(json_path));

    // Загрузить модель игры из файла
    model::Game game;

    const auto& root = content.as_object();
    const auto it = root.find("maps"s);
    if (it == root.cend())
        throw std::runtime_error("JSON has no 'maps'");

    if (root.contains("defaultDogSpeed"s)) {
        game.SetSpeed(root.at("defaultDogSpeed"s).as_double());
    }
    for (const auto& map_json : it->value().as_array()) {
        game.AddMap(ParseMap(map_json));
    }
    return game;
}

loot_gen::LootGenerator LoadGenerator(const std::filesystem::path& json_path) {
    std::string content = ReadFile(json_path);
    try {
        // Parse the JSON string
        json::value value = json::parse(content);

        // Navigate to the "lootGeneratorConfig" object
        const auto& config_obj = value.as_object().at("lootGeneratorConfig").as_object();

        // Extract period and probability.
        // We use to_number<double>() to handle both integer (5) and double (5.0) values.
        double period_seconds = config_obj.at("period").to_number<double>();
        double probability = config_obj.at("probability").to_number<double>();

        // Convert seconds to milliseconds for the generator
        auto period_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(period_seconds));

        return loot_gen::LootGenerator{period_ms, probability};

    } catch (const std::exception& e) {
        // Re-throw with more context if fields are missing or types are wrong
        throw std::runtime_error("JSON parsing error in lootGeneratorConfig: "s + e.what());
    }
}

// Parse JSON text into boost::json::value. Throws std::runtime_error on parse error.
inline json::value ParseJsonText(const std::string& txt, const std::string& context) {
    try {
        return json::parse(txt);
    } catch (const json::system_error& e) {
        std::string msg = "ParseJsonText: JSON parse error";
        if (!context.empty())
            msg += " (" + context + ")";
        msg += ": ";
        msg += e.what();
        throw std::runtime_error(msg);
    }
}

extra_data::ExtraData LoadExtra(const std::filesystem::path& json_path) {
    // 1. I/O Layer
    const std::string text = ReadFile(json_path);

    // 2. Parsing Layer (Boost.JSON)
    const json::value root = ParseJsonText(text, json_path.string());

    // 3. Logic Layer (Your new function)
    return extra_data_ser::ExtractExtraData(root);
}

}  // namespace json_loader
