// Containers: every standard container, same API, elements in PSRAM.

#include <PsramStl.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!psram::begin(Serial)) return;

  // std::map with PSRAM nodes. Keys and values can be PSRAM strings too.
  psram::map<psram::string, int> stock;
  stock["resistor, 10k, 0603"] = 5000;
  stock["capacitor, 100nF, 0402"] = 12000;
  stock["ESP32-S3-WROOM-1-N8R8"] = 40;

  // std::unordered_map: nodes and the bucket array both land in PSRAM.
  psram::unordered_map<uint32_t, float> readings;
  readings.reserve(2048);  // size the bucket array once
  for (uint32_t id = 0; id < 2000; ++id) {
    readings[id] = id * 0.1f;
  }

  // Sequences.
  psram::deque<int> window;
  psram::list<int> queue;
  psram::set<int> seen;
  for (int i = 0; i < 1000; ++i) {
    window.push_back(i);
    queue.push_front(i);
    seen.insert(i % 97);
  }

  for (const auto& [part, count] : stock) {
    Serial.printf("%-26s %6d\n", part.c_str(), count);
  }
  Serial.printf("readings %u, window %u, queue %u, unique %u\n", (unsigned)readings.size(),
                (unsigned)window.size(), (unsigned)queue.size(), (unsigned)seen.size());
  Serial.printf("a map node in PSRAM: %s\n", psram::is_psram(&*stock.begin()) ? "yes" : "no");
  psram::report(Serial);
}

void loop() {}
