idf_component_register(SRCS "test_main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp32-psram-stl esp_psram freertos heap esp_hw_support)
