#include "app.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace app {

using namespace std::literals;

Token PlayerTokens::GenerateToken() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Apply setw for the first number
    ss << std::setw(16) << generator1_();

    // Apply setw for the second number
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
    // 1. Find the map
    const auto* map = game_.FindMap(model::Map::Id{authReq.map});
    if (!map) {
        return std::nullopt;
    }

    // 2. Get or create a session for the map
    auto* session = game_.FindSession(model::Map::Id{authReq.map});
    if (!session) {
        return std::nullopt;
    }

    // 3. Create a dog in the session
    auto* dog = session->AddDog(authReq.playerName);

    // 4. Create a player and bind to token
    Player player{session, dog};
    Token token = player_tokens_.AddPlayer(player);

    return JoinGameResult{token, dog->GetId()};
}

std::vector<Player> Application::GetPlayers(const Token& token) {
    Player* player = player_tokens_.FindPlayer(token);
    if (!player) {
        throw std::invalid_argument("Invalid token");
    }

    // Get all dogs in the session of the requesting player
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
        return false;  // Игрок не найден
    }

    model::Dog& dog = player->GetDog();
    const model::Map* map = player->GetSession()->GetMap();
    double speed = map->GetDogSpeed();

    if (!dir.has_value()) {
        // Остановка
        dog.SetSpeed({0.0, 0.0});
    } else {
        // Установка направления и расчет вектора скорости
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

}  // namespace app