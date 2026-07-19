cmake_minimum_required(VERSION 3.20)

project(SolutionFinderEnhancedQt VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(solution-finder-enhanced-qt
    ../qt-common/src/main.cpp
    ../qt-common/resources/resources.qrc
    ../../macos-native-gui/portable/game_core.c
)

target_link_libraries(solution-finder-enhanced-qt PRIVATE Qt6::Widgets)
target_include_directories(solution-finder-enhanced-qt PRIVATE
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

    add_executable(game-core-scoring-tests
        ../../macos-native-gui/portable/tests/game_core_scoring_tests.c
        ../../macos-native-gui/portable/game_core.c
    )
    target_include_directories(game-core-scoring-tests PRIVATE
        ../../macos-native-gui/portable/include
    )
    add_test(NAME game-core-scoring COMMAND game-core-scoring-tests)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS solution-finder-enhanced-qt
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY "${REPO_ROOT}/shared"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/solution-finder-enhanced"
    )
    install(FILES
        "${REPO_ROOT}/solution-finder-1.43/sfinder.jar"
        "${REPO_ROOT}/solution-finder-1.43/README.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/solution-finder-enhanced/solution-finder-1.43"
    )
    install(DIRECTORY
        "${REPO_ROOT}/solution-finder-1.43/kicks"
        "${REPO_ROOT}/solution-finder-1.43/theme"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/solution-finder-enhanced/solution-finder-1.43"
    )
    if(EXISTS "${REPO_ROOT}/native-linux")
        install(DIRECTORY "${REPO_ROOT}/native-linux"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/solution-finder-enhanced"
            USE_SOURCE_PERMISSIONS
        )
    endif()
endif()
