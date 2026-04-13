idf_component_register(
    SRCS "pokemon_viewer.c" "png_decoder.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lcd_st7789 sd_card wifi_manager esp_wifi nvs_flash esp_http_client mbedtls lodepng
)
