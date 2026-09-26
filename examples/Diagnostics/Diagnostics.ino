// Diagnostics: watch the PSRAM heap.
//
// On a freshly booted board the two patterns below end up close; the habit
// that matters on a long-running device is watching largest_block, not just free.

#include <PsramStl.h>

void printHeap(const char* label) {
  const psram::heap_info h = psram::info();
  Serial.printf("%-22s free %8u  largest %8u  min %8u\n", label, (unsigned)h.free,
                (unsigned)h.largest_block, (unsigned)h.minimum_free);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!psram::begin(Serial)) return;
  printHeap("start");

  {
    // Many small vectors growing one push_back at a time: each growth step
    // allocates a bigger buffer, copies, and frees the old one.
    psram::vector<psram::vector<int>> grown(200);
    for (int round = 0; round < 64; ++round)
      for (auto& v : grown) v.push_back(round);
    printHeap("200 vectors, grown");
  }
  {
    // Same data with reserve(): one allocation per vector.
    psram::vector<psram::vector<int>> reserved(200);
    for (auto& v : reserved) v.reserve(64);
    for (int round = 0; round < 64; ++round)
      for (auto& v : reserved) v.push_back(round);
    printHeap("200 vectors, reserved");
  }

  printHeap("end");
  Serial.printf("heap integrity: %s\n", psram::check_integrity() ? "ok" : "CORRUPTED");
}

void loop() {}
