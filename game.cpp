#include "game.h"
#include <random>

//////////////////////////////////////////////////////////////////////////////

namespace {

//////////////////////////////////////////////////////////////////////////////

constexpr size_t kMapSize = 31;
constexpr size_t kFOVRadius = 15;
constexpr size_t kVisionRadius = 3;

constexpr size_t kMoveTimer = 960;
constexpr size_t kTurnTimer = 120;

constexpr int32_t kTrainerHP = 8;
constexpr double kTrainerSpeed = 1.0 / 10;

constexpr Point kSteps[] = {
  {-1,  0}, {0,  1}, { 0, -1}, {1, 0},
  {-1, -1}, {1, -1}, {-1,  1}, {1, 1},
};

//////////////////////////////////////////////////////////////////////////////

std::uniform_int_distribution<> die(size_t n) {
  return std::uniform_int_distribution<>{0, static_cast<int>(n - 1)};
}

void charge(Entity& entity) {
  auto const charge = static_cast<int>(round(kTurnTimer * entity.speed));
  if (entity.move_timer > 0) entity.move_timer -= charge;
  if (entity.turn_timer > 0) entity.turn_timer -= charge;
}

//bool moveReady(const Entity& entity) { return entity.move_timer <= 0; }

bool turnReady(const Entity& entity) { return entity.turn_timer <= 0; }

void wait(Entity& entity, double moves, double turns) {
  entity.move_timer += static_cast<int>(round(kMoveTimer * moves));
  entity.turn_timer += static_cast<int>(round(kTurnTimer * turns));
}

//////////////////////////////////////////////////////////////////////////////

struct Result { bool success; int moves; int turns; };
constexpr Result kSuccess { .success = true,  .moves = 0, .turns = 1 };
constexpr Result kFailure { .success = false, .moves = 0, .turns = 1 };

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

bool player(const Entity& entity) {
  return entity.match(
    [](const Pokemon&) { return false; },
    [](const Trainer& trainer) { return trainer.player; }
  );
}

Action plan(const Entity& entity, MaybeAction& input, RNG& rng) {
  if (!player(entity)) {
    return MoveAction{kSteps[die(std::size(kSteps))(rng)]};
  }
  //if (!entity.player) return IdleAction{};
  if (!input) return WaitForInputAction{};
  auto result = std::move(*input);
  input.reset();
  return result;
}

Result act(Board& board, Entity& entity, const Action& action) {
  return std::visit(overloaded{
    [&](const IdleAction&) { return kSuccess; },
    [&](const MoveAction& m) {
      auto const pos = entity.pos + m.step;
      if (pos == entity.pos) return kSuccess;
      if (board.getStatus(pos) != Status::Free) return kFailure;
      board.moveEntity(entity, pos);
      return kSuccess;
    },
    [&](const WaitForInputAction&) { return kFailure; },
  }, action);
}

//////////////////////////////////////////////////////////////////////////////

void initBoard(Board& board, RNG& rng) {
  board.clearAllTiles();
  auto const size = board.getSize();
  auto d100 = die(100);

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
        if (d100(rng) < 45) result.set({x, y}, true);
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

//////////////////////////////////////////////////////////////////////////////

} // namespace

//////////////////////////////////////////////////////////////////////////////

Board::Board(Point size) : m_fov(kFOVRadius), m_map(size, tileType('#')) {}

Point Board::getSize() const { return m_map.size(); }

Status Board::getStatus(Point p) const {
  if (getTile(p).flags & FlagBlocked) return Status::Blocked;
  if (m_entityAtPos.contains(p)) return Status::Occupied;
  return Status::Free;
}

const Tile& Board::getTile(Point p) const { return *m_map.get(p); }

Entity& Board::getActiveEntity() {
  assert(m_entityIndex < m_entities.size());
  return *m_entities[m_entityIndex];
}

Entity* Board::getEntity(Point p) {
  auto const it = m_entityAtPos.find(p);
  return it != m_entityAtPos.end() ? it->second.get() : nullptr;
}

const std::vector<Entity*>& Board::getEntities() const { return m_entities; }

void Board::clearAllTiles() { m_map.fill(tileType('.')); }

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

void Board::removeEntity(Entity& entity) {
  auto it = m_entityAtPos.find(entity.pos);
  assert(it != m_entityAtPos.end());
  assert(it->second.get() == &entity);
  m_entityAtPos.erase(it);
  m_vision.erase(&entity);
}

void Board::advanceEntity() {
  charge(getActiveEntity());
  m_entityIndex += 1;
  if (m_entityIndex >= m_entities.size()) m_entityIndex = 0;
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
      case '.': return Point{ 0,  0};
    }
    return std::nullopt;
  }();

  if (dir) state.input = MoveAction{*dir};
}

void updateState(State& state, std::deque<Input>& inputs) {
  auto& board = state.board;
  auto& player = *state.player;

  if (!player.removed && &player == &board.getActiveEntity()) {
    while (!inputs.empty() && !state.input) {
      processInput(state, inputs.front());
      inputs.pop_front();
    }
  }

  while (!player.removed) {
    auto& entity = board.getActiveEntity();
    if (!turnReady(entity)) {
      board.advanceEntity();
      continue;
    }
    auto const action = plan(entity, state.input, state.rng);
    auto const result = act(board, entity, action);
    if (!result.success && &entity == &player) break;
    wait(entity, result.moves, result.turns);
  }
}

void update(State& state, std::deque<Input>& inputs) {
  updateState(state, inputs);
  //updatePlayerKnowledge(state);
}

} // namespace

State::State() : board({kMapSize, kMapSize}) {
  auto const size = board.getSize();
  auto const start = Point{size.x / 2, size.y / 2};
  while (true) {
    rng = RNG(epochTimeNanos());
    initBoard(board, rng);
    if (board.getStatus(start) == Status::Free) break;
  }

  player = new Trainer("", start, true, kTrainerHP, kTrainerSpeed);
  board.addEntity(OwnedEntity(player));

  auto dx = die(board.getSize().x);
  auto dy = die(board.getSize().y);
  for (auto i = 0; i < 5; i++) {
    auto const pos = [&]() -> std::optional<Point> {
      for (auto j = 0; j < 100; j++) {
        auto const result = Point{dx(rng), dy(rng)};
        if (board.getStatus(result) == Status::Free) return result;
      }
      return std::nullopt;
    }();
    if (pos) board.addEntity(OwnedEntity(new Pokemon("Pidgey", *pos)));
  }
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
