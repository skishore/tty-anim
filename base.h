#pragma once

#include <cstdint>

#define DISALLOW_COPY_AND_ASSIGN(X) \
  X(X&&) = delete;                  \
  X(const X&) = delete;             \
  X& operator=(X&&) = delete;       \
  X& operator=(const X&) = delete;

struct Color { uint8_t value; };
struct Glyph { uint16_t ch; Color fg; Color bg; };
