set(SOURCES "espHTTP.c")
set(pri_set esp_http_server)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    REQUIRES ${pri_set}
                    REQUIRES nvs_flash
                    REQUIRES json
                    REQUIRES espWiFi
                    REQUIRES project_system_files
                    REQUIRES espCLIENT
                    )
