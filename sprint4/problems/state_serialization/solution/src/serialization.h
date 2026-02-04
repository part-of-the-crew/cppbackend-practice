#pragma once
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include "app.h"
#include "geom.h"
#include "model.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Size& s, [[maybe_unused]] const unsigned version) {
    ar & s.width;
    ar & s.height;
}

template <typename Archive>
void serialize(Archive& ar, Rectangle& rec, [[maybe_unused]] const unsigned version) {
    ar & rec.position;
    ar & rec.size;
}

template <typename Archive>
void serialize(Archive& ar, Offset& s, [[maybe_unused]] const unsigned version) {
    ar & s.dx;
    ar & s.dy;
}

template <typename Archive>
void serialize(Archive& ar, Position& p, [[maybe_unused]] const unsigned version) {
    ar & p.x;
    ar & p.y;
}

template <typename Archive>
void serialize(Archive& ar, Speed& s, [[maybe_unused]] const unsigned version) {
    ar & s.ux;
    ar & s.uy;
}

template <typename Archive>
void serialize(Archive& ar, Direction& d, [[maybe_unused]] const unsigned version) {
    ar & d;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, BagItem& obj, [[maybe_unused]] const unsigned version) {
    ar&(obj.id);
    ar&(obj.type);
}

}  // namespace model

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : name_(dog.GetName())
        , id_(dog.GetId())
        , pos_(dog.GetPosition())
        , bag_capacity_(dog.GetBagCapacity())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_content_(dog.GetBagContent()) {}

    [[nodiscard]] model::Dog Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & name_;
        ar & id_;
        ar & pos_;
        ar & bag_capacity_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_content_;
    }

private:
    std::string name_;
    size_t id_ = 0;
    geom::Position pos_;
    size_t bag_capacity_ = 0;
    geom::Speed speed_;
    geom::Direction direction_ = geom::Direction::NORTH;
    size_t score_ = 0;
    model::Dog::BagContent bag_content_;
};

class LootRepr {
public:
    LootRepr() = default;
    explicit LootRepr(const app::LootInMap& loot) : type_(loot.type), pos_(loot.pos) {}

    [[nodiscard]] app::LootInMap Restore() const { return {type_, pos_}; }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & type_ & pos_;
    }

private:
    unsigned long type_ = 0;
    geom::Position pos_;
};

class ApplicationRepr {
public:
    ApplicationRepr() = default;

    explicit ApplicationRepr(const app::Application& app);
    void Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & dog_reprs_;
        ar & player_reprs_;
        ar & loot_reprs_;
    }

private:
    // MapID -> List of Dogs (to repopulate GameSessions)
    std::unordered_map<std::string, std::vector<DogRepr>> dog_reprs_;

    // Token -> Pair<MapID, DogID> (to reconnect Players to their Dogs)
    std::unordered_map<std::string, std::pair<std::string, int>> player_reprs_;

    // MapID -> List of Loot
    std::unordered_map<std::string, std::vector<LootRepr>> loot_reprs_;
};

}  // namespace serialization
