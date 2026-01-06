cmake_minimum_required(VERSION 3.16)

# Choose which app to build
set(APP_NAME "evt_bus_test" CACHE STRING "Select which application to build")
message(STATUS "Building application: ${APP_NAME}")

# Paths
set(APP_PATH ${CMAKE_SOURCE_DIR}/apps/${APP_NAME})
set(CUSTOM_ROOT_PATH ${CMAKE_SOURCE_DIR})
message(STATUS "Root path: ${CUSTOM_ROOT_PATH}")

# Validate app folder
if(NOT EXISTS ${APP_PATH}/CMakeLists.txt)
    message(FATAL_ERROR "Invalid APP_NAME: ${APP_NAME}. Folder not found at ${APP_PATH}")
endif()

# Integrate Middlewares
# Use separate variables per app
# Declared here because it gives compilation errors when being included in apps folder CMakeLists.txt
set(APP_COMPONENTS_infrared_test "")
set(APP_COMPONENTS_evt_bus_test "${CUSTOM_ROOT_PATH}/externals/embedded_evt_bus/ports/esp-idf/evt_bus")
set(APP_COMPONENTS_system_demo "")


# Function to get components for app
function(get_app_components app_name out_var)
    # Construct the variable name dynamically
    set(var_name APP_COMPONENTS_${app_name})
    if(DEFINED ${var_name})
        set(components ${${var_name}})
        set(${out_var} ${components} PARENT_SCOPE)
        message(STATUS "Found special requirements for app ${app_name}: ${components}")
    else()
        message(WARNING "No special requirements defined for app ${app_name}")
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()


# Get middleware/components for this app
get_app_components(${APP_NAME} APP_REQUIREMENTS)

message(STATUS "App requirements: ${APP_REQUIREMENTS}")

# Include the app itself in EXTRA_COMPONENT_DIRS
set(EXTRA_COMPONENT_DIRS ${APP_PATH} ${APP_REQUIREMENTS})

message(STATUS "App - Extra component dirs: ${EXTRA_COMPONENT_DIRS}")

# Include ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fw)