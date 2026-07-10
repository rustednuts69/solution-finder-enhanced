cmake_minimum_required(VERSION 3.20)

project(SolutionFinderEnhancedQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

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
