#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// Point and Matrix classes, in the header to allow inlining.

struct Point {
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

template<typename T>
struct Matrix {
  Matrix(const Point& size, const T& init)
    : m_size(size), m_data(size.x * size.y, init) {}

  const T& get(const Point& p) const {
    assert(contains(p));
    return m_data[p.x + m_size.x * p.y];
  }

  void set(const Point& p, const T& t) {
    assert(contains(p));
    m_data[p.x + m_size.x * p.y] = t;
  }

  bool contains(const Point& p) const {
    return 0 <= p.x && p.x < m_size.x && 0 <= p.y && p.y < m_size.y;
  }

private:
  Point m_size;
  std::vector<T> m_data;
};

//////////////////////////////////////////////////////////////////////////////

std::vector<Point> LOS(const Point& a, const Point& b);

//////////////////////////////////////////////////////////////////////////////
