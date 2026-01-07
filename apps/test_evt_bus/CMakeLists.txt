set(srcs "test_evt_bus_main.c")


message(STATUS "Extra component dirs: ${EXTRA_COMPONENT_DIRS}")
message(STATUS "Source dir:" ${CMAKE_SOURCE_DIR})
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/managed_components/node_dht")
message(STATUS "Extra component dirs AFTER: ${EXTRA_COMPONENT_DIRS}")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       # Add ESP_IDF libraries here as needed
                       REQUIRES evt_bus unity
                       WHOLE_ARCHIVE
                    )