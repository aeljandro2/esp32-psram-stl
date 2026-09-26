// LargeBuffers: a ring buffer of sensor frames, aligned for fast copies.

#include <PsramStl.h>

// 64-byte alignment: each frame starts on its own cache line.
struct alignas(64) Frame {
  uint32_t timestamp;
  int16_t accel[3];
  int16_t gyro[3];
  float temperature;
  uint8_t padding[40];
};

class FrameRing {
 public:
  explicit FrameRing(size_t capacity) : frames_(capacity) {}
  void push(const Frame& f) {
    frames_[head_] = f;
    head_ = (head_ + 1) % frames_.size();
    count_ = count_ < frames_.size() ? count_ + 1 : count_;
  }
  size_t size() const { return count_; }
  const Frame* data() const { return frames_.data(); }

 private:
  psram::vector<Frame> frames_;
  size_t head_ = 0;
  size_t count_ = 0;
};

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!psram::begin(Serial)) return;

  FrameRing ring(20000);  // 20k frames x 64 B = 1.25 MB
  for (uint32_t t = 0; t < 50000; ++t) {
    Frame f{};
    f.timestamp = t;
    f.temperature = 25.0f + (t % 100) * 0.01f;
    ring.push(f);
  }

  Serial.printf("frames kept: %u\n", (unsigned)ring.size());
  Serial.printf("64-byte aligned: %s\n", (reinterpret_cast<uintptr_t>(ring.data()) % 64 == 0) ? "yes" : "no");
  Serial.printf("in PSRAM: %s\n", psram::is_psram(ring.data()) ? "yes" : "no");
  psram::report(Serial);
}

void loop() {}
