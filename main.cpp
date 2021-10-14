#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>

auto constexpr kFPS = 60;
auto constexpr kUsPerSecond = 1000000;

struct Timing {
  using time_us_t = uint64_t;
  struct Stats { double cpu; double fps; };

  Timing() {
    auto const ts = time();
    for (auto i = 0; i < kFPS; i++) frames[i] = {ts, ts};
  }

  void block() const {
    auto const& a = frames[index % kFPS];
    auto const next = a.first + kUsPerSecond;
    auto const prev = time();
    if (next <= prev) return;
    auto const delay = std::min<time_us_t>(next - prev, kUsPerSecond / kFPS);
    std::this_thread::sleep_for(std::chrono::microseconds(delay));
  }

  Stats stats() const {
    auto const& a = frames[index % kFPS];
    auto const& b = frames[(index + kFPS - 1) % kFPS];
    auto const total = std::max<time_us_t>(b.second - a.first, 1);
    auto const cpu = static_cast<double>(used) * 100 / total;
    auto const fps = static_cast<double>(kFPS) * kUsPerSecond / total;
    return {cpu, fps};
  }

  void start() {
    auto& frame = frames[index % kFPS];
    used -= frame.second - frame.first;
    frame.first = time();
  }

  void end() {
    auto& frame = frames[index % kFPS];
    frame.second = time();
    used += frame.second - frame.first;
    index++;
  }

 private:
  time_us_t time() const {
    auto const epoch = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
  }

  using Frame = std::pair<time_us_t, time_us_t>;
  Frame frames[kFPS];
  time_us_t used = 0;
  size_t index = 0;
};

int main() {
  auto timing = Timing();
  while (true) {
    timing.block();
    timing.start();
    auto tmp = std::vector<size_t>();
    for (auto i = 0; i < 10000; i++) {
      tmp.push_back(i);
    }
    timing.end();
    auto const stats = timing.stats();
    std::cout << std::fixed << std::setprecision(2)
              << "CPU: " << stats.cpu << "%; FPS: " << stats.fps << std::endl;
  }
}
