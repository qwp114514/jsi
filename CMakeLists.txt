cmake_minimum_required(VERSION 3.18)
project(aux CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fvisibility=hidden -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--gc-sections -Wl,-s")

if(NOT DEFINED IMGUI_DIR)
  set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
endif()
if(NOT DEFINED ZYGISK_DIR)
  set(ZYGISK_DIR "${CMAKE_SOURCE_DIR}/third_party")
endif()

add_library(aux SHARED
    src/main.cpp
    src/stealth.cpp
    src/touch.cpp
    src/input.cpp
    src/recoil.cpp
    src/loader.cpp
    src/game.cpp
    src/draw.cpp
    src/menu.cpp
    src/config.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(aux PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ZYGISK_DIR}
)

target_compile_definitions(aux PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    AUX_VERSION=\"1.0.4\"
)

target_link_libraries(aux
    log
    android
    EGL
    GLESv3
    GLESv2
    dl
    m
)
