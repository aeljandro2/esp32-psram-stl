// Policies: what happens when PSRAM is full.
//
//   psram::vector<T>            PSRAM only. Out of memory is an error
//                               (std::bad_alloc, or log and abort without exceptions).
//   psram::fallback::vector<T>  PSRAM first, internal RAM when PSRAM is full.

#include <PsramStl.h>

void logOutOfMemory(size_t bytes) {
  Serial.printf("[psram] out of memory: %u bytes requested\n", (unsigned)bytes);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!psram::begin(Serial)) return;

  psram::set_oom_handler(logOutOfMemory);

  // Ask for more than the whole PSRAM.
  const size_t tooMuch = psram::info().total + 1;
  try {
    psram::vector<uint8_t> strict;
    strict.reserve(tooMuch);
  } catch (const std::bad_alloc&) {
    Serial.println("psram::vector: request refused, nothing allocated");
  }

  // The fallback family keeps working when PSRAM is full.
  psram::fallback::vector<int> flexible(1000, 42);
  Serial.printf("psram::fallback::vector landed in %s\n",
                psram::is_psram(flexible.data()) ? "PSRAM" : "internal RAM");
}

void loop() {}
