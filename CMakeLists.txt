cmake_minimum_required(VERSION 3.24)
project(Chromarchy VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

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

if(BUILD_TESTING)
  find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

  qt_add_executable(chromarchy_metadata_tests
    tests/MetadataTest.cpp
  )
  target_link_libraries(chromarchy_metadata_tests PRIVATE Qt6::Test)
  target_compile_definitions(chromarchy_metadata_tests PRIVATE
    CHROMARCHY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  )

  add_test(NAME chromarchy_metadata COMMAND chromarchy_metadata_tests)
endif()

