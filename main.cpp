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
  update(cols - particle->w, &particle->x, &particle->dx);
  update(rows - particle->h, &particle->y, &particle->dy);
}

std::string render(
    size_t cols, size_t rows, const std::vector<Particle>& particles) {
  auto pixels = std::vector<uint8_t>(cols * rows, 0);
  for (auto const& particle : particles) {
    for (size_t i = 0; i < particle.w; i++) {
      for (size_t j = 0; j < particle.h; j++) {
        auto const x = particle.x + i;
        auto const y = particle.y + j;
        assert(0 <= x && x < cols);
        assert(0 <= y && y < rows);
        pixels[x + cols * y] = particle.color;
      }
    }
  }

  std::string result = "\x1b[2J\x1b[?25l\x1b[H";
  for (size_t y = 0; y < rows; y++) {
    size_t last = 0;
    for (size_t x = 0; x < cols; x++) {
      auto const color = pixels[x + cols * y];
      if (!color) continue;
      if (last < x) result.append(x - last, ' ');
      result.append("\x1b[3");
      result.push_back('0' + color);
      result.append("m#\x1b[39m");
      last = x + 1;
    }
    if (y < rows) result.append("\x1b[0K\r\n");
  }
  return result;
}

struct State {
  State(size_t cols, size_t rows) : cols(cols), rows(rows) {
    for (auto i = 0; i < 10; i++) particles.push_back(create(cols, rows));
  }

  std::string render() const { return ::render(cols, rows, particles); }
  void update() { for (auto& x : particles) ::update(cols, rows, &x); }

 private:
  size_t cols;
  size_t rows;
  std::vector<Particle> particles;
};

//////////////////////////////////////////////////////////////////////////////

struct Terminal {
  Terminal() {
    auto const error = []{
      throw std::runtime_error("Terminal initialization failed!");
    };

    auto const fd = STDIN_FILENO;
    if (!isatty(fd)) error();
    if (tcgetattr(fd, &original) == -1) error();

    auto raw = original;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) error();

    onResize();
  }

  ~Terminal() { exit(); }

  size_t getCols() const { return window.ws_col; }
  size_t getRows() const { return window.ws_row; }

  void tick(const std::string& status) {
    if (!state) return;
    state->update();
    auto render = state->render();
    render.append(status);
    write(STDOUT_FILENO, render.data(), render.size());
  }

  void onResize() {
    auto const code = ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    auto const fail = code || !getCols() || !getRows();
    if (fail) throw std::runtime_error("Failed to get window size!");
    state = std::make_unique<State>(getCols(), getRows());
  }

  void exit() const {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    const std::string restore = "\x1b[?25h";
    write(STDOUT_FILENO, restore.data(), restore.size());
  }

 private:
  termios original;
  winsize window;
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
static std::function<void()> g_resize_handler = nullptr;
void resizeHandler(int) { if (g_resize_handler) g_resize_handler(); }
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
  signal(SIGWINCH, resizeHandler);
  Terminal terminal;
  g_resize_handler = [&]{ terminal.onResize(); };

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
