cmake_minimum_required(VERSION 4.3.2)

project(Database VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql)
qt_standard_project_setup()

include(GNUInstallDirs)

add_library(Database SHARED
    database_global.h
    databasecore.h databasecore.cpp
    databasecheck.h databasecheck.cpp
    peerdatabase.h peerdatabase.cpp
    privatechatdatabase.h privatechatdatabase.cpp
    groupchatdatabase.h groupchatdatabase.cpp
)

target_compile_features(Database PRIVATE cxx_std_23)

target_include_directories(Database
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Messager/Database>
)

target_link_libraries(Database
    PUBLIC
        Qt6::Core
        Qt6::Sql
)

target_compile_definitions(Database
    PRIVATE
        DATABASE_LIBRARY
)

install(TARGETS Database
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
