#include "entity.h"

//////////////////////////////////////////////////////////////////////////////

using PokemonSpeciesWithAttacks = std::pair<PokemonSpeciesData, Attacks>;

const Attack& getAttack(const std::string& name) {
  static const HashMap<std::string, Attack> result = [&]{
    std::vector<Attack> attacks{
      {"Ember",    12, 40},
      {"Ice Beam", 12, 60},
      {"Blizzard", 12, 80},
      {"Headbutt", 8,  80},
      {"Tackle",   4,  40},
    };
    HashMap<std::string, Attack> result;
    for (const auto& attack : attacks) {
      result[attack.name] = attack;
    }
    return result;
  }();
  return result.at(name);
}

const PokemonSpeciesWithAttacks& getSpecies(const std::string& name) {
  static const HashMap<std::string, PokemonSpeciesWithAttacks> result = [&]{
    std::vector<std::pair<PokemonSpeciesData, std::vector<std::string>>> species{
      {{"Ratatta", Wide('R'), 60, 1.0 / 4}, {"Headbutt", "Tackle"}},
      {{"Pidgey",  Wide('P'), 30, 1.0 / 3}, {"Tackle"}},
    };
    const auto getAttacks = [&](const std::vector<std::string>& names) {
      Attacks result = {};
      assert(names.size() <= result.size());
      for (size_t i = 0; i < names.size(); i++) {
        result[i] = &getAttack(names[i]);
      }
      return result;
    };
    HashMap<std::string, PokemonSpeciesWithAttacks> result;
    for (const auto& [species, attacks] : species) {
      const auto data = PokemonSpeciesWithAttacks{species, getAttacks(attacks)};
      result.emplace(species.name, data);
    }
    return result;
  }();
  return result.at(name);
}

std::shared_ptr<PokemonIndividualData> getIndividual(const std::string& name) {
  const auto& [species, attacks] = getSpecies(name);
  return std::make_shared<PokemonIndividualData>(
      PokemonIndividualData{attacks, species, std::shared_ptr<Trainer>()});
}

//////////////////////////////////////////////////////////////////////////////

Entity::Entity(Type type_, Point pos_, Glyph glyph_, int32_t hp, double speed_)
    : type(type_), removed(false), pos(pos_), glyph(glyph_),
      move_timer(0), turn_timer(0), cur_hp(hp), max_hp(hp), speed(speed_) {}

Trainer::Trainer(std::string name_, Point pos, bool player_,
                 int32_t hp, double speed)
    : Entity(Type::Trainer, pos, Wide('@'), hp, speed),
      name(std::move(name_)), player(player_) {}

Pokemon::Pokemon(const std::string& species, Point pos)
    : Pokemon(getIndividual(species), pos) {}

Pokemon::Pokemon(std::shared_ptr<PokemonIndividualData> self, Point pos)
    : Entity(Type::Pokemon, pos, self->species.glyph,
             self->species.hp, self->species.speed),
      self(std::move(self)) {}

//////////////////////////////////////////////////////////////////////////////
