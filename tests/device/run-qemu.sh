#!/usr/bin/env bash
# Runs the device tests in Espressif's QEMU and stops as soon as they report.
# Usage (inside the ESP-IDF environment): ./run-qemu.sh esp32|esp32s3
set -u
target="$1"
log="build/qemu-$target.log"
idf.py -B "build/$target" qemu > "$log" 2>&1 &
for _ in $(seq 1 1800); do
  if grep -q -E "ALL TESTS PASSED|TESTS FAILED|Guru Meditation|abort\(\) was called" "$log"; then
    break
  fi
  sleep 1
done
pkill -f qemu-system || true
wait
grep -E "^\[(PASS|FAIL)\]|expectation failed|DEVICE TESTS|ALL TESTS PASSED|TESTS FAILED|Guru Meditation|abort\(\)" "$log"
grep -q "ALL TESTS PASSED" "$log"
