#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std::string_literals;

namespace json_loader {
namespace json = boost::json;

auto coord = [](const boost::json::value& v) { return model::Coord(v.as_int64()); };

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

model::Road ParseRoad(const json::object& obj) {
    model::Point start{coord(obj.at("x0"s).as_int64()), coord(obj.at("y0"s).as_int64())};
    if (const auto it = obj.find("x1"s); it != obj.cend()) {
        return {model::Road::HORIZONTAL, start, static_cast<int>(it->value().as_int64())};
    }
    return {model::Road::VERTICAL, start, static_cast<int>(obj.at("y1"s).as_int64())};
}

model::Building ParseBuilding(const json::object& obj) {
    return model::Building{model::Rectangle{
        .position = {coord(obj.at("x"s)), coord(obj.at("y"s))}, .size = {coord(obj.at("w"s)), coord(obj.at("h"s))}}};
}

model::Office ParseOffice(const json::object& obj) {
    return model::Office{model::Office::Id{std::string(obj.at("id"s).as_string())},
        model::Point{coord(obj.at("x"s)), coord(obj.at("y"s))},
        model::Offset{coord(obj.at("offsetX"s)), coord(obj.at("offsetY"s))}};
}

model::Map ParseMap(const json::value& map_json) {
    const auto& desc = map_json.as_object();
    model::Map map(model::Map::Id{std::string(desc.at("id"s).as_string())}, std::string(desc.at("name"s).as_string()));
    
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

}  // namespace json_loader
