# Wokwi (optional second simulator)

The device tests run in QEMU in CI. These files run the same firmware in
[Wokwi](https://wokwi.com) instead, if you want a second opinion.

1. Build the device tests (see [docs/testing.md](../../docs/testing.md)).
2. Get a CI token at https://wokwi.com/dashboard/ci and export it as `WOKWI_CLI_TOKEN`.
3. Run:

```bash
wokwi-cli --timeout 600000 --expect-text "ALL TESTS PASSED" --fail-text "TESTS FAILED" extras/wokwi/esp32
```

Use `extras/wokwi/esp32s3` for the ESP32-S3 (8 MB octal PSRAM).
