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

#include "game.h"

//////////////////////////////////////////////////////////////////////////////

short ColorIndex(uint8_t fg, uint8_t bg) {
  return ((static_cast<short>(fg) << 8) | static_cast<short>(bg)) + 1;
}

struct Terminal {
  Terminal() {
    initTerminal(true);
    terminalSize = getSize();
    auto constexpr size = 30;
    state = std::make_unique<State>(Point{size, size});
  }

  ~Terminal() { exit(); }

  void tick(const std::string& status) {
    auto const emitCharacter = [](uint16_t ch){
      if (ch > 0xff00) {
        const unsigned char ch0 = (ch & 0x3f) | 0x80;
        const unsigned char ch1 = 0xbc;
        const unsigned char ch2 = 0xef;
        std::cout << ch2 << ch1 << ch0;
      } else {
        std::cout << static_cast<unsigned char>(ch);
      }
    };

    auto size = state->board.size();
    const Point offset{1 + (terminalSize.x - 2 * size.x) / 2,
                       1 + (terminalSize.y - size.y) / 2};
    for (auto row = 0; row < size.y; row++) {
      moveCursor(offset + Point{0, row});
      for (auto col = 0; col < size.x; col++) {
        emitCharacter('.');
      }
    }

    const auto col = terminalSize.x - static_cast<int>(status.size());
    moveCursor({col, terminalSize.y});
    std::cout << "\x1b[2K" << status << std::flush;

    //state->update();
    //erase();
    //state->render();
    //auto const color = 1 + 16 + 1 * 0 + 6 * 2 + 36 * 4;
    //attr_set(0, 256, nullptr);
    //mvaddstr(getRows() - 1, 0, status.data());
    //refresh();
  }

  void exit() { initTerminal(false); }

 private:
  Point getSize() const {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return {w.ws_col, w.ws_row};
  }

  void initTerminal(bool enabled) {
    const char* code = enabled ? "\x1b[?1049h\x1b[?25l"
                               : "\x1b[?1049l\x1b[?25h";
    std::cout << code << std::flush;

    struct termios t;
    tcgetattr(0, &t);
    enabled ? (t.c_lflag &= ~ECHO) : (t.c_lflag |= ECHO);
    tcsetattr(0, TCSANOW, &t);
  }

  void moveCursor(Point point) {
    std::cout << "\x1b[" << point.y << ";" << point.x << "H";
  }

  Point terminalSize;
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
