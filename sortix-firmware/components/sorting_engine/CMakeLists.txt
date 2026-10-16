idf_component_register(
    SRCS "sorting_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES encoder servo sensors state_machine nvs_config esp_timer json
)
