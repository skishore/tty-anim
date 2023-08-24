#include "game.h"

//////////////////////////////////////////////////////////////////////////////

namespace {

//////////////////////////////////////////////////////////////////////////////

constexpr size_t kMapSize = 32;

//////////////////////////////////////////////////////////////////////////////

void initBoard(Board& board) {
  auto const size = board.getSize();
  auto const automata = [&]() -> Matrix<bool> {
    Matrix<bool> result{size, false};
    for (auto x = 0; x < size.x; x++) {
      result.set({x, 0}, true);
      result.set({x, size.y - 1}, true);
    }
    for (auto y = 0; y < size.y; y++) {
      result.set({0, y}, true);
      result.set({size.x - 1, y}, true);
    }

    for (auto y = 0; y < size.y; y++) {
      for (auto x = 0; x < size.x; x++) {
        if (rand() % 100 < 45) result.set({x, y}, true);
      }
    }

    for (auto i = 0; i < 3; i++) {
      Matrix<bool> next = result;

      for (auto y = 1; y < size.y - 1; y++) {
        for (auto x = 1; x < size.x - 1; x++) {
          auto adj1 = 0, adj2 = 0;
          for (auto dy = -2; dy <= 2; dy++) {
            for (auto dx = -2; dx <= 2; dx++) {
              if (dx == 0 && dy == 0) continue;
              if (std::min(std::abs(dx), std::abs(dy)) == 2) continue;
              auto const next = Point{x + dx, y + dy};
              if (!(result.contains(next) && result.get(next))) continue;
              auto const distance = std::max(std::abs(dx), std::abs(dy));
              if (distance <= 1) adj1++;
              if (distance <= 2) adj2++;
            }
          }

          auto const blocked = adj1 >= 5 || (i < 2 && adj2 <= 1);
          next.set({x, y}, blocked);
        }
      }

      result = std::move(next);
    }

    return result;
  };

  auto const walls = automata();
  auto const grass = automata();
  auto const wt = tileType('#');
  auto const gt = tileType('"');
  for (auto y = 0; y < size.y; y++) {
    for (auto x = 0; x < size.x; x++) {
      auto const p = Point{x, y};
      if (walls.get(p)) {
        board.setTile(p, wt);
      } else if (grass.get(p)) {
        board.setTile(p, gt);
      }
    }
  }
}

std::unique_ptr<Entity> makeTrainer(Point pos) {
  return std::make_unique<Entity>(Entity{.glyph = Wide('@'), .pos = pos});
}

//////////////////////////////////////////////////////////////////////////////

} // namespace

//////////////////////////////////////////////////////////////////////////////

Board::Board(Point size) : m_map(size, tileType('#')) {
  m_map.fill(tileType('.'));
}

Point Board::getSize() const { return m_map.size(); }

Status Board::getStatus(Point p) const {
  if (getTile(p)->flags & FlagBlocked) return Status::Blocked;
  if (m_entityAtPos.contains(p)) return Status::Occupied;
  return Status::Free;
}

const Tile* Board::getTile(Point p) const { return m_map.get(p); }

const Entity* Board::getEntity(Point p) const {
  auto const it = m_entityAtPos.find(p);
  return it != m_entityAtPos.end() ? it->second : nullptr;
}

void Board::setTile(Point p, const Tile* tile) { m_map.set(p, tile); }

void Board::addEntity(std::unique_ptr<Entity> entity) {
  auto& entry = m_entityAtPos[entity->pos];
  assert(entry == nullptr);
  entry = entity.get();
  m_entities.emplace_back(entity);
}

//////////////////////////////////////////////////////////////////////////////

State::State() : board({kMapSize, kMapSize}) {
  auto const size = board.getSize();
  auto const start = Point{size.x / 2, size.y / 2};
  while (true) {
    initBoard(board);
    if (board.getStatus(start) == Status::Free) break;
  }
  auto player = makeTrainer(start);
  this->player = player.get();
  board.addEntity(std::move(player));
}

//////////////////////////////////////////////////////////////////////////////

UI::UI() : frame({2 * kMapSize, kMapSize}, {}) {
  auto const size = state.board.getSize();
  for (auto y = 0; y < size.y; y++) {
    for (auto x = 0; x < size.x; x++) {
      frame.set({2 * x, y}, state.board.getTile({x, y})->glyph);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
