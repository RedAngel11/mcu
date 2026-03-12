cmake_minimum_required(VERSION 3.15)

project(protocol)

add_library(${PROJECT_NAME} STATIC
	"src/protocol-task.c"
)

target_include_directories(${PROJECT_NAME} PUBLIC
	"include"
)
