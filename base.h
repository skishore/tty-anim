#pragma once

#include <cstdint>

#define DISALLOW_COPY_AND_ASSIGN(X) \
  X(X&&) = delete;                  \
  X(const X&) = delete;             \
  X& operator=(X&&) = delete;       \
  X& operator=(const X&) = delete;

struct Color { uint8_t value; };
struct Glyph { uint16_t ch; Color fg; Color bg; };

constexpr static Color kBlack = {0};
constexpr static Color kNone  = {255};
constexpr static Color kGray  = {16 + 216 + 5};

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
