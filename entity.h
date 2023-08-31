#pragma once

#include <array>
#include <string>

#include "base.h"
#include "geo.h"

//////////////////////////////////////////////////////////////////////////////

struct Trainer;

struct Attack {
  std::string name;
  int32_t range;
  int32_t damage;
};

struct PokemonSpeciesData {
  std::string name;
  Glyph glyph;
  int32_t hp;
  double speed;

  DISALLOW_COPY_AND_ASSIGN(PokemonSpeciesData);
};

struct PokemonIndividualData {
  std::array<const Attack*, 4> attacks;
  const PokemonSpeciesData& species;
  std::weak_ptr<Trainer> trainer;

  DISALLOW_COPY_AND_ASSIGN(PokemonIndividualData);
};

//////////////////////////////////////////////////////////////////////////////

struct Entity {
  enum class Type { Pokemon, Trainer };

  Entity(Type type, Point pos, Glyph glyph, int32_t hp, double speed);
  virtual ~Entity() = default;

  template <typename P, typename T> auto match(P p, T t);
  template <typename P, typename T> auto match(P p, T t) const;

  Type type;
  bool removed;
  Point pos;
  Glyph glyph;
  int32_t move_timer;
  int32_t turn_timer;
  int32_t cur_hp;
  int32_t max_hp;
  double speed;

  DISALLOW_COPY_AND_ASSIGN(Entity);
};

struct Pokemon : public Entity {
  PokemonIndividualData* self;
};

struct Trainer : public Entity {
  Trainer(std::string name, Point pos, bool player, int32_t hp, double speed);

  std::string name;
  bool player;
};

/////////////////////////////////////////////////////////////////////////////

template <typename P, typename T>
auto Entity::match(P p, T t) {
  switch (type) {
    case Type::Pokemon: return p(*static_cast<Pokemon*>(this));
    case Type::Trainer: return t(*static_cast<Trainer*>(this));
  }
}

template <typename P, typename T>
auto Entity::match(P p, T t) const {
  switch (type) {
    case Type::Pokemon: return p(*static_cast<const Pokemon*>(this));
    case Type::Trainer: return t(*static_cast<const Trainer*>(this));
  }
}

using OwnedEntity = std::shared_ptr<Entity>;

/////////////////////////////////////////////////////////////////////////////
