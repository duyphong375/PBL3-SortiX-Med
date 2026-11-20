idf_component_register(
    SRCS "sorting_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES encoder servo sensors motor_control state_machine nvs_config nvs_flash esp_timer json
)
