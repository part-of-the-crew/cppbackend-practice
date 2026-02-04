#include "app.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>

#include "collision_detector.h"

namespace app {

constexpr double eps = 1e9;

using namespace std::literals;

Token PlayerTokens::GenerateToken() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << generator1_();
    ss << std::setw(16) << generator2_();
    return ss.str();
}

Token PlayerTokens::AddPlayer(Player player) {
    Token token;
    do {
        token = GenerateToken();
    } while (token_to_player_.contains(token));

    AddTokenUnsafe(token, std::move(player));
    return token;
}

void PlayerTokens::AddTokenUnsafe(const Token& token, Player player) {
    token_to_player_.emplace(token, std::move(player));
}

Player* PlayerTokens::FindPlayer(Token token) {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::size_t PlayerTokens::GetPlayerNumber() const {
    return token_to_player_.size();
}

std::optional<JoinGameResult> Application::JoinGame(const AuthRequest& authReq) {
    const auto* map = game_.FindMap(model::Map::Id{authReq.map});
    if (!map) {
        return std::nullopt;
    }

    auto* session = game_.FindSession(model::Map::Id{authReq.map});
    if (!session) {
        return std::nullopt;
    }

    auto* dog = session->AddDogByName(authReq.playerName);

    Player player{session, dog};
    Token token = player_tokens_.AddPlayer(player);

    return JoinGameResult{token, dog->GetId()};
}

std::vector<Player> Application::GetPlayers(const Token& token) {
    Player* player = player_tokens_.FindPlayer(token);
    if (!player) {
        throw std::invalid_argument("Invalid token");
    }

    const auto& dogs = player->GetSession()->GetDogs();

    std::vector<Player> result;
    for (const auto& dog : dogs) {
        result.emplace_back(
            const_cast<model::GameSession*>(player->GetSession()), const_cast<model::Dog*>(&dog));
    }
    return result;
}

bool Application::SetPlayerAction(const Token& token, std::optional<geom::Direction> dir) {
    Player* player = player_tokens_.FindPlayer(token);
    if (!player) {
        return false;
    }

    model::Dog& dog = player->GetDog();
    const model::Map* map = player->GetSession()->GetMap();
    double speed = map->GetDogSpeed();

    if (!dir.has_value()) {
        dog.SetSpeed({0.0, 0.0});
    } else {
        dog.SetDirection(*dir);
        switch (*dir) {
            case geom::Direction::NORTH:
                dog.SetSpeed({0.0, -speed});
                break;
            case geom::Direction::SOUTH:
                dog.SetSpeed({0.0, speed});
                break;
            case geom::Direction::WEST:
                dog.SetSpeed({-speed, 0.0});
                break;
            case geom::Direction::EAST:
                dog.SetSpeed({speed, 0.0});
                break;
        }
    }
    return true;
}

// Helper: check boundaries
geom::Position CalculateNewPosition(
    const model::Map* map, geom::Position current_pos, geom::Speed speed, double dt) {
    if (speed.ux == 0.0 && speed.uy == 0.0) {
        return current_pos;
    }

    geom::Position next_pos = {current_pos.x + speed.ux * dt, current_pos.y + speed.uy * dt};

    // Helper to calculate bounds for the axis being moved
    auto get_axis_bounds = [&](double move_coord, double stay_coord, const model::Map::Roads& parallel_roads,
                               const model::Map::Roads& perpendicular_roads, bool moving_horizontally) {
        double min_b = -eps, max_b = eps;
        bool first = true;

        auto update = [&](double low, double high) {
            if (first) {
                min_b = low;
                max_b = high;
                first = false;
            } else {
                min_b = std::min(min_b, low);
                max_b = std::max(max_b, high);
            }
        };

        for (const auto& road : parallel_roads) {
            // FIX: Use std::min and std::max separately to ensure we store values, not references
            double r_min = moving_horizontally ? std::min(road.GetStart().x, road.GetEnd().x)
                                               : std::min(road.GetStart().y, road.GetEnd().y);

            double r_max = moving_horizontally ? std::max(road.GetStart().x, road.GetEnd().x)
                                               : std::max(road.GetStart().y, road.GetEnd().y);

            double valid_min = r_min - model::Road::HALF_WIDTH;
            double valid_max = r_max + model::Road::HALF_WIDTH;

            if (move_coord >= valid_min && move_coord <= valid_max) {
                update(valid_min, valid_max);
            }
        }

        // Check perpendicular roads
        for (const auto& road : perpendicular_roads) {
            // FIX: Same logic here
            double r_min = moving_horizontally ? std::min(road.GetStart().y, road.GetEnd().y)
                                               : std::min(road.GetStart().x, road.GetEnd().x);

            double r_max = moving_horizontally ? std::max(road.GetStart().y, road.GetEnd().y)
                                               : std::max(road.GetStart().x, road.GetEnd().x);

            double valid_min = r_min - model::Road::HALF_WIDTH;
            double valid_max = r_max + model::Road::HALF_WIDTH;

            if (stay_coord >= valid_min && stay_coord <= valid_max) {
                double cross_pos = moving_horizontally ? road.GetStart().x : road.GetStart().y;
                update(cross_pos - model::Road::HALF_WIDTH, cross_pos + model::Road::HALF_WIDTH);
            }
        }
        return std::make_pair(min_b, max_b);
    };

    // Determine which axis we are moving on and call the helper
    if (speed.ux != 0) {
        auto [min_x, max_x] = get_axis_bounds(current_pos.x, current_pos.y,
            map->GetRoadsByY(static_cast<int>(std::round(current_pos.y))),
            map->GetRoadsByX(static_cast<int>(std::round(current_pos.x))), true);
        next_pos.x = std::clamp(next_pos.x, min_x, max_x);
    } else {
        auto [min_y, max_y] = get_axis_bounds(current_pos.y, current_pos.x,
            map->GetRoadsByX(static_cast<int>(std::round(current_pos.x))),
            map->GetRoadsByY(static_cast<int>(std::round(current_pos.y))), false);
        next_pos.y = std::clamp(next_pos.y, min_y, max_y);
    }

    return next_pos;
}
void Application::UpdateDog(Player& player, double dt) {
    auto& dog = player.GetDog();
    auto speed = dog.GetSpeed();
    if (speed.ux == 0.0 && speed.uy == 0.0) {
        return;
    }
    auto pos = dog.GetPosition();
    const auto* map = player.GetSession()->GetMap();
    auto new_pos = CalculateNewPosition(map, pos, speed, dt);
    // Calculate expected distance vs actual distance to detect collisions
    double expected_dx = speed.ux * dt;
    double actual_dx = new_pos.x - pos.x;
    double expected_dy = speed.uy * dt;
    double actual_dy = new_pos.y - pos.y;
    // If the actual move is smaller than expected (blockage), reset velocity
    // We use a small epsilon for float comparison errors
    if (std::abs(actual_dx - expected_dx) > eps) {
        speed.ux = 0.0;
    }
    if (std::abs(actual_dy - expected_dy) > eps) {
        speed.uy = 0.0;
    }
    dog.SetPosition(new_pos);
    dog.SetSpeed(speed);
}

class GameItemGatherer : public collision_detector::ItemGathererProvider {
public:
    GameItemGatherer(const std::vector<LootInMap>& loots, const std::vector<model::Office>& offices,
        const std::vector<std::pair<model::Dog*, geom::Position>>& moving_dogs)
        : loots_(loots), offices_(offices), moving_dogs_(moving_dogs) {}

    size_t ItemsCount() const override { return loots_.size() + offices_.size(); }

    collision_detector::Item GetItem(size_t idx) const override {
        // Case 1: Loot (Already double/Position)
        if (idx < loots_.size()) {
            return {.position = loots_[idx].pos, .width = 0.0};
        }

        // Case 2: Office (Int/Point2D -> Needs conversion)
        const auto& office = offices_[idx - loots_.size()];
        const auto& pos = office.GetPosition();

        return {.position = {static_cast<double>(pos.x), static_cast<double>(pos.y)}, .width = ITEM_WIDTH};
    }

    size_t GatherersCount() const override { return moving_dogs_.size(); }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        const auto& [dog, start_pos] = moving_dogs_[idx];
        return collision_detector::Gatherer{start_pos, dog->GetPosition(), PLAYER_WIDTH};
    }

private:
    const std::vector<LootInMap>& loots_;
    const std::vector<model::Office>& offices_;
    const std::vector<std::pair<model::Dog*, geom::Position>>& moving_dogs_;
};

