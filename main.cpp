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

#include "game.h"

//////////////////////////////////////////////////////////////////////////////

short ColorIndex(uint8_t fg, uint8_t bg) {
  return ((static_cast<short>(fg) << 8) | static_cast<short>(bg)) + 1;
}

struct Terminal {
  Terminal() {
    initTerminal(true);
    terminalSize = getSize();
    lastFrame = io.frame;
  }

  ~Terminal() { exit(); }

  void tick(const std::string& status) {
    getKeyInputs(io.inputs);
    io.tick();

    auto const size = lastFrame.size();
    assert(size == io.frame.size());
    const Point offset{(terminalSize.x - size.x) / 2,
                       (terminalSize.y - size.y) / 2};
    for (auto row = 0; row < size.y; row++) {
      auto min_col = size.x;
      auto max_col = 0;
      for (auto col = 0; col < size.x; col++) {
        auto const point = Point{col, row};
        auto const old_glyph = lastFrame.get(point);
        auto const new_glyph = io.frame.get(point);
        if (old_glyph == new_glyph) continue;
        min_col = std::min(min_col, col);
        max_col = std::max(max_col, col);
      }
      if (min_col > max_col) continue;

      auto const point = Point{min_col, row};
      auto const glyph = io.frame.get(point);
      moveCursor(offset + point);
      setColors(glyph.fg, glyph.bg);
      auto prev = glyph;

      for (auto col = min_col; col <= max_col;) {
        auto const point = Point{col, row};
        auto const glyph = io.frame.get(point);
        if (glyph.fg != prev.fg) setForegroundColor(glyph.fg);
        if (glyph.bg != prev.bg) setBackgroundColor(glyph.bg);
        lastFrame.set(point, glyph);
        prev = glyph;

        if (glyph.ch > 0xff00) {
          const char ch0 = (glyph.ch & 0x3f) | 0x80;
          const char ch1 = 0xbc;
          const char ch2 = 0xef;
          std::cout << ch2 << ch1 << ch0;
          col += 2;
        } else {
          std::cout << static_cast<char>(glyph.ch);
          col += 1;
        }
      }
    }

    const auto col = terminalSize.x - static_cast<int>(status.size());
    moveCursor({col - 1, terminalSize.y - 1});
    setColors(kNone, kNone);
    std::cout << "\x1b[2K" << status << std::flush;
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
    auto const flags = ICANON | ECHO;
    enabled ? (t.c_lflag &= ~flags) : (t.c_lflag |= flags);
    tcsetattr(0, TCSANOW, &t);
  }

  void moveCursor(Point point) {
    std::cout << "\x1b[" << point.y + 1 << ";" << point.x + 1 << "H";
  }

  void setColors(Color fg, Color bg) {
    setForegroundColor(fg);
    setBackgroundColor(bg);
  }

  void setForegroundColor(Color color) {
    if (color == kNone) return static_cast<void>(std::cout << "\x1b[39m");
    std::cout << "\x1b[38;5;" << static_cast<int>(color.value) << 'm';
  }

  void setBackgroundColor(Color color) {
    if (color == kNone) return static_cast<void>(std::cout << "\x1b[49m");
    std::cout << "\x1b[48;5;" << static_cast<int>(color.value) << 'm';
  }

  void getKeyInputs(std::deque<Input>& inputs) {
    int count = 0;
    ioctl(STDIN_FILENO, FIONREAD, &count);
    for (auto i = 0; i < count;) {
      auto const getch = [&]{ i++; return getchar(); };
      auto const ch = getch();
      if (ch != 27 || i >= count) {
        if (0x20 <= ch && ch < 0x7f) {
          inputs.push_back(Input(ch));
        } else if (ch == 9) {
          inputs.push_back(Input::Tab);
        } else if (ch == 10) {
          inputs.push_back(Input::Enter);
        } else if (i >= count) {
          inputs.push_back(Input::Esc);
        }
        continue;
      }
      auto const next = getch();
      if (next == 27) {
        inputs.push_back(Input(ch));
        continue;
      } else if (next != '[' || i >= count) {
        i = count;
        continue;
      }
      auto const code = getch();
      if ('A' <= code && code <= 'D') {
        auto const base = static_cast<int>(Input::Up);
        inputs.push_back(Input(base + (code - 'A')));
        continue;
      } else if (code == 'Z') {
        inputs.push_back(Input::ShiftTab);
        continue;
      }
      auto semi = code;
      while (semi != ';' && i < count) semi = getch();
      auto const modifier = i < count ? getch() : '\0';
      auto const last = i < count ? getch() : '\0';
      if (modifier == '2' && 'A' <= last && last <= 'D') {
        auto const base = static_cast<int>(Input::ShiftUp);
        inputs.push_back(Input(base + (last - 'A')));
      }
    }
  }

  IO io;
  Point terminalSize;
  Matrix<Glyph> lastFrame;
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
