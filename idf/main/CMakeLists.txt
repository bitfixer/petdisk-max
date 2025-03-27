FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.cpp)

set(components driver freertos hal fatfs esp_wifi nvs_flash esp_http_client console)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "."
                    REQUIRES ${components}
)
