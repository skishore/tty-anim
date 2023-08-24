#pragma once

#include <string>

#include <absl/container/flat_hash_map.h>

#include "base.h"
#include "geo.h"

//////////////////////////////////////////////////////////////////////////////

enum class Status { Free, Blocked, Occupied };

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

struct Board {
  explicit Board(Point size);

  // Reads

  Point getSize() const;

  Status getStatus(Point p) const;
  const Tile* getTile(Point p) const;
  const Entity* getEntity(Point p) const;

  const std::vector<Entity*>& getEntities() const;

  // Writes

  void setTile(Point p, const Tile* tile);
  void addEntity(OwnedEntity entity);

private:
  Matrix<const Tile*> m_map;
  std::vector<Entity*> m_entities;
  absl::flat_hash_map<Point, OwnedEntity> m_entityAtPos;

  DISALLOW_COPY_AND_ASSIGN(Board);
};

//////////////////////////////////////////////////////////////////////////////

struct State {
  State();

  Board board;
  Entity* player;

  DISALLOW_COPY_AND_ASSIGN(State);
};

//////////////////////////////////////////////////////////////////////////////

struct UI {
  UI();

  State state;
  Matrix<Glyph> frame;

  DISALLOW_COPY_AND_ASSIGN(UI);
};

//////////////////////////////////////////////////////////////////////////////
