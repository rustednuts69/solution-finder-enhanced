cmake_minimum_required(VERSION 3.20)

project(SolutionFinderEnhancedQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(solution-finder-enhanced-qt
    src/main.cpp
)

target_link_libraries(solution-finder-enhanced-qt PRIVATE Qt6::Widgets)

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS solution-finder-enhanced-qt
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
