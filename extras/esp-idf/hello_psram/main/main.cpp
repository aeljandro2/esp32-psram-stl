// ESP-IDF example: the same library, no Arduino.

#include <PsramStl.h>

#include <cstdio>

extern "C" void app_main() {
    if (!psram::available()) {
        std::printf("No PSRAM: enable CONFIG_SPIRAM in menuconfig.\n");
        return;
    }

    psram::map<int, psram::string> names;
    for (int i = 0; i < 1000; ++i) {
        names[i] = "device-" + psram::string(std::to_string(i).c_str()) + "-with-a-name-longer-than-sso";
    }

    psram::vector<double> history(200000, 1.0);  // 1.6 MB

    std::printf("%u names, history %u samples, in PSRAM: %s\n", static_cast<unsigned>(names.size()),
                static_cast<unsigned>(history.size()), psram::is_psram(history.data()) ? "yes" : "no");
    psram::report();
}
