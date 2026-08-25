cmake_minimum_required(VERSION 3.16)

project(countdown VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.2)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set_source_files_properties(qml/AppConfig.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_executable(appcountdown
    main.cpp
    resources/app.rc
)

qt_add_qml_module(appcountdown
    URI countdown
    VERSION 1.0
    QML_FILES
        main.qml
        qml/AppConfig.qml
        qml/StatusCard.qml
        qml/RoundedButton.qml
        qml/SoftMenuItem.qml
        qml/SettingsDialog.qml
    RESOURCES
        resources/mascot.png
        resources/mascot_rest.png
)

set_target_properties(appcountdown PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appcountdown
    PRIVATE Qt6::Quick Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS appcountdown
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
