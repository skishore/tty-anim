#include <assert.h>
#include <execinfo.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include <curses.h>

#include "game.h"

//////////////////////////////////////////////////////////////////////////////

short ColorIndex(uint8_t fg, uint8_t bg) {
  return ((static_cast<short>(fg) << 8) | static_cast<short>(bg)) + 1;
}

struct Terminal {
  Terminal() {
    setlocale(LC_ALL, "");
    initscr();
    curs_set(0);
    use_default_colors();
    start_color();
    for (auto i = 0; i < 256; i++) {
      for (auto j = 0; j < 256; j++) {
        auto const index = ColorIndex(i, j);
        if (index < COLOR_PAIRS) init_pair(index, i - 1, j - 1);
      }
    }
    init_pair(256, 1, -1);
    cbreak();
    noecho();
    erase();

    auto constexpr size = 60;
    state = std::make_unique<State>(Point{size, size});
  }

  ~Terminal() { exit(); }

  size_t getCols() const { return COLS; }
  size_t getRows() const { return LINES; }

  void tick(const std::string& status) {
    if (!state) return;
    //state->update();
    erase();
    //state->render();
    //auto const color = 1 + 16 + 1 * 0 + 6 * 2 + 36 * 4;
    attr_set(0, 256, nullptr);
    mvaddstr(getRows() - 1, 0, status.data());
    refresh();
  }

  void exit() const { endwin(); }

 private:
  std::unique_ptr<State> state;
};

//////////////////////////////////////////////////////////////////////////////

auto constexpr kFPS = 60;
auto constexpr kUsPerSecond = 1000000;
auto constexpr kUsPerFrame = kUsPerSecond / kFPS;
auto constexpr kUsMinDelay = int(0.9 * kUsPerFrame);

struct Timing {
  using time_us_t = uint64_t;
  struct Stats { double cpu; double fps; };

  Timing() {
    auto const ts = time();
    for (auto i = 0; i < kFPS; i++) frames[i] = {ts, ts};
  }

  void block() const {
    auto const& a = frames[index % kFPS];
    auto const& b = frames[(index + kFPS - 1) % kFPS];
    auto const next = std::max(a.first + kUsPerSecond, b.first + kUsMinDelay);
    auto const prev = time();
    if (next <= prev) return;
    auto const delay = std::min<time_us_t>(next - prev, kUsPerFrame);
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
    auto const epoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
  }

  using Frame = std::pair<time_us_t, time_us_t>;
  Frame frames[kFPS];
  time_us_t used = 0;
  size_t index = 0;
};

//////////////////////////////////////////////////////////////////////////////

static bool g_done = false;
void sigintHandler(int) { g_done = true; }

void segfaultHandler(int) {
  auto constexpr kNumFrames = 64;
  void* frames[kNumFrames];
  auto const size = backtrace(frames, kNumFrames);
  backtrace_symbols_fd(frames, size, STDERR_FILENO);
  exit(1);
}

int main() {
  srand(time(nullptr));
  signal(SIGINT, sigintHandler);
  signal(SIGSEGV, segfaultHandler);
  Terminal terminal;

  auto timing = Timing();
  while (!g_done) {
    auto const stats = timing.stats();
    timing.block();
    timing.start();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2)
       << "CPU: " << stats.cpu << "%; FPS: " << stats.fps;
    terminal.tick(ss.str());
    timing.end();
  }
}
