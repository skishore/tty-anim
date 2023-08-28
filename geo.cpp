#include "geo.h"

//////////////////////////////////////////////////////////////////////////////
// Tran-Thong symmetric line-of-sight calculation.

std::vector<Point> LOS(const Point& a, const Point& b) {
  auto const x_diff = std::abs(a.x - b.x);
  auto const y_diff = std::abs(a.y - b.y);
  auto const x_sign = b.x < a.x ? -1 : 1;
  auto const y_sign = b.y < a.y ? -1 : 1;

  auto const size = static_cast<size_t>(std::max(x_diff, y_diff) + 1);
  auto result = std::vector<Point>{};
  result.reserve(size);
  result.push_back(a);

  auto test = 0;
  auto current = a;

  if (x_diff >= y_diff) {
    test = (x_diff + test) / 2;
    for (auto i = 0; i < x_diff; i++) {
      current.x += x_sign;
      test -= y_diff;
      if (test < 0) {
        current.y += y_sign;
        test += x_diff;
      }
      result.push_back(current);
    }
  } else {
    test = (y_diff + test) / 2;
    for (auto i = 0; i < y_diff; i++) {
      current.y += y_sign;
      test -= x_diff;
      if (test < 0) {
        current.x += x_sign;
        test += y_diff;
      }
      result.push_back(current);
    }
  }

  assert(result.size() == size);
  return result;
};

//////////////////////////////////////////////////////////////////////////////
