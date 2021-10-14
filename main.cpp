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

//////////////////////////////////////////////////////////////////////////////

struct Particle {
  size_t x;
  size_t y;
  size_t w;
  size_t h;
  ssize_t dx;
  ssize_t dy;
  uint8_t color;
};

void update(size_t max, size_t* x, ssize_t* v) {
  auto nx = static_cast<ssize_t>(*x + *v);
  while (!(0 <= nx && nx <= static_cast<ssize_t>(max))) {
    nx = nx < 0 ? -nx : 2 * max - nx;
    *v = -*v;
  }
  *x = nx;
}

size_t random(size_t n) {
  return rand() % n;
}

Particle create(size_t cols, size_t rows) {
  auto const w = 4;
  auto const h = 2;
  auto const dx = (random(2) ? w : -w) / 2;
  auto const dy = (random(2) ? h : -h) / 2;
  auto const color = static_cast<uint8_t>(random(7) + 1);
  return {random(cols - w), random(rows - h), w, h, dx, dy, color};
}

void update(size_t cols, size_t rows, Particle* particle) {
  if (random(100)) return;
  update(cols - particle->w, &particle->x, &particle->dx);
  update(rows - particle->h, &particle->y, &particle->dy);
}

void render(size_t cols, size_t rows, const std::vector<Particle>& particles) {
  for (auto const& particle : particles) {
    for (size_t i = 0; i < particle.w; i++) {
      for (size_t j = 0; j < particle.h; j++) {
        auto const x = particle.x + i;
        auto const y = particle.y + j;
        assert(0 <= x && x < cols);
        assert(0 <= y && y < rows);
        attron(COLOR_PAIR(particle.color + 1));
        mvaddch(y, x, '#');
        attroff(COLOR_PAIR(particle.color + 1));
      }
    }
  }
}

struct State {
  State(size_t cols, size_t rows) : cols(cols), rows(rows) {
    for (auto i = 0; i < 100; i++) particles.push_back(create(cols, rows));
  }

  void render() const { ::render(cols, rows, particles); }
  void update() { for (auto& x : particles) ::update(cols, rows, &x); }

 private:
  size_t cols;
  size_t rows;
  std::vector<Particle> particles;
};

//////////////////////////////////////////////////////////////////////////////

struct Terminal {
  Terminal() {
    initscr();
    curs_set(0);
    use_default_colors();
    start_color();
    for (auto i = 0; i < 8; i++) {
      init_pair(i + 1, i, -1);
    }
    cbreak();
    noecho();
    erase();

    state = std::make_unique<State>(getCols() - 1, getRows() - 1);
  }

  ~Terminal() { exit(); }

  size_t getCols() const { return COLS; }
  size_t getRows() const { return LINES; }

  void tick(const std::string& status) {
    if (!state) return;
    state->update();
    erase();
    state->render();
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
