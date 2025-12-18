#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std::string_literals;

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    std::ifstream in(json_path);
    if (!in) {
        throw std::runtime_error("Failed to open file");
    }
    // Распарсить строку как JSON, используя boost::json::parse
    const auto content =
        boost::json::parse(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));

    // Загрузить модель игры из файла
    model::Game game;

    auto coord = [](const boost::json::value& v) { return model::Coord(v.as_int64()); };
    // coord(obj.at("x"s))
    const auto& root = content.as_object();
    if (!root.contains("maps"s)) {
        throw std::runtime_error("JSON has no 'maps'");
    }

    for (const auto& map_json : root.at("maps"s).as_array()) {
        const auto& desc = map_json.as_object();

        model::Map map(model::Map::Id{std::string(desc.at("id"s).as_string())},
                       std::string(desc.at("name"s).as_string()));

        // Buildings
        for (const auto& e : desc.at("buildings"s).as_array()) {
            const auto& obj = e.as_object();
            model::Point p(coord(obj.at("x"s)), coord(obj.at("y"s)));
            model::Size s(coord(obj.at("w"s)), coord(obj.at("h"s)));
            map.AddBuilding(model::Building{model::Rectangle{p, s}});
        }

        // Offices
        for (const auto& e : desc.at("offices"s).as_array()) {
            const auto& obj = e.as_object();
            map.AddOffice(model::Office{model::Office::Id{std::string(obj.at("id"s).as_string())},
                                        model::Point{coord(obj.at("x"s)), coord(obj.at("y"s))},
                                        model::Offset{coord(obj.at("offsetX"s)), coord(obj.at("offsetY"s))}});
        }

        // Roads
        for (const auto& e : desc.at("roads"s).as_array()) {
            const auto& obj = e.as_object();
            model::Point start(coord(obj.at("x0"s)), coord(obj.at("y0"s)));

            if (const auto it = obj.find("x1"s); it != obj.end()) {
                map.AddRoad({model::Road::HORIZONTAL, start, coord(it->value())});
            } else {
                map.AddRoad({model::Road::VERTICAL, start, coord(obj.at("y1"s))});
            }
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
