cmake_minimum_required(VERSION 3.20)

project(SolutionFinderEnhancedWindows VERSION 0.1.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.ico")
configure_file(resources/app-icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc" @ONLY)

qt_add_executable(solution-finder-enhanced WIN32
    ../qt-common/src/main.cpp
    ../qt-common/resources/resources.qrc
    ../../macos-native-gui/portable/game_core.c
    "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc"
)

target_link_libraries(solution-finder-enhanced PRIVATE Qt6::Widgets)
target_include_directories(solution-finder-enhanced PRIVATE
    ../../macos-native-gui/portable/include
)

include(CTest)
if(BUILD_TESTING)
    add_executable(game-core-orientation-tests
        ../../macos-native-gui/portable/tests/game_core_orientation_tests.c
        ../../macos-native-gui/portable/game_core.c
    )
    target_include_directories(game-core-orientation-tests PRIVATE
        ../../macos-native-gui/portable/include
    )
    add_test(NAME game-core-orientations COMMAND game-core-orientation-tests)

    add_executable(game-core-gravity-tests
        ../../macos-native-gui/portable/tests/game_core_gravity_tests.c
        ../../macos-native-gui/portable/game_core.c
    )
    target_include_directories(game-core-gravity-tests PRIVATE
        ../../macos-native-gui/portable/include
    )
    add_test(NAME game-core-gravity COMMAND game-core-gravity-tests)

    add_executable(game-core-lock-reset-tests
        ../../macos-native-gui/portable/tests/game_core_lock_reset_tests.c
        ../../macos-native-gui/portable/game_core.c
    )
    target_include_directories(game-core-lock-reset-tests PRIVATE
        ../../macos-native-gui/portable/include
    )
    add_test(NAME game-core-lock-reset COMMAND game-core-lock-reset-tests)

    add_executable(game-core-pc-scout-tests
        ../../macos-native-gui/portable/tests/game_core_pc_scout_tests.c
        ../../macos-native-gui/portable/game_core.c
    )
    target_include_directories(game-core-pc-scout-tests PRIVATE
        ../../macos-native-gui/portable/include
    )
    add_test(NAME game-core-pc-scout COMMAND game-core-pc-scout-tests)
endif()

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
