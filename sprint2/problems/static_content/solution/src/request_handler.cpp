#include "request_handler.h"

namespace http_handler {
/*
template <typename Request>
ResponseVariant RequestHandler::HandleAPI(const Request& req) {
    auto target = req.target();

    if (target == "/api/v1/maps") {
        json::array arr;
        for (const auto& map : game_.GetMaps()) {
            arr.push_back({{"id", *map.GetId()}, {"name", map.GetName()}});
        }
        return responses::MakeJSON(http::status::ok, std::move(arr), req);
    }

    // Default error
    return responses::MakeError(http::status::bad_request, "badRequest", "Invalid API path", req);
}
*/

std::string UrlDecode(std::string_view text) {
    std::string res;
    res.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%') {
            if (i + 2 < text.size()) {
                char hex[] = {text[i + 1], text[i + 2], '\0'};
                char* endptr;
                long val = std::strtol(hex, &endptr, 16);
                if (endptr == hex + 2) {
                    res += static_cast<char>(val);
                    i += 2;
                } else {
                    res += '%';
                }
            } else {
                res += '%';
            }
        } else if (text[i] == '+') {
            res += ' ';
        } else {
            res += text[i];
        }
    }
    return res;
}

std::string_view DefineMIMEType(const std::filesystem::path& path) {
    using namespace std::literals;

    // Get extension and convert to lowercase for comparison
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".htm"sv || ext == ".html"sv)
        return "text/html"sv;
    if (ext == ".css"sv)
        return "text/css"sv;
    if (ext == ".txt"sv)
        return "text/plain"sv;
    if (ext == ".js"sv)
        return "text/javascript"sv;
    if (ext == ".json"sv)
        return "application/json"sv;
    if (ext == ".xml"sv)
        return "application/xml"sv;
    if (ext == ".png"sv)
        return "image/png"sv;
    if (ext == ".jpg"sv || ext == ".jpe"sv || ext == ".jpeg"sv)
        return "image/jpeg"sv;
    if (ext == ".gif"sv)
        return "image/gif"sv;
    if (ext == ".bmp"sv)
        return "image/bmp"sv;
    if (ext == ".ico"sv)
        return "image/vnd.microsoft.icon"sv;
    if (ext == ".tiff"sv || ext == ".tif"sv)
        return "image/tiff"sv;
    if (ext == ".svg"sv || ext == ".svgz"sv)
        return "image/svg+xml"sv;
    if (ext == ".mp3"sv)
        return "audio/mpeg"sv;

    // Default for unknown files
    return "application/octet-stream"sv;
}

json::value RequestHandler::HandleMaps() {
    json::array json_maps;
    for (const auto& map : game_.GetMaps()) {
        json::object json_map;
        json_map["id"] = *map.GetId();
        json_map["name"] = map.GetName();
        json_maps.emplace_back(std::move(json_map));
    }
    return json_maps;
}

std::pair<json::value, bool> RequestHandler::HandleMapId(std::string_view name_map) {
    const auto* map = game_.FindMap(model::Map::Id{std::string(name_map)});
    if (!map) {
        return {json::value{}, true};
    }
    return {SerializeMap(*map), false};
}

json::object RequestHandler::SerializeMap(const model::Map& map) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();

    json::array roads;
    for (const auto& r : map.GetRoads())
        roads.push_back(SerializeRoad(r));
    map_obj["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& b : map.GetBuildings())
        buildings.push_back(SerializeBuilding(b));
    map_obj["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& o : map.GetOffices())
        offices.push_back(SerializeOffice(o));
    map_obj["offices"] = std::move(offices);

    return map_obj;
}

json::object RequestHandler::SerializeRoad(const model::Road& road) {
    json::object obj;
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();
    obj["x0"] = start.x;
    obj["y0"] = start.y;
    if (start.y == end.y)
        obj["x1"] = end.x;
    else
        obj["y1"] = end.y;
    return obj;
}

json::object RequestHandler::SerializeBuilding(const model::Building& b) {
    const auto& bounds = b.GetBounds();
    return {{"x", bounds.position.x}, {"y", bounds.position.y}, {"w", bounds.size.width}, {"h", bounds.size.height}};
}

json::object RequestHandler::SerializeOffice(const model::Office& o) {
    return {{"id", *o.GetId()},
            {"x", o.GetPosition().x},
            {"y", o.GetPosition().y},
            {"offsetX", o.GetOffset().dx},
            {"offsetY", o.GetOffset().dy}};
}

}  // namespace http_handler