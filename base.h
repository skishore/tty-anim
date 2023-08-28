#pragma once

#include <array>
#include <cstdint>

//////////////////////////////////////////////////////////////////////////////

#define DISALLOW_COPY_AND_ASSIGN(X) \
  X(X&&) = delete;                  \
  X(const X&) = delete;             \
  X& operator=(X&&) = delete;       \
  X& operator=(const X&) = delete;

//////////////////////////////////////////////////////////////////////////////

struct Color { uint8_t value; };
struct Glyph { uint16_t ch; Color fg; Color bg; };

inline bool operator==(const Color& a, const Color& b) {
  return a.value == b.value;
}
inline bool operator!=(const Color& a, const Color& b) { return !(a == b); }

inline bool operator==(const Glyph& a, const Glyph& b) {
  return a.ch == b.ch && a.fg == b.fg && a.bg == b.bg;
}
inline bool operator!=(const Glyph& a, const Glyph& b) { return !(a == b); }

constexpr static Color kBlack = {0};
constexpr static Color kNone  = {255};
constexpr static Color kGray  = {16 + 216 + 5};

// Usage: RGB(0x420) is a color w/ intensity 4/5 red, 2/5 green, 0/5 blue.
constexpr static Color RGB(int code) {
  auto const r = (code >> 8) & 0xf;
  auto const g = (code >> 4) & 0xf;
  auto const b = (code >> 0) & 0xf;
  return {static_cast<uint8_t>(16 + b + 6 * g + 36 * r)};
}

constexpr static Glyph Wide(char ch, int code = 255) {
  auto const offset = 0xff00 - 0x20;
  auto const wide = static_cast<char16_t>(static_cast<uint32_t>(ch) + offset);
  return {wide, code == 255 ? kNone : RGB(code), kNone};
}

constexpr static Glyph Empty() { return Wide(' '); }

//////////////////////////////////////////////////////////////////////////////

#define INPUTS(X) \
  X(Esc)          \
  X(Tab)          \
  X(Enter)        \
  X(Up)           \
  X(Down)         \
  X(Right)        \
  X(Left)         \
  X(ShiftTab)     \
  X(ShiftUp)      \
  X(ShiftDown)    \
  X(ShiftRight)   \
  X(ShiftLeft)    \

enum struct Input : uint8_t {
#define X(name) name,
INPUTS(X)
#undef X
};

inline const char* show(Input input) {
  auto const start = 0x20, limit = 0x7f;
  static const std::array<char, 256> cache = [&]{
    std::array<char, 256> result;
    for (auto i = start; i < limit; i++) {
      result[2 * (i - start) + 0] = i;
      result[2 * (i - start) + 1] = '\0';
    }
    return result;
  }();
  auto const i = static_cast<int>(input);
  if (start <= i && i < limit) return &cache[2 * (i - start)];
  switch (input) {
#define X(name) case Input::name: return #name;
INPUTS(X)
#undef X
  }
  return "<unknown>";
}

#undef INPUTS

//////////////////////////////////////////////////////////////////////////////
