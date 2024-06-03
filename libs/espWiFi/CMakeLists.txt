set(SOURCES "espWiFi.c")
set(pri_set esp_wifi)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    REQUIRES ${pri_set}
                    REQUIRES nvs_flash
                    REQUIRES json
                    REQUIRES espHTTP
                    REQUIRES espFILE
                    REQUIRES espMQTT
                    )

