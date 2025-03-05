cmake_minimum_required(VERSION 3.5...3.10)

project(TinyScreenshoterWin LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "This project is targeted to Windows, especially Windows 9x")
endif()

add_executable(TinyScreenshoterWin WIN32
    src/main.c
    src/shot_data.c src/shot_data.h
    src/shot_proc.c src/shot_proc.h
    src/tray_icon.c src/tray_icon.h
    src/shot_hooks.c src/shot_hooks.h
    res/tinyscreen.rc
)

target_include_directories(TinyScreenshoterWin PRIVATE src res ../lib)
target_link_libraries(TinyScreenshoterWin PRIVATE gdi32 user32)
target_link_options(TinyScreenshoterWin PRIVATE -static -static-libgcc)
