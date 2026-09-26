// Diagnostics: watch the PSRAM heap and catch fragmentation early.

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
    // frees the old buffer and allocates a bigger one, leaving gaps behind.
    psram::vector<psram::vector<int>> grown(200);
    for (int round = 0; round < 64; ++round)
      for (auto& v : grown) v.push_back(round);
    printHeap("200 vectors, grown");
  }
  {
    // Same data with reserve(): one allocation per vector, no gaps.
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
