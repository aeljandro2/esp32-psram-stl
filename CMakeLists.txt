# ESP-IDF component: header-only, so it only publishes the include path.
idf_component_register(INCLUDE_DIRS "src"
                       REQUIRES heap esp_hw_support log)