void Application::ProcessCollisions(
    const std::string& map_id, DogMoves& dogs_moves, std::vector<LootInMap>& map_loots) {
    const auto* map = game_.FindMap(model::Map::Id{map_id});
    const auto& offices = map->GetOffices();

    GameItemGatherer provider(map_loots, offices, dogs_moves);
    auto events = collision_detector::FindGatherEvents(provider);

    std::vector<size_t> loots_to_remove;

    for (const auto& event : events) {
        auto& [dog, _] = dogs_moves[event.gatherer_id];

        // --- Loot interaction ---
        if (event.item_id < map_loots.size()) {
            if (std::find(loots_to_remove.begin(), loots_to_remove.end(), event.item_id) !=
                loots_to_remove.end()) {
                continue;
            }

            if (dog->GetBag().size() < map->GetBagCapacity()) {
                const auto& loot = map_loots[event.item_id];
                dog->AddToBag({.id = static_cast<int>(event.item_id), .type = static_cast<int>(loot.type)});
                loots_to_remove.push_back(event.item_id);
            }
        }
        // --- Office interaction ---
        else {
            if (!dog->GetBag().empty()) {
                int total_points = 0;
                for (const auto& item : dog->GetBag()) {
                    total_points += extra_data_.GetLootValue(map_id, item.type);
                }
                dog->AddScore(total_points);
                dog->ClearBag();
            }
        }
    }

    // Remove collected loot
    std::sort(loots_to_remove.rbegin(), loots_to_remove.rend());
    for (auto idx : loots_to_remove) {
        map_loots.erase(map_loots.begin() + idx);
    }
}

