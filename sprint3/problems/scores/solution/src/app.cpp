#include "app.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>

#include "collision_detector.h"

namespace app {

constexpr double eps = std::numeric_limits<double>::epsilon();

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

    token_to_player_.emplace(token, std::move(player));
    return token;
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

    auto* dog = session->AddDog(authReq.playerName);

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
    geom::Position next_pos;
    next_pos.x = current_pos.x + speed.ux * dt;
    next_pos.y = current_pos.y + speed.uy * dt;

    if (speed.ux == 0.0 && speed.uy == 0.0) {
        return current_pos;
    }

    int curr_x_idx = static_cast<int>(std::round(current_pos.x));
    int curr_y_idx = static_cast<int>(std::round(current_pos.y));

    // Limits for clamping
    double min_bound = -1e9, max_bound = 1e9;
    bool first_match = true;

    auto update_bounds = [&](double low, double high) {
        if (first_match) {
            min_bound = low;
            max_bound = high;
            first_match = false;
        } else {
            min_bound = std::min(min_bound, low);
            max_bound = std::max(max_bound, high);
        }
    };

    if (speed.ux != 0) {
        // Horizontal movement: Check Roads by Y (main movement roads)
        for (const auto& road : map->GetRoadsByY(curr_y_idx)) {
            double r_min = std::min(road.GetStart().x, road.GetEnd().x);
            double r_max = std::max(road.GetStart().x, road.GetEnd().x);
            double road_half_width = model::Road::HALF_WIDTH;
            double valid_min = r_min - road_half_width;
            double valid_max = r_max + road_half_width;

            if (current_pos.x >= valid_min && current_pos.x <= valid_max) {
                update_bounds(valid_min, valid_max);
            }
        }
        // Horizontal movement: Check Crossing Vertical Roads
        for (const auto& road : map->GetRoadsByX(curr_x_idx)) {
            double r_min = std::min(road.GetStart().y, road.GetEnd().y);
            double r_max = std::max(road.GetStart().y, road.GetEnd().y);
            double road_half_width = model::Road::HALF_WIDTH;
            if (current_pos.y >= r_min - road_half_width && current_pos.y <= r_max + road_half_width) {
                double v_min = road.GetStart().x - road_half_width;
                double v_max = road.GetStart().x + road_half_width;
                update_bounds(v_min, v_max);
            }
        }
        next_pos.x = std::clamp(next_pos.x, min_bound, max_bound);
    } else {
        // Vertical movement
        for (const auto& road : map->GetRoadsByX(curr_x_idx)) {
            double r_min = std::min(road.GetStart().y, road.GetEnd().y);
            double r_max = std::max(road.GetStart().y, road.GetEnd().y);
            double road_half_width = model::Road::HALF_WIDTH;
            double valid_min = r_min - road_half_width;
            double valid_max = r_max + road_half_width;

            if (current_pos.y >= valid_min && current_pos.y <= valid_max) {
                update_bounds(valid_min, valid_max);
            }
        }
        for (const auto& road : map->GetRoadsByY(curr_y_idx)) {
            double r_min = std::min(road.GetStart().x, road.GetEnd().x);
            double r_max = std::max(road.GetStart().x, road.GetEnd().x);
            double road_half_width = model::Road::HALF_WIDTH;
            if (current_pos.x >= r_min - road_half_width && current_pos.x <= r_max + road_half_width) {
                double h_min = road.GetStart().y - road_half_width;
                double h_max = road.GetStart().y + road_half_width;
                update_bounds(h_min, h_max);
            }
        }
        next_pos.y = std::clamp(next_pos.y, min_bound, max_bound);
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

// PASTE HERE, inside namespace app so it can see LootInMap directly
class GameItemGatherer : public collision_detector::ItemGathererProvider {
public:
    // Note: removed 'app::' prefix since we are inside namespace app
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
        const auto& pos = office.GetPosition();  // This is Point2D (int)

        return {.position = {static_cast<double>(pos.x), static_cast<double>(pos.y)}, .width = 0.5};
    }

    size_t GatherersCount() const override { return moving_dogs_.size(); }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        const auto& [dog, start_pos] = moving_dogs_[idx];
        return collision_detector::Gatherer{start_pos, dog->GetPosition(), 0.6};
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
}

std::string Application::GetMapValue(const std::string& name) const {
    return extra_data_.GetMapValue(name);
}

void Application::GenerateOneLoot(std::string idMap, model::GameSession* session, unsigned long numberInMap) {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> dist(0, numberInMap - 1);
    loots_.at(idMap).emplace_back(dist(gen), session->GenerateRamdomPosition());
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

        for ([[maybe_unused]] auto i : std::views::iota(0u, n)) {
            GenerateOneLoot(*id, game_session, *numberInMap);
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