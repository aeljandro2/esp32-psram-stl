#!/usr/bin/env bash
# Boots every compiled Arduino example in Espressif's QEMU (ESP32, 4 MB PSRAM)
# and checks its serial output against tests/arduino/expected.txt.
#
# Run inside the ESP-IDF environment (for example the espressif/idf Docker image),
# after compiling each example with FlashMode=dio into build/arduino/<Example>/:
#   arduino-cli compile --fqbn esp32:esp32:esp32:PSRAM=enabled,FlashMode=dio \
#     --library . --output-dir build/arduino/QuickStart examples/QuickStart
# (QEMU's flash model does not support the QIO mode Arduino uses by default.)
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
qemu="$(ls /opt/esp/tools/qemu-xtensa/*/qemu/bin/qemu-system-xtensa 2>/dev/null | head -1)"
if [ -z "$qemu" ]; then
  echo "qemu-system-xtensa not found: run inside the ESP-IDF environment" >&2
  exit 2
fi

failed=0
while IFS='|' read -r name expected; do
  case "$name" in ''|'#'*) continue ;; esac
  image="$root/build/arduino/$name/$name.ino.merged.bin"
  if [ ! -f "$image" ]; then
    echo "[FAIL] $name: $image not found"
    failed=$((failed + 1))
    continue
  fi
  flash="$(mktemp)"
  log="$root/build/arduino/$name/serial.log"
  cp "$image" "$flash" && truncate -s 4M "$flash"
  "$qemu" -M esp32 -m 4M -nographic -drive file="$flash",if=mtd,format=raw \
    -global driver=timer.esp32.timg,property=wdt_disable,value=true -serial mon:stdio < /dev/null > "$log" 2>&1 &
  pid=$!
  for _ in $(seq 1 90); do
    grep -a -q -F "$expected" "$log" && break
    grep -a -q -E "Guru Meditation|abort\(\)|assert failed" "$log" && break
    sleep 1
  done
  sleep 1
  kill "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
  rm -f "$flash"
  if grep -a -q -F "$expected" "$log" && ! grep -a -q -E "Guru Meditation|abort\(\)|assert failed" "$log"; then
    echo "[PASS] $name"
  else
    echo "[FAIL] $name (see $log)"
    failed=$((failed + 1))
  fi
done < "$here/expected.txt"

echo "ARDUINO EXAMPLES IN QEMU: $failed failed"
[ "$failed" -eq 0 ]
