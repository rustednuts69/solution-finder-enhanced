cmake_minimum_required(VERSION 3.20)

project(SolutionFinderEnhancedWindows VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(solution-finder-enhanced WIN32
    ../qt-common/src/main.cpp
)

target_link_libraries(solution-finder-enhanced PRIVATE Qt6::Widgets)

add_custom_command(TARGET solution-finder-enhanced POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${REPO_ROOT}/shared"
        "$<TARGET_FILE_DIR:solution-finder-enhanced>/shared"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${REPO_ROOT}/solution-finder-1.43"
        "$<TARGET_FILE_DIR:solution-finder-enhanced>/solution-finder-1.43"
    COMMENT "Copying Solution Finder Enhanced runtime files"
)

install(TARGETS solution-finder-enhanced RUNTIME DESTINATION .)
install(DIRECTORY "${REPO_ROOT}/shared" DESTINATION .)
install(DIRECTORY "${REPO_ROOT}/solution-finder-1.43" DESTINATION .)
