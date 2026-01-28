#pragma once
#include <cmath>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "geom.h"
#include "tagged.h"

namespace model {

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };
    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};
    constexpr static double WIDTH = 0.8;
    constexpr static double HALF_WIDTH = WIDTH / 2.0;

    Road(HorizontalTag, geom::Point2D start, geom::Coord end_x) noexcept
        : start_{start}, end_{end_x, start.y} {}
    Road(VerticalTag, geom::Point2D start, geom::Coord end_y) noexcept
        : start_{start}, end_{start.x, end_y} {}
    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }
    geom::Point2D GetStart() const noexcept { return start_; }
    geom::Point2D GetEnd() const noexcept { return end_; }

private:
    geom::Point2D start_;
    geom::Point2D end_;
};

class Building {
public:
    explicit Building(geom::Rectangle bounds) noexcept : bounds_{bounds} {}
    const geom::Rectangle& GetBounds() const noexcept { return bounds_; }

private:
    geom::Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;
    Office(Id id, geom::Point2D position, geom::Offset offset) noexcept
        : id_{std::move(id)}, position_{position}, offset_{offset} {}
    const Id& GetId() const noexcept { return id_; }
    geom::Point2D GetPosition() const noexcept { return position_; }
    geom::Offset GetOffset() const noexcept { return offset_; }

private:
    Id id_;
    geom::Point2D position_;
    geom::Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept : id_(std::move(id)), name_(std::move(name)) {}
    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Offices& GetOffices() const noexcept { return offices_; }

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

    // Fast lookup for roads
    const Roads& GetRoadsByX(geom::Coord x) const;
    const Roads& GetRoadsByY(geom::Coord y) const;

    void SetRandomSpawn(bool randomSpawn) { randomSpawn_ = randomSpawn; };
    bool GetRandomSpawn(void) const { return randomSpawn_; };

    void SetBagCapacity(double bagCapacity) { bagCapacity_ = bagCapacity; };
    void SetDogSpeed(double speed) { dogSpeed_ = speed; }
    double GetBagCapacity() const { return bagCapacity_; }
    double GetDogSpeed() const { return dogSpeed_; }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    Id id_;
    std::string name_;

    Roads roads_;
    // Map coordinate -> List of roads on that line
    std::unordered_map<geom::Coord, Roads> roads_by_x_;
    std::unordered_map<geom::Coord, Roads> roads_by_y_;

    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    bool randomSpawn_;
    double dogSpeed_{-1.0};
    double bagCapacity_{-1.0};
};

struct BagItem {
    int id;
    int type;
};

class Dog {
public:
    Dog(std::string name, int id, geom::Position pos)
        : name_(std::move(name))
        , id_(id)
        , position_(pos)
        , speed_{0.0, 0.0}
        , direction_(geom::Direction::NORTH) {}

    const std::string& GetName() const { return name_; }
    int GetId() const { return id_; }

    geom::Position GetPosition() const { return position_; }
    geom::Speed GetSpeed() const { return speed_; }
    geom::Direction GetDirection() const { return direction_; }
    int GetScore() const { return score_; }
    void SetPosition(geom::Position pos) { position_ = pos; }
    void SetSpeed(geom::Speed speed) { speed_ = speed; }
    void SetDirection(geom::Direction dir) { direction_ = dir; }
    const std::vector<BagItem>& GetBag() const { return bag_; }
    void AddToBag(BagItem item) { bag_.push_back(item); }
    void ClearBag() { bag_.clear(); }
    void AddScore(int points) { score_ += points; }

private:
    std::string name_;
    int id_;

    geom::Position position_;
    geom::Speed speed_;
    geom::Direction direction_;
    std::vector<BagItem> bag_;
    int score_{};
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}

    Dog* AddDog(std::string_view name);

    const Map* GetMap() const { return map_; }
    const std::deque<Dog>& GetDogs() const { return dogs_; }
    // Non-const getter for updating state
    std::deque<Dog>& GetDogs() { return dogs_; }
    std::size_t GetNumberDogs() const { return dogs_.size(); }
    geom::Position GenerateRamdomPosition(void) const;

private:
    const Map* map_;
    std::deque<Dog> dogs_;
    int next_dog_id_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;
    void AddMap(Map map);
    const Maps& GetMaps() const noexcept { return maps_; }
    const Map* FindMap(const Map::Id& id) const noexcept;
    GameSession* FindSession(const Map::Id& id);
    void SetSpeed(double speed) { speed_ = speed; };
    void SetRandomSpawn(bool randomSpawn) { randomSpawn_ = randomSpawn; };
    void SetDefaultBagCapacity(double defaultBagCapacity) { defaultBagCapacity_ = defaultBagCapacity; };

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using MapIdToSession = std::unordered_map<Map::Id, std::shared_ptr<GameSession>, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    MapIdToSession map_id_to_session_;
    double speed_{1.0};
    double defaultBagCapacity_{3.0};
    bool randomSpawn_{};
};

}  // namespace model