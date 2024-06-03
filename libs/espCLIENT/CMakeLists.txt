set(SOURCES "espCLIENT.c")
set(pri_set esp_http_client)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    REQUIRES ${pri_set}
                    REQUIRES espWiFi
                    REQUIRES espHTTP
                    REQUIRES espMQTT
                    REQUIRES json
)
