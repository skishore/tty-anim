#pragma once

#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "base.h"
#include "entity.h"
#include "geo.h"

//////////////////////////////////////////////////////////////////////////////

enum struct Status { Free, Blocked, Occupied };

using TileFlags = uint8_t;
constexpr static TileFlags FlagNone    = 0x0;
constexpr static TileFlags FlagBlocked = 0x1;
constexpr static TileFlags FlagObscure = 0x2;

struct Tile {
  Glyph glyph;
  TileFlags flags;
  std::string description;
};

inline const Tile* tileType(char ch) {
  static const HashMap<char, Tile> result{
    {'.', {Wide('.'),        FlagNone,    "grass"}},
    {'"', {Wide('"', 0x231), FlagObscure, "tall grass"}},
    {'#', {Wide('#', 0x010), FlagBlocked, "a tree"}},
  };
  return &result.at(ch);
}

//////////////////////////////////////////////////////////////////////////////

struct Vision {
  Point offset;
  bool dirty = true;
  Matrix<int32_t> visibility;

  DISALLOW_COPY_AND_ASSIGN(Vision);
};

struct Board {
  explicit Board(Point size);

  // Reads

  Point getSize() const;

  Status getStatus(Point p) const;
  const Tile& getTile(Point p) const;

  Entity& getActiveEntity();
  Entity* getEntity(Point p);
  const std::vector<Entity*>& getEntities() const;

  // Writes

  void clearAllTiles();
  void setTile(Point p, const Tile* tile);
  void addEntity(OwnedEntity entity);
  void moveEntity(Entity& entity, Point to);
  void removeEntity(Entity& entity);
  void advanceEntity();

  // Cached field-of-vision

  bool canSee(const Entity& entity, Point point) const;
  bool canSee(const Vision& vision, Point point) const;
  int32_t visibilityAt(const Entity& entity, Point point) const;
  int32_t visibilityAt(const Vision& vision, Point point) const;
  const Vision& getVision(const Entity& entity) const;

private:
  void dirtyVision(const Entity& entity, const Point* target);

  const FOV m_fov;
  size_t m_entityIndex = {};
  Matrix<const Tile*> m_map;
  std::vector<Entity*> m_entities;
  HashMap<Point, OwnedEntity> m_entityAtPos;
  mutable HashMap<const Entity*, std::unique_ptr<Vision>> m_vision;

  DISALLOW_COPY_AND_ASSIGN(Board);
};

//////////////////////////////////////////////////////////////////////////////

struct IdleAction {};
struct MoveAction { Point step; };
struct WaitForInputAction {};

using Action = std::variant<IdleAction, MoveAction, WaitForInputAction>;
using MaybeAction = std::optional<Action>;

//////////////////////////////////////////////////////////////////////////////

using RNG = std::mt19937;

struct State {
  State();

  RNG rng;
  Board board;
  Entity* player;
  MaybeAction input;

  DISALLOW_COPY_AND_ASSIGN(State);
};

//////////////////////////////////////////////////////////////////////////////

struct IO {
  IO();
  void tick();

  State state;
  Matrix<Glyph> frame;
  std::deque<Input> inputs;

  DISALLOW_COPY_AND_ASSIGN(IO);
};

//////////////////////////////////////////////////////////////////////////////
