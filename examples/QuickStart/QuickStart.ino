// QuickStart: a vector that would never fit in internal RAM.
//
// Board: any ESP32 with PSRAM. In the Arduino IDE, set Tools > PSRAM to
// "Enabled" (ESP32) or "OPI PSRAM" (ESP32-S3 N8R8 and similar).

#include <PsramStl.h>

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!psram::begin(Serial)) {
    return;  // begin() already printed how to enable PSRAM
  }

  // One million floats: 4 MB would not fit in internal RAM, but PSRAM holds it.
  psram::vector<float> samples;
  samples.reserve(1000000);
  for (int i = 0; i < 1000000; ++i) {
    samples.push_back(i * 0.5f);
  }

  Serial.printf("stored %u floats, last = %.1f\n", (unsigned)samples.size(), samples.back());
  Serial.printf("data lives in PSRAM: %s\n", psram::is_psram(samples.data()) ? "yes" : "no");
  psram::report(Serial);
}

void loop() {}
