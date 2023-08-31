#include "entity.h"

#include <random>

//////////////////////////////////////////////////////////////////////////////

Entity::Entity(Type type_, Point pos_, Glyph glyph_, int32_t hp, double speed_)
    : type(type_), removed(false), pos(pos_), glyph(glyph_),
      move_timer(0), turn_timer(0), cur_hp(hp), max_hp(hp), speed(speed_) {}

Trainer::Trainer(std::string name_, Point pos, bool player_,
                 int32_t hp, double speed)
    : Entity(Type::Trainer, pos, Wide('@'), hp, speed),
      name(std::move(name_)), player(player_) {}

//////////////////////////////////////////////////////////////////////////////
