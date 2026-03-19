cmake_minimum_required(VERSION 3.15)

project(ili9341_driver)


add_library(${PROJECT_NAME} STATIC
    "src/ili9341-driver.c"
    "src/ili9341-display.c"
    "src/ili9341-font.c"
    "src/font-jetbrains.c"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "include"
)
