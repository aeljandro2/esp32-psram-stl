// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
// https://github.com/aeljandro2/esp32-psram-stl
//
// One include gives you everything:
//
//   #include <PsramStl.h>
//
//   psram::vector<float> samples;                  // std::vector, elements in PSRAM
//   psram::map<int, psram::string> names;          // std::map, nodes in PSRAM
//   auto frame = psram::make_unique<Frame>();      // your object, in PSRAM
//   psram::fallback::vector<int> safe;             // PSRAM first, internal RAM if full
//
// Header-only, C++17, no dependencies beyond ESP-IDF or Arduino-ESP32.

#pragma once

#include "psram/config.hpp"
#include "psram/allocator.hpp"
#include "psram/containers.hpp"
#include "psram/memory.hpp"
#include "psram/diagnostics.hpp"
