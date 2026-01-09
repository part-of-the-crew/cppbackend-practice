// model.cpp
#include "model.h"

#include <algorithm>  // Для std::min, std::max
#include <random>     // Для генерации случайных чисел
#include <stdexcept>

namespace model {
using namespace std::literals;

// ... (методы Map::AddOffice, Game::AddMap, Game::FindMap, Game::FindSession остаются без изменений) ...

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

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
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

// -------------------------------------------------------------
// Реализация логики появления пса
// -------------------------------------------------------------

Dog* GameSession::AddDog(std::string_view name) {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        throw std::runtime_error("Map has no roads to spawn a dog");
    }

    // Инициализируем генератор случайных чисел
    // (лучше сделать его static thread_local, чтобы не создавать каждый раз)
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());

    // 1. Выбираем случайную дорогу (равномерное распределение по индексу)
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(gen)];

    // 2. Генерируем случайную точку на этой дороге
    Position start_pos;

    if (road.IsHorizontal()) {
        // Дорога горизонтальная: Y фиксирован, X меняется
        start_pos.y = static_cast<double>(road.GetStart().y);

        double min_x = std::min(road.GetStart().x, road.GetEnd().x);
        double max_x = std::max(road.GetStart().x, road.GetEnd().x);

        std::uniform_real_distribution<double> coord_dist(min_x, max_x);
        start_pos.x = coord_dist(gen);
    } else {
        // Дорога вертикальная: X фиксирован, Y меняется
        start_pos.x = static_cast<double>(road.GetStart().x);

        double min_y = std::min(road.GetStart().y, road.GetEnd().y);
        double max_y = std::max(road.GetStart().y, road.GetEnd().y);

        std::uniform_real_distribution<double> coord_dist(min_y, max_y);
        start_pos.y = coord_dist(gen);
    }

    // Создаем пса с вычисленными координатами
    // Начальная скорость (0,0) и направление (NORTH) задаются в конструкторе Dog
    dogs_.emplace_back(std::string(name), next_dog_id_++, start_pos);

    return &dogs_.back();
}

}  // namespace model