// model.h

#pragma once
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};
struct Size {
    Dimension width, height;
};
struct Rectangle {
    Point position;
    Size size;
};
struct Offset {
    Dimension dx, dy;
};

// --- НОВЫЕ СТРУКТУРЫ ---

// Координаты в вещественных числах
struct Position {
    double x = 0.0;
    double y = 0.0;
};

// Скорость в единицах карты за секунду
struct Speed {
    double ux = 0.0;
    double uy = 0.0;
};

// Направление
enum class Direction { NORTH, SOUTH, WEST, EAST };

// -----------------------

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
    Road(HorizontalTag, Point start, Coord end_x) noexcept : start_{start}, end_{end_x, start.y} {}
    Road(VerticalTag, Point start, Coord end_y) noexcept : start_{start}, end_{start.x, end_y} {}
    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }
    Point GetStart() const noexcept { return start_; }
    Point GetEnd() const noexcept { return end_; }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept : bounds_{bounds} {}
    const Rectangle& GetBounds() const noexcept { return bounds_; }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;
    Office(Id id, Point position, Offset offset) noexcept : id_{std::move(id)}, position_{position}, offset_{offset} {}
    const Id& GetId() const noexcept { return id_; }
    Point GetPosition() const noexcept { return position_; }
    Offset GetOffset() const noexcept { return offset_; }

private:
    Id id_;
    Point position_;
    Offset offset_;
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
    void AddRoad(const Road& road) { roads_.emplace_back(road); }
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    // Конструктор теперь принимает начальную позицию
    Dog(std::string name, int id, Position pos)
        : name_(std::move(name))
        , id_(id)
        , position_(pos)
        , speed_{0.0, 0.0}              // Скорость по умолчанию 0
        , direction_(Direction::NORTH)  // Направление по умолчанию Север
    {}

    const std::string& GetName() const { return name_; }
    int GetId() const { return id_; }

    // Геттеры для новых полей
    Position GetPosition() const { return position_; }
    Speed GetSpeed() const { return speed_; }
    Direction GetDirection() const { return direction_; }

    // Сеттеры (понадобятся для механики движения)
    void SetPosition(Position pos) { position_ = pos; }
    void SetSpeed(Speed speed) { speed_ = speed; }
    void SetDirection(Direction dir) { direction_ = dir; }

private:
    std::string name_;
    int id_;

    Position position_;
    Speed speed_;
    Direction direction_;
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}

    // AddDog внутри будет вычислять случайные координаты
    Dog* AddDog(std::string_view name);

    const Map* GetMap() const { return map_; }
    const std::deque<Dog>& GetDogs() const { return dogs_; }

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

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using MapIdToSession = std::unordered_map<Map::Id, std::shared_ptr<GameSession>, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    MapIdToSession map_id_to_session_;
};

}  // namespace model