cmake_minimum_required(VERSION 3.24)
project(Chromarchy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets)

qt_add_executable(chromarchy
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
)

target_link_libraries(chromarchy PRIVATE Qt6::Widgets)

install(TARGETS chromarchy RUNTIME DESTINATION bin)

