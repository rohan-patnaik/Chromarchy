cmake_minimum_required(VERSION 3.24)
project(Chromarchy VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Gui Widgets)

qt_add_library(chromarchy_core STATIC
  src/core/Document.cpp
  src/core/Document.h
  src/core/DocumentHistory.cpp
  src/core/DocumentHistory.h
  src/core/ImageIO.cpp
  src/core/ImageIO.h
  src/core/Layer.cpp
  src/core/Layer.h
  src/core/NativeDocumentCodec.cpp
  src/core/NativeDocumentCodec.h
  src/core/TiledImage.cpp
  src/core/TiledImage.h
)
target_link_libraries(chromarchy_core PUBLIC Qt6::Gui)
target_include_directories(chromarchy_core PUBLIC src)

qt_add_executable(chromarchy
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
  src/ui/CanvasWidget.cpp
  src/ui/CanvasWidget.h
  src/ui/DocumentView.cpp
  src/ui/DocumentView.h
)

target_link_libraries(chromarchy PRIVATE chromarchy_core Qt6::Widgets)

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

  qt_add_executable(chromarchy_document_tests tests/core/DocumentTest.cpp)
  target_link_libraries(chromarchy_document_tests PRIVATE chromarchy_core Qt6::Test)
  add_test(NAME chromarchy_document COMMAND chromarchy_document_tests)

  qt_add_executable(chromarchy_tiled_image_tests tests/core/TiledImageTest.cpp)
  target_link_libraries(chromarchy_tiled_image_tests PRIVATE chromarchy_core Qt6::Test)
  add_test(NAME chromarchy_tiled_image COMMAND chromarchy_tiled_image_tests)

  qt_add_executable(chromarchy_image_io_tests tests/core/ImageIOTest.cpp)
  target_link_libraries(chromarchy_image_io_tests PRIVATE chromarchy_core Qt6::Test)
  add_test(NAME chromarchy_image_io COMMAND chromarchy_image_io_tests)

  qt_add_executable(chromarchy_native_document_tests
    tests/core/NativeDocumentCodecTest.cpp
  )
  target_link_libraries(chromarchy_native_document_tests PRIVATE
    chromarchy_core Qt6::Test
  )
  add_test(NAME chromarchy_native_document COMMAND chromarchy_native_document_tests)

  qt_add_executable(chromarchy_history_tests tests/core/DocumentHistoryTest.cpp)
  target_link_libraries(chromarchy_history_tests PRIVATE chromarchy_core Qt6::Test)
  add_test(NAME chromarchy_history COMMAND chromarchy_history_tests)

  qt_add_executable(chromarchy_canvas_tests
    tests/ui/CanvasWidgetTest.cpp
    src/ui/CanvasWidget.cpp
    src/ui/CanvasWidget.h
  )
  target_link_libraries(chromarchy_canvas_tests PRIVATE
    chromarchy_core Qt6::Test Qt6::Widgets
  )
  add_test(NAME chromarchy_canvas COMMAND chromarchy_canvas_tests)
  set_tests_properties(chromarchy_canvas PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endif()

