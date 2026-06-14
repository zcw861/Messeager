cmake_minimum_required(VERSION 4.3.2)

project(IPMSG VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Threads REQUIRED)

qt_standard_project_setup()

add_library(IPMSG STATIC
    privatechat.h
    privatechat.cpp
    translatefile.h
    translatefile.cpp
)

qt_add_qml_module(IPMSG
    URI "se.qt.ipmsg"
    VERSION 1.0
    SOURCES  privatechat.h privatechat.cpp

)

target_compile_features(IPMSG PRIVATE cxx_std_23)

target_include_directories(IPMSG
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# source_group("IPMSG", FILES  # 模块文件的分组
#    math.cppm
#)

target_link_libraries(IPMSG
    PUBLIC
        Qt6::Core
    PRIVATE
        # Qt6::Widgets
        Threads::Threads
)