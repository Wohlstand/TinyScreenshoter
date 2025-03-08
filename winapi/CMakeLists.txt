cmake_minimum_required(VERSION 3.5...3.10)

project(TinyScreenshoterWin LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "This project is targeted to Windows, especially Windows 9x")
endif()

set(CMAKE_C_STANDARD 90)

add_executable(TinyScreenshoterWin WIN32
    src/main.c
    src/shot_data.c src/shot_data.h
    src/shot_proc.c src/shot_proc.h
    src/tray_icon.c src/tray_icon.h
    src/shot_hooks.c src/shot_hooks.h
    src/settings.c src/settings.h
    src/misc.c src/misc.h
    res/tinyscreen.rc
    res/resource.h res/resource_ex.h

    ../lib/spng.c ../lib/spng.h
    ../lib/miniz.c ../lib/miniz.h
)

target_compile_definitions(TinyScreenshoterWin PRIVATE -DSPNG_STATIC -DSPNG_SSE=0 -DSPNG_USE_MINIZ)

if(NOT MSVC)
    target_compile_options(TinyScreenshoterWin PRIVATE -Wall)
endif()

target_include_directories(TinyScreenshoterWin PRIVATE src res ../lib)
target_link_libraries(TinyScreenshoterWin PRIVATE shlwapi comctl32 gdi32 user32)
target_link_options(TinyScreenshoterWin PRIVATE -static -static-libgcc)
