#include "serialization.h"

namespace serialization {

[[nodiscard]] model::Dog DogRepr::Restore() const {
    model::Dog dog{name_, id_, pos_, bag_capacity_};
    dog.SetSpeed(speed_);
    dog.SetDirection(direction_);
    dog.AddScore(score_);
    for (const auto& item : bag_content_) {
        if (!dog.AddToBag(item)) {
            throw std::runtime_error("Failed to put bag content");
        }
    }
    return dog;
}

ApplicationRepr::ApplicationRepr(const app::Application& app) {
    // 1. Save all Dogs (grouped by Map)
    for (const auto& [token, player] : app.player_tokens_) {
        // Save the Player Token mapping
        std::string map_id = *player.GetSession()->GetMap()->GetId();
        auto dog_id = player.GetDog()->GetId();
        player_reprs_[token] = {map_id, dog_id};

        dog_reprs_[map_id].push_back(DogRepr(*player.GetDog()));
    }

    // 2. Save Loot
    for (const auto& [map_id, loots] : app.loots_) {
        for (const auto& loot : loots) {
            loot_reprs_[map_id].emplace_back(loot);
        }
    }
}

void ApplicationRepr::Restore(app::Application& app) const {
    // 1. Restore Game Sessions and Dogs
    for (const auto& [map_id_str, dogs] : dog_reprs_) {
        model::Map::Id map_id{map_id_str};

        model::GameSession* session = app.game_.FindSession(map_id);
        if (!session)
            continue;

        for (const auto& dog_repr : dogs) {
            model::Dog restored_dog = dog_repr.Restore();
            session->AddDog(std::move(restored_dog));
        }
    }

    // 2. Restore Player Tokens
    for (const auto& [token, pair] : player_reprs_) {
        std::string map_id_str = pair.first;
        int dog_id = pair.second;

        model::Map::Id map_id{map_id_str};
        model::GameSession* session = app.game_.FindSession(map_id);

        model::Dog* found_dog = nullptr;
        for (auto& dog : session->GetDogs()) {
            if (dog.GetId() == dog_id) {
                found_dog = &dog;
                break;
            }
        }

        if (found_dog) {
            app::Player player{session, found_dog};
            app.player_tokens_.AddTokenUnsafe(token, player);
        }
    }

    // 3. Restore Loot
    for (const auto& [map_id, loots] : loot_reprs_) {
        for (const auto& loot_repr : loots) {
            app.loots_[map_id].push_back(loot_repr.Restore());
        }
    }
}

}  // namespace serialization