cmake_minimum_required(VERSION 3.16)

project(Aoe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.9 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Multimedia
)

# All project sources live in the top-level directory. Deliberately NOT recursive:
# GLOB_RECURSE would also pick up nested copies of the project (e.g. an archive
# unpacked one level too deep), which silently adds a second UsrAI.cpp/main.cpp to
# the target and breaks the link with duplicate symbols.
# CONFIGURE_DEPENDS makes CMake re-run the glob at build time, so adding or removing
# a source file no longer requires a manual re-configure.
file(GLOB AOE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
)

add_executable(aoe ${AOE_SOURCES})

# Keep the executable location stable so launch.json never has to guess it.
set_target_properties(aoe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(aoe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(aoe PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
)


if(MSVC)
    target_compile_options(aoe PRIVATE /utf-8)
endif()
if(WIN32)
    target_compile_definitions(aoe PRIVATE UNICODE _UNICODE)
endif()

# 免安装打包: 把 Qt 运行库、MSVC 运行库和运行期数据文件全部搬到 exe 旁边,
# 让输出目录本身就是可以整个拷走、双击运行的成品. 只有 release 预设会打开这个开关.
option(AOE_DEPLOY "Copy Qt/MSVC runtime and data files next to the executable" OFF)

if(AOE_DEPLOY AND WIN32)
    # config.json / res.rcc / 地图都是按当前工作目录打开的(见 GlobalVariate.cpp 的 ReadConfig
    # 和 MainWidget.cpp 的 registerResource), 双击 exe 时工作目录就是 exe 所在目录, 所以必须同级。
    # res.rcc 之类的资源不在版本库里(见 .gitignore), 缺文件时 copy_if_different 会直接让构建失败,
    # 所以只拷配置阶段确实存在的。改动了这些文件的有无需要重新配置一次。
    set(AOE_RUNTIME_FILES res.rcc config.json map.njust map1.njust map2.njust)
    foreach(f IN LISTS AOE_RUNTIME_FILES)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
            add_custom_command(TARGET aoe POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${CMAKE_CURRENT_SOURCE_DIR}/${f}" "$<TARGET_FILE_DIR:aoe>/${f}"
                VERBATIM
            )
        else()
            message(STATUS "AOE_DEPLOY: ${f} not found, skipped.")
        endif()
    endforeach()

    # MSVC 运行库: 取当前这套编译器自带的 redist, 不写死版本号
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    foreach(dll IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        add_custom_command(TARGET aoe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${dll}" "$<TARGET_FILE_DIR:aoe>"
            VERBATIM
        )
    endforeach()

    # Qt 运行库和插件交给 windeployqt: 平台插件 qwindows.dll 缺了程序直接起不来,
    # 而它在 plugins/platforms 下, 不在 bin 里, 手抄容易漏。
    get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    find_program(AOE_WINDEPLOYQT windeployqt HINTS "${_qt_bin}")

    if(AOE_WINDEPLOYQT)
        # --no-compiler-runtime: CRT 上面已经从编译器的 redist 拿过了, windeployqt 找的是
        # Qt 自带的那份, 这个 Qt 装的时候不一定带, 让它去找会直接报错中断构建。
        add_custom_command(TARGET aoe POST_BUILD
            COMMAND "${AOE_WINDEPLOYQT}" --release --no-compiler-runtime "$<TARGET_FILE:aoe>"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found next to ${_qt_qmake}; the executable will not run outside a Qt-aware shell.")
    endif()
endif()
