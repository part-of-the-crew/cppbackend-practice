#pragma once
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "model.h"

namespace app {

using Token = std::string;

class Player {
public:
    Player(model::GameSession* session, model::Dog* dog) : session_(session), dog_(dog) {}

    const std::string& GetName() const { return dog_->GetName(); }
    const model::GameSession* GetSession() const { return session_; }
    int GetId() const { return dog_->GetId(); }
    const model::Dog* GetDog() const { return dog_; }
    model::Dog& GetDog() { return *dog_; }

private:
    model::GameSession* session_;
    model::Dog* dog_;
};

class PlayerTokens {
public:
    // Generate a new token and store the player
    Token AddPlayer(Player player);

    // Find a player by token
    Player* FindPlayer(Token token);

    // Allow iterating over players
    auto begin() { return token_to_player_.begin(); }
    auto end() { return token_to_player_.end(); }

private:
    std::unordered_map<Token, Player> token_to_player_;

    std::random_device random_device_;
    std::mt19937_64 generator1_{random_device_()};
    std::mt19937_64 generator2_{random_device_()};

    Token GenerateToken();
};

struct JoinGameResult {
    Token token;
    int playerId;
};

struct AuthRequest {
    std::string playerName;
    std::string map;
};

class Application {
public:
    explicit Application(model::Game game) : game_(std::move(game)) {}

    const model::Game& GetGame() const { return game_; }

    std::optional<JoinGameResult> JoinGame(const AuthRequest& authReq);

    std::vector<Player> GetPlayers(const Token& token);

    const model::Map* FindMap(const model::Map::Id& id) const { return game_.FindMap(id); }

    bool SetPlayerAction(const Token& token, std::optional<model::Direction> dir);

    void MakeTick(std::uint64_t timeDelta);

private:
    model::Game game_;
    PlayerTokens player_tokens_;
};

}  // namespace app