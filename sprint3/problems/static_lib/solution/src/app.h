#pragma once
#include <cstddef>
#include <cstdint>
// #include <iostream>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "extra_data.h"
#include "loot_generator.h"
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

    std::size_t GetPlayerNumber() const;

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

struct LootInMap {
    unsigned long type;
    model::Position pos;
};

class Application {
public:
    explicit Application(model::Game game, extra_data::ExtraData extra_data, loot_gen::LootGenerator loot_gen)
        : game_(std::move(game)), extra_data_(std::move(extra_data)), loot_gen_(std::move(loot_gen)) {
        // Initialize empty loot lists for all maps immediately
        for (const auto& map : game_.GetMaps()) {
            loots_.emplace(*map.GetId(), std::vector<LootInMap>{});
        }
    }

    const model::Game& GetGame() const { return game_; }

    std::optional<JoinGameResult> JoinGame(const AuthRequest& authReq);

    std::vector<Player> GetPlayers(const Token& token);

    const model::Map* FindMap(const model::Map::Id& id) const { return game_.FindMap(id); }

    bool SetPlayerAction(const Token& token, std::optional<model::Direction> dir);

    void MakeTick(std::uint64_t timeDelta);

    std::string GetMapValue(const std::string& name) const;
    std::vector<LootInMap> GetLootInMap(const std::string& name) const;
    void GenerateOneLoot(std::string idMap, model::GameSession* session, unsigned long numberInMap);

private:
    void GenerateLoot(std::chrono::milliseconds timeDelta);

    model::Game game_;
    PlayerTokens player_tokens_;
    extra_data::ExtraData extra_data_;
    std::unordered_map<std::string, std::vector<LootInMap>> loots_;
    loot_gen::LootGenerator loot_gen_;
};

}  // namespace app