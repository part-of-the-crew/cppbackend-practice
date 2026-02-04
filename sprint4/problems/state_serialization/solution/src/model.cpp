#include "model.h"

#include <algorithm>  // For std::min, std::max
#include <cmath>
#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }
    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);

    // Build spatial index
    if (road.IsHorizontal()) {
        roads_by_y_[road.GetStart().y].push_back(road);
    } else {
        roads_by_x_[road.GetStart().x].push_back(road);
    }
}

const Map::Roads& Map::GetRoadsByX(geom::Coord x) const {
    static const Roads empty;
    auto it = roads_by_x_.find(x);
    return it != roads_by_x_.end() ? it->second : empty;
}

const Map::Roads& Map::GetRoadsByY(geom::Coord y) const {
    static const Roads empty;
    auto it = roads_by_y_.find(y);
    return it != roads_by_y_.end() ? it->second : empty;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            if (map.GetDogSpeed() < 0.0) {
                map.SetDogSpeed(speed_);
            }
            if (map.GetBagCapacity() < 0.0) {
                map.SetDogSpeed(defaultBagCapacity_);
            }
            map.SetRandomSpawn(randomSpawn_);
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::FindSession(const Map::Id& id) {
    if (auto it = map_id_to_session_.find(id); it != map_id_to_session_.end()) {
        return it->second.get();
    }
    const Map* map = FindMap(id);
    if (!map) {
        return nullptr;
    }
    auto session = std::make_shared<GameSession>(map);
    map_id_to_session_[id] = session;
    return session.get();
}

geom::Position Map::GetRandomPositionOnRoad(std::mt19937& gen) const {
    std::uniform_int_distribution<size_t> road_dist(0, roads_.size() - 1);
    const auto& road = roads_.at(road_dist(gen));

    auto uniform = [&](double a, double b) {
        std::uniform_real_distribution<double> d(std::min(a, b), std::max(a, b));
        return d(gen);
    };
    if (road.IsHorizontal()) {
        return geom::Position{
            .x = uniform(road.GetStart().x, road.GetEnd().x), .y = static_cast<double>(road.GetStart().y)};
    }
    return geom::Position{
        .x = static_cast<double>(road.GetStart().x), .y = uniform(road.GetStart().y, road.GetEnd().y)};
}

Dog* GameSession::AddDogByName(std::string_view name) {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        throw std::runtime_error("Map has no roads to spawn a dog");
    }
    /*

    */
    geom::Position start_pos;
    if (map_->GetRandomSpawn()) {
        start_pos = map_->GetRandomPositionOnRoad(gen_);
    } else {
        // Determine spawn point: start of the first road
        const auto& first_road = roads[0];
        start_pos.x = static_cast<double>(first_road.GetStart().x);
        start_pos.y = static_cast<double>(first_road.GetStart().y);
    }

    dogs_.emplace_back(std::string(name), next_dog_id_++, start_pos);

    return &dogs_.back();
}
void GameSession::AddDog(Dog dog) {
    dogs_.push_back(std::move(dog));
}
}  // namespace model