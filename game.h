#pragma once

#include <string>

#include <absl/container/flat_hash_map.h>

#include "base.h"
#include "geo.h"

//////////////////////////////////////////////////////////////////////////////

using TileFlags = uint8_t;
constexpr static TileFlags FlagNone    = 0x0;
constexpr static TileFlags FlagBlocked = 0x1;
constexpr static TileFlags FlagObscure = 0x2;

struct Tile {
  char glyph;
  TileFlags flags;
  std::string description;
};

static const Tile* getTile(char ch) {
  static const absl::flat_hash_map<char, Tile> result{
    {'.', {'.', FlagNone,    "grass"}},
    {'"', {'.', FlagObscure, "tall grass"}},
    {'#', {'.', FlagBlocked, "a tree"}},
  };
  return &result.at(ch);
}

//////////////////////////////////////////////////////////////////////////////

using Entity = char;

struct Board {
  explicit Board(Point size) : m_map(size, getTile('#')) {
    m_map.fill(getTile('.'));
  }

  // Reads

  Point size() const { return m_map.size(); }

private:
  Matrix<const Tile*> m_map;
  absl::flat_hash_map<Point, Entity> m_entities;

  DISALLOW_COPY_AND_ASSIGN(Board);
};

//////////////////////////////////////////////////////////////////////////////

struct State {
  explicit State(Point size) : board(size) {}

  Board board;

  DISALLOW_COPY_AND_ASSIGN(State);
};

//////////////////////////////////////////////////////////////////////////////
