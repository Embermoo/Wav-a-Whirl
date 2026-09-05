cmake_minimum_required(VERSION 3.16)
project(Main VERSION 1.1 LANGUAGES CXX)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Multimedia)
qt_add_executable(Main main.cpp mainwindow.cpp mainwindow.h mainwindow.ui audionormalizer.cpp audionormalizer.h)
target_link_libraries(Main PRIVATE Qt6::Widgets Qt6::Multimedia)
set_target_properties(Main PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
include(GNUInstallDirs)
install(TARGETS Main
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

option(WAV_BUILD_TESTS "Build playback regression checks" OFF)
if(WAV_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(playback_test tests/playback_test.cpp mainwindow.cpp mainwindow.h mainwindow.ui audionormalizer.cpp audionormalizer.h)
    target_include_directories(playback_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(playback_test PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Test)
    add_test(NAME playback_controls COMMAND playback_test -platform offscreen)
endif()
