cmake_minimum_required(VERSION 4.3.2)

function(_qt_add_executable target)
    qt_add_executable(${target})
endfunction()

project(IPMSG VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets CanvasPainter)
qt_standard_project_setup()

_qt_add_executable(IPMSG)

target_compile_features(IPMSG PRIVATE cxx_std_23)

target_sources(IPMSG
    PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES FILES
        # math.cppm           # 模块文件
    PRIVATE
        main.cpp
        privatechat.cpp
        privatechat.h
        main.qml
        translatefile.h translatefile.cpp
)

# source_group("IPMSG", FILES  # 模块文件的分组
#    math.cppm
#)

target_link_libraries(IPMSG
    PRIVATE
        Qt6::Widgets
        Qt6::CanvasPainter
)

set_target_properties(IPMSG PROPERTIES
    AUTORCC TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX "/opt/IPMSG" CACHE PATH "Install path prefix" FORCE)
install(TARGETS IPMSG
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET IPMSG
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
