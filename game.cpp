#include "game.h"

//////////////////////////////////////////////////////////////////////////////

namespace {

//////////////////////////////////////////////////////////////////////////////

constexpr size_t kMapSize = 31;
constexpr size_t kFOVRadius = 15;
constexpr size_t kVisionRadius = 3;

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

Board::Board(Point size) : m_fov(kFOVRadius), m_map(size, tileType('#')) {
  m_map.fill(tileType('.'));
}

Point Board::getSize() const { return m_map.size(); }

Status Board::getStatus(Point p) const {
  if (getTile(p).flags & FlagBlocked) return Status::Blocked;
  if (m_entityAtPos.contains(p)) return Status::Occupied;
  return Status::Free;
}

const Tile& Board::getTile(Point p) const { return *m_map.get(p); }

const Entity* Board::getEntity(Point p) const {
  auto const it = m_entityAtPos.find(p);
  return it != m_entityAtPos.end() ? it->second.get() : nullptr;
}

const std::vector<Entity*>& Board::getEntities() const {
  return m_entities;
}

void Board::setTile(Point p, const Tile* tile) {
  if (!m_map.contains(p)) return;
  auto const prev = m_map.get(p);
  m_map.set(p, tile);

  auto const mask = (FlagBlocked | FlagObscure);
  auto const dirty = (prev->flags & mask) != (tile->flags & mask);
  if (dirty) for (auto const& entity : m_entities) dirtyVision(*entity, &p);
}

void Board::addEntity(OwnedEntity entity) {
  m_entities.emplace_back(entity.get());
  auto& entry = m_entityAtPos[entity->pos];
  assert(entry == nullptr);
  entry = std::move(entity);
}

void Board::moveEntity(Entity& entity, Point to) {
  auto it = m_entityAtPos.find(entity.pos);
  assert(it != m_entityAtPos.end());
  assert(it->second.get() == &entity);
  OwnedEntity source = std::move(it->second);
  m_entityAtPos.erase(it);

  OwnedEntity& target = m_entityAtPos[to];
  assert(target == nullptr);
  target = std::move(source);
  target->pos = to;
  dirtyVision(entity, nullptr);
}

bool Board::canSee(const Entity& entity, Point point) const {
  return canSee(getVision(entity), point);
}

bool Board::canSee(const Vision& vision, Point point) const {
  return visibilityAt(vision, point) >= 0;
}

int32_t Board::visibilityAt(const Entity& entity, Point point) const {
  return visibilityAt(getVision(entity), point);
}

int32_t Board::visibilityAt(const Vision& vision, Point point) const {
  return vision.visibility.get(point + vision.offset);
}

const Vision& Board::getVision(const Entity& entity) const {
  auto const radius = kFOVRadius;
  auto& result = m_vision[&entity];
  if (result == nullptr) {
    result.reset(new Vision{});
    const size_t side = 2 * radius + 1;
    result->visibility = Matrix({side, side}, -1);
  }

  if (result->dirty) {
    auto const pos = entity.pos;
    auto const offset = Point{radius, radius} - pos;
    auto& map = result->visibility;
    map.fill(-1);

    auto const blocked = [&](Point p, const Point* parent) {
      auto const q = p + pos;
      auto const visibility = [&]() -> int32_t {
        // The constants in these expressions come from Point.distanceNethack.
        // They're chosen so that, in a field of tall grass, we can only see
        // cells at a distanceNethack of <= kVisionRadius away.
        if (!parent) return 100 * (kVisionRadius + 1) - 95 - 46 - 25;

        auto const tile = m_map.get(q);
        if (tile->flags & FlagBlocked) return 0;

        auto const obscure = tile->flags & FlagObscure;
        auto const diagonal = p.x != parent->x && p.y != parent->y;
        auto const loss = obscure ? 95 + (diagonal ? 46 : 0) : 0;
        auto const prev = map.get(*parent + pos + offset);
        return std::max(prev - loss, 0);
      }();

      auto const key = q + offset;
      map.set(key, std::max(visibility, map.get(key)));
      return visibility <= 0;
    };

    m_fov.fieldOfVision(blocked);
    result->offset = offset;
    result->dirty = false;
  }
  return *result;
}

void Board::dirtyVision(const Entity& entity, const Point* target) {
  auto const it = m_vision.find(&entity);
  if (it == m_vision.end() || it->second->dirty) return;
  if (target && !canSee(*it->second, *target)) return;
  it->second->dirty = true;
}

//////////////////////////////////////////////////////////////////////////////

namespace {

void processInput(State& state, Input input) {
  auto const ch = static_cast<char>(input);
  auto const dir = [&]() -> std::optional<Point> {
    switch (ch) {
      case 'h': return Point{-1,  0};
      case 'j': return Point{ 0,  1};
      case 'k': return Point{ 0, -1};
      case 'l': return Point{ 1,  0};
      case 'y': return Point{-1, -1};
      case 'u': return Point{ 1, -1};
      case 'b': return Point{-1,  1};
      case 'n': return Point{ 1,  1};
    }
    return std::nullopt;
  }();

  if (!dir) return;
  auto const target = state.player->pos + *dir;
  auto const status = state.board.getStatus(target);
  if (status == Status::Free) state.board.moveEntity(*state.player, target);
}

void update(State& state, std::deque<Input>& inputs) {
  while (!inputs.empty()) {
    processInput(state, inputs.front());
    inputs.pop_front();
  }
}

} // namespace

State::State() : board({kMapSize, kMapSize}) {
  auto const size = board.getSize();
  auto const start = Point{size.x / 2, size.y / 2};
  while (true) {
    initBoard(board);
    if (board.getStatus(start) == Status::Free) break;
  }
  auto trainer = makeTrainer(start);
  player = trainer.get();
  board.addEntity(std::move(trainer));
}

//////////////////////////////////////////////////////////////////////////////

namespace {

void render(const State& state, Matrix<Glyph>& frame) {
  auto const& board = state.board;
  auto const& vision = board.getVision(*state.player);

  auto const offset = Point::origin();
  auto const size = board.getSize();
  auto const map = [&](Point p) { return offset + Point{2 * p.x, p.y}; };

  for (auto y = 0; y < size.y; y++) {
    for (auto x = 0; x < size.x; x++) {
      auto const p = Point{x, y};
      auto const seen = board.canSee(vision, p);
      frame.set(map(p), seen ? board.getTile(p).glyph : Empty());
    }
  }

  for (const auto& entity : board.getEntities()) {
    auto const seen = board.canSee(vision, entity->pos);
    if (seen) frame.set(map(entity->pos), entity->glyph);
  }
}

} // namespace

IO::IO() : frame({2 * kMapSize, kMapSize}, {}) {}

void IO::tick() {
  update(state, inputs);
  render(state, frame);
}

//////////////////////////////////////////////////////////////////////////////
