cmake_minimum_required(VERSION 4.2.3)
# set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
# cmake_minimum_required(VERSION 4.3.2)
# set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
# set(CMAKE_CXX_MODULE_STD ON)

project(Messager VERSION 0.1 LANGUAGES CXX)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Sql Qml QuickDialogs2)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(appMessager  WIN32)

set_source_files_properties(
    ../source/file.svg
    PROPERTIES
        QT_RESOURCE_ALIAS source/file.svg
)

qt_add_qml_module(appMessager
    URI "se.qt.messager"
    VERSION 1.0
    SOURCES     main.cpp
                appcontroller.h appcontroller.cpp
    QML_FILES
                PeerPanel.qml
                ChatPanel.qml
                Window.qml
                InputPanel.qml
                InviteUserInterface.qml
                Login.qml
    RESOURCES
                ../source/file.svg

                ../source/close.svg
                ../source/smaller.svg
                ../source/bigger.svg
                ../source/bigger2.svg
)
target_compile_features(appMessager PRIVATE cxx_std_23)
target_link_libraries(appMessager
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickDialogs2
        Database
        IPMSG
)

set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "Install path prefix" FORCE)
install(TARGETS appMessager
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# C++ Modules support
set(MESSAGER_MODULE_FILES
    # math.cppm                             # 模块文件
)
source_group("Messager" FILES ${MESSAGER_MODULE_FILES})
target_sources(appMessager
    PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS   ${CMAKE_CURRENT_SOURCE_DIR}
        FILES  ${MESSAGER_MODULE_FILES}
)