void Application::MakeTick(std::uint64_t timeDelta) {
    const double dt = timeDelta / 1000.0;

    // 1. Move dogs & collect movements per map
    std::unordered_map<std::string, DogMoves> map_moves;

    for (auto& [_, player] : player_tokens_) {
        auto& dog = player.GetDog();
        auto old_pos = dog.GetPosition();

        UpdateDog(player, dt);

        map_moves[*player.GetSession()->GetMap()->GetId()].emplace_back(&dog, old_pos);
    }

    // 2. Process collisions per map
    for (auto& [map_id, dogs_moves] : map_moves) {
        ProcessCollisions(map_id, dogs_moves, loots_[map_id]);
    }

    // 3. Generate new loot
    GenerateLoot(std::chrono::milliseconds{timeDelta});
    if (listener_ != nullptr) {
        listener_->OnTick(std::chrono::milliseconds{timeDelta});
    }
}
std::string Application::GetMapValue(const std::string& name) const {
    return extra_data_.GetMapValue(name);
}

void Application::GenerateLoot(std::chrono::milliseconds timeDelta) {
    for (auto const& map : game_.GetMaps()) {
        const auto& id = map.GetId();
        auto numberInMap = extra_data_.GetNumberLootforMap(*id);
        if (!numberInMap.has_value())
            continue;
        auto game_session = game_.FindSession(id);
        if (game_session == nullptr)
            continue;
        auto it = loots_.find(*id);
        if (it == loots_.end())
            continue;
        auto n = loot_gen_.Generate(timeDelta, it->second.size(), game_session->GetNumberDogs());
        std::uniform_int_distribution<size_t> dist(0, *numberInMap - 1);
        for ([[maybe_unused]] auto i : std::views::iota(0u, n)) {
            loots_.at(*id).emplace_back(dist(game_session->GetRandomGen()),
                map.GetRandomPositionOnRoad(game_session->GetRandomGen()));
        }
    }
}

std::vector<LootInMap> Application::GetLootInMap(const std::string& name) const {
    // Attempt to find the loot list for the given map name
    if (auto it = loots_.find(name); it != loots_.end()) {
        return it->second;
    }
    return {};
}

}  // namespace app