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

  double lenL2() const { return std::sqrt(lenL2Squared()); }
  int32_t lenL2Squared() const { return x * x + y * y; }

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
  Matrix() {}

  Matrix(Point size, const Value& init)
    : m_size(size), m_init(init), m_data(size.x * size.y, init) {}

  Point size() const { return m_size; }

  Value get(Point p) const {
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
  Point m_size = {};
  Value m_init = {};
  std::vector<Value> m_data;
};

//////////////////////////////////////////////////////////////////////////////

std::vector<Point> LOS(const Point& a, const Point& b);

struct FOVNode {
  const Point point = {};
  const FOVNode* parent = {};
  std::vector<std::unique_ptr<FOVNode>> children;
};

struct FOV {
  FOV(int32_t r) : radius(r) {
    root = std::make_unique<FOVNode>();
    for (auto i = 0; i <= radius; i++) {
      for (auto j = 0; j < 8; j++) {
        auto const [xa, ya] = (j & 1) ? Point{radius, i} : Point{i, radius};
        auto const xb = xa * ((j & 2) ? 1 : -1);
        auto const yb = ya * ((j & 4) ? 1 : -1);
        trieUpdate(*root, LOS(Point::origin(), {xb, yb}), 0);
      }
    }
  }

  template <typename Fn>
  void fieldOfVision(Fn blocked) const {
    cache.clear();
    cache.push_back(root.get());
    for (size_t i = 0; i < cache.size(); i++) {
      auto const node = cache[i];
      auto const prev = node->parent ? &node->parent->point : nullptr;
      if (blocked(node->point, prev)) continue;
      for (const auto& x : node->children) cache.push_back(x.get());
    }
  }

private:
  void trieUpdate(FOVNode& node, const std::vector<Point>& line, size_t i) {
    assert(line[i] == node.point);
    if (node.point.lenL2() > radius - 0.5) return;
    if (!(i + 1 < line.size())) return;

    auto const next = line[i + 1];
    auto& child = [&]() -> FOVNode& {
      for (auto& x : node.children) if (x->point == next) return *x;
      node.children.emplace_back(new FOVNode{next, &node, {}});
      return *node.children.back();
    }();
    assert(next == child.point);
    trieUpdate(child, line, i + 1);
  }

  const int32_t radius;
  std::unique_ptr<FOVNode> root;
  mutable std::vector<const FOVNode*> cache;
};

//////////////////////////////////////////////////////////////////////////////
