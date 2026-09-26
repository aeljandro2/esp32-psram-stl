// ObjectsInPsram: put your own objects in PSRAM, three ways.

#include <PsramStl.h>

// 1. A plain type, created with psram::make_unique / psram::make_shared.
struct Histogram {
  uint32_t bins[4096] = {};  // 16 KB
  void add(uint16_t value) { bins[value % 4096]++; }
};

// 2. A type that always lives in PSRAM when created with `new`.
//    psram::resident is an empty base: it adds no bytes.
class ImageBuffer : public psram::resident {
 public:
  uint16_t pixels[320 * 240];  // 150 KB
};

// 3. Hierarchies: derive the base from psram::resident, keep std::unique_ptr.
struct Filter : psram::resident {
  virtual ~Filter() = default;
  virtual float apply(float x) = 0;
};
struct LowPass : Filter {
  float state = 0;
  float apply(float x) override { return state += 0.1f * (x - state); }
};

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!psram::begin(Serial)) return;

  auto histogram = psram::make_unique<Histogram>();
  for (int i = 0; i < 100000; ++i) histogram->add(i * 7);

  auto shared = psram::make_shared<Histogram>();  // object and counts in one PSRAM block

  ImageBuffer* frame = new ImageBuffer();
  frame->pixels[0] = 0xF800;

  std::unique_ptr<Filter> filter(new LowPass());
  float y = 0;
  for (int i = 0; i < 100; ++i) y = filter->apply(1.0f);

  Serial.printf("histogram in PSRAM: %s\n", psram::is_psram(histogram.get()) ? "yes" : "no");
  Serial.printf("shared in PSRAM:    %s\n", psram::is_psram(shared.get()) ? "yes" : "no");
  Serial.printf("frame in PSRAM:     %s\n", psram::is_psram(frame) ? "yes" : "no");
  Serial.printf("filter in PSRAM:    %s (output %.3f)\n", psram::is_psram(filter.get()) ? "yes" : "no", y);

  delete frame;
  psram::report(Serial);
}

void loop() {}
