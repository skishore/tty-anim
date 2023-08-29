#pragma once

#include <deque>
#include <random>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "base.h"
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
  static const absl::flat_hash_map<char, Tile> result{
    {'.', {Wide('.'),        FlagNone,    "grass"}},
    {'"', {Wide('"', 0x231), FlagObscure, "tall grass"}},
    {'#', {Wide('#', 0x010), FlagBlocked, "a tree"}},
  };
  return &result.at(ch);
}

//////////////////////////////////////////////////////////////////////////////

struct Entity {
  Glyph glyph;
  Point pos;
};
using OwnedEntity = std::unique_ptr<Entity>;

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
  const Entity* getEntity(Point p) const;

  const std::vector<Entity*>& getEntities() const;

  // Writes

  void clearAllTiles();
  void setTile(Point p, const Tile* tile);
  void addEntity(OwnedEntity entity);
  void moveEntity(Entity& entity, Point to);

  // Cached field-of-vision

  bool canSee(const Entity& entity, Point point) const;
  bool canSee(const Vision& vision, Point point) const;
  int32_t visibilityAt(const Entity& entity, Point point) const;
  int32_t visibilityAt(const Vision& vision, Point point) const;
  const Vision& getVision(const Entity& entity) const;

private:
  void dirtyVision(const Entity& entity, const Point* target);

  const FOV m_fov;
  Matrix<const Tile*> m_map;
  std::vector<Entity*> m_entities;
  absl::flat_hash_map<Point, OwnedEntity> m_entityAtPos;
  mutable absl::flat_hash_map<const Entity*, std::unique_ptr<Vision>> m_vision;

  DISALLOW_COPY_AND_ASSIGN(Board);
};

//////////////////////////////////////////////////////////////////////////////

using RNG = std::mt19937;

struct State {
  State();

  RNG rng;
  Board board;
  Entity* player;

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
