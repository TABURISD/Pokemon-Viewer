idf_component_register(SRCS "app_core.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lcd_st7789 pokemon_viewer wifi_manager nvs_flash)
