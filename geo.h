#pragma once

#include <cstdint>
#include <vector>

#include "base.h"

//////////////////////////////////////////////////////////////////////////////
// Point and Matrix classes, in the header to allow inlining.

struct Point {
  constexpr static Point origin() { return {0, 0}; }

  Point& operator+=(const Point& o) { *this = *this + o; return *this; }
  Point& operator-=(const Point& o) { *this = *this - o; return *this; }

  Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
  Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }

  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
  bool operator!=(const Point& o) const { return x != o.x || y != o.y; }

  double len() const { return std::sqrt(lenSquared()); }
  int32_t lenSquared() const { return x * x + y * y; }

  int32_t lenNethack() const {
    auto const ax = std::abs(x);
    auto const ay = std::abs(y);
    return (46 * std::min(ax, ay) + 95 * std::max(ax, ay) + 25) / 100;
  }

  int32_t lenTaxicab() const { return std::abs(x) + std::abs(y); }
  int32_t lenWalking() const { return std::max(std::abs(x), std::abs(y)); }

  int32_t x;
  int32_t y;
};

template <typename H>
H AbslHashValue(H h, const Point& p) {
  return H::combine(std::move(h), p.x, p.y);
}

static_assert(std::is_pod<Point>::value);

template<typename Value>
struct Matrix {
  explicit Matrix(Point size, const Value& init)
    : m_size(size), m_init(init), m_data(size.x * size.y, init) {}

  Point size() const { return m_size; }

  const Value& get(Point p) const {
    return contains(p) ? m_data[p.x + m_size.x * p.y] : m_init;
  }

  void set(Point p, const Value& v) {
    if (contains(p)) m_data[p.x + m_size.x * p.y] = v;
  }

  bool contains(Point p) const {
    return 0 <= p.x && p.x < m_size.x && 0 <= p.y && p.y < m_size.y;
  }

  void fill(const Value& v) {
    std::fill(m_data.begin(), m_data.end(), v);
  }

private:
  const Point m_size;
  const Value m_init;
  std::vector<Value> m_data;

  DISALLOW_COPY_AND_ASSIGN(Matrix);
};

//////////////////////////////////////////////////////////////////////////////

std::vector<Point> LOS(const Point& a, const Point& b);

//////////////////////////////////////////////////////////////////////////////
