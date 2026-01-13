#include "app.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

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
        result.emplace_back(const_cast<model::GameSession*>(player->GetSession()), const_cast<model::Dog*>(&dog));
    }
    return result;
}

bool Application::SetPlayerAction(const Token& token, std::optional<model::Direction> dir) {
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
            case model::Direction::NORTH:
                dog.SetSpeed({0.0, -speed});
                break;
            case model::Direction::SOUTH:
                dog.SetSpeed({0.0, speed});
                break;
            case model::Direction::WEST:
                dog.SetSpeed({-speed, 0.0});
                break;
            case model::Direction::EAST:
                dog.SetSpeed({speed, 0.0});
                break;
        }
    }
    return true;
}

double ClampOnAxis(
    double next_coord,
    double curr_coord,
    double curr_perp,
    const std::vector<model::Road>& parallel_roads,
    const std::vector<model::Road>& crossing_roads,
    model::Coord model::Point::*main_axis,
    model::Coord model::Point::*perp_axis
) {
    std::optional<double> min_bound;
    std::optional<double> max_bound;

    auto update_bounds = [&](double low, double high) {
        if (!min_bound) {
            min_bound = low;
            max_bound = high;
        } else {
            min_bound = std::max(*min_bound, low);
            max_bound = std::min(*max_bound, high);
        }
    };

    // Roads along movement
    for (const auto& road : parallel_roads) {
        double a = road.GetStart().*main_axis;
        double b = road.GetEnd().*main_axis;

        double r_min = std::min(a, b) - model::Road::HALF_WIDTH;
        double r_max = std::max(a, b) + model::Road::HALF_WIDTH;

        if (curr_coord >= r_min && curr_coord <= r_max) {
            update_bounds(r_min, r_max);
        }
    }

    // Crossing roads
    for (const auto& road : crossing_roads) {
        double p0 = road.GetStart().*perp_axis;
        double p1 = road.GetEnd().*perp_axis;

        double r_min = std::min(p0, p1) - model::Road::HALF_WIDTH;
        double r_max = std::max(p0, p1) + model::Road::HALF_WIDTH;

        if (curr_perp >= r_min && curr_perp <= r_max) {
            double fixed = road.GetStart().*main_axis;
            update_bounds(
                fixed - model::Road::HALF_WIDTH,
                fixed + model::Road::HALF_WIDTH
            );
        }
    }

    if (!min_bound) {
        return next_coord;
    }

    return std::clamp(next_coord, *min_bound, *max_bound);
}

model::Position CalculateNewPosition(
    const model::Map* map, model::Position current_pos, model::Speed speed, double dt) {
    model::Position next_pos;
    next_pos.x = current_pos.x + speed.ux * dt;
    next_pos.y = current_pos.y + speed.uy * dt;

    // Optimization: If not moving, don't calculate bounds
    if (speed.ux == 0.0 && speed.uy == 0.0) {
        return current_pos;
    }

    int curr_x_idx = static_cast<int>(std::round(current_pos.x));
    int curr_y_idx = static_cast<int>(std::round(current_pos.y));

    if (speed.ux != 0) {
        next_pos.x = ClampOnAxis(next_pos.x, current_pos.x, current_pos.y, map->GetRoadsByY(curr_y_idx),
            map->GetRoadsByX(curr_x_idx), &model::Point::x, &model::Point::y);
        next_pos.y = current_pos.y;
    } else {
        next_pos.y = ClampOnAxis(next_pos.y, current_pos.y, current_pos.x, map->GetRoadsByX(curr_x_idx),
            map->GetRoadsByY(curr_y_idx), &model::Point::y, &model::Point::x);
        next_pos.x = current_pos.x;
    }

    return next_pos;
}

void Application::MakeTick(std::uint64_t timeDelta) {
    double dt = timeDelta / 1000.0;

    for (auto& [token, player] : player_tokens_) {
        auto& dog = player.GetDog();
        auto speed = dog.GetSpeed();
        if (speed.ux == 0.0 && speed.uy == 0.0) {
            continue;
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
}

}  // namespace app