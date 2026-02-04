#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/app.h"
#include "../src/model.h"
#include "../src/serialization.h"
#include "geom.h"

using namespace std::literals;

// Helper to create a base game with one map (Static Data)
model::Game CreateTestGame() {
    model::Game game;
    model::Map map{model::Map::Id{"map1"s}, "Test Map"s};
    map.AddRoad({{geom::Direction::HORIZONTAL, {0, 0}, 100}});
    game.AddMap(std::move(map));
    return game;
}

SCENARIO("Application State Serialization Round-trip") {
    GIVEN("An application with players, dogs, and loot") {
        auto game = CreateTestGame();
        app::Application app{std::move(game)};

        // 1. Setup specific state: Join a player
        auto join_result = app.JoinGame({"Player1"s, "map1"s});
        REQUIRE(join_result.has_value());

        auto& dog = join_result->player.GetDog();
        dog.SetPosition({12.5, 42.1});
        dog.SetSpeed({1.0, -1.0});
        dog.AddScore(150);
        dog.PutToBag({model::FoundObject::Id{1}, 0u});

        // 2. Setup specific state: Add some loot to the map
        // (Assuming you have access to loots_ or a way to add it)
        app.GetLootInMap("map1"s).push_back({10u, {5.0, 5.0}});

        std::string serialized_data;

        WHEN("The application state is serialized") {
            std::stringstream strm;
            {
                serialization::ApplicationRepr repr{app};
                boost::archive::text_oarchive oa{strm};
                oa << repr;
            }
            serialized_data = strm.str();

            THEN("It can be deserialized into a new Application instance") {
                std::stringstream input_strm(serialized_data);
                boost::archive::text_iarchive ia{input_strm};

                serialization::ApplicationRepr restored_repr;
                ia >> restored_repr;

                // Create a fresh app with the same static config
                auto fresh_game = CreateTestGame();
                app::Application restored_app{std::move(fresh_game)};

                // Restore the dynamic state
                restored_repr.Restore(restored_app);

                // --- ASSERTIONS ---

                // 1. Verify Player/Token recovery
                auto* restored_player = restored_app.FindPlayer(join_result->token);
                REQUIRE(restored_player != nullptr);
                CHECK(restored_player->GetName() == "Player1"s);

                // 2. Verify Dog attributes
                const auto& restored_dog = restored_player->GetDog();
                CHECK(restored_dog.GetPosition().x == Catch::Approx(12.5));
                CHECK(restored_dog.GetPosition().y == Catch::Approx(42.1));
                CHECK(restored_dog.GetScore() == 150);
                CHECK(restored_dog.GetBagContent().size() == 1);

                // 3. Verify Loot recovery
                const auto& restored_loots = restored_app.GetLootInMap("map1"s);
                REQUIRE(restored_loots.size() == 1);
                CHECK(restored_loots[0].type == 10u);
                CHECK(restored_loots[0].pos.x == Catch::Approx(5.0));
            }
        }
    }
}

SCENARIO("Empty Application Serialization") {
    GIVEN("An application with no players") {
        app::Application app{CreateTestGame()};
        std::stringstream strm;

        WHEN("Serialized and Deserialized") {
            {
                serialization::ApplicationRepr repr{app};
                boost::archive::text_oarchive oa{strm};
                oa << repr;
            }

            serialization::ApplicationRepr restored_repr;
            boost::archive::text_iarchive ia{strm};
            ia >> restored_repr;

            app::Application restored_app{CreateTestGame()};
            restored_repr.Restore(restored_app);

            THEN("The restored app has no players") {
                // Assuming you add a GetPlayerNumber() to Application or PlayerTokens
                // CHECK(restored_app.GetTotalPlayers() == 0);
            }
        }
    }
}