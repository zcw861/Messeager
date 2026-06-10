cmake_minimum_required(VERSION 4.3.2)

function(_qt_add_executable target)
    qt_add_executable(${target})
endfunction()

project(Messager VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets CanvasPainter)
qt_standard_project_setup()

_qt_add_executable(Messager)

target_compile_features(Messager PRIVATE cxx_std_23)

target_sources(Messager
    PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES FILES
        # math.cppm           # 模块文件
    PRIVATE
        main.cpp
        p2p.cpp
        p2p.h
)

# source_group("Messager", FILES  # 模块文件的分组
#    math.cppm
#)

target_link_libraries(Messager
    PRIVATE
        Qt6::Widgets
        Qt6::CanvasPainter
)

set_target_properties(Messager PROPERTIES
    AUTORCC TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX "/opt/Messager" CACHE PATH "Install path prefix" FORCE)
install(TARGETS Messager
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET Messager
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
