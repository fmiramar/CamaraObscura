cmake_minimum_required(VERSION 3.15)

project(CamaraObscura VERSION 0.1.0 LANGUAGES CXX)

option(SCSYNTH "Build the scsynth plugin target" ON)
option(SUPERNOVA "Build the supernova plugin target" OFF)
option(STRICT "Enable strict compiler warnings" OFF)
option(CREATIVE_REVERBS_SANITIZE
    "Enable address and undefined-behavior sanitizers for tests" OFF)
option(CREATIVE_REVERBS_BUILD_BENCHMARKS
    "Build the native DSP benchmark" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/stage"
        CACHE PATH "Install prefix" FORCE)
endif()

if(NOT SC_PATH)
    message(FATAL_ERROR
        "SC_PATH must point to a matching SuperCollider source tree")
endif()

get_filename_component(SC_PATH_FOR_MODULES "${SC_PATH}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH
    "${SC_PATH_FOR_MODULES}/tools/cmake_gen")

include(SuperColliderServerPlugin)
sc_do_initial_compiler_config()
sc_check_sc_path("${SC_PATH}")

set(creative_reverbs_cpp_files
    plugins/CamaraObscura.cpp
    plugins/DarkVelvetReverb.cpp
    plugins/GroupedFDN.cpp
    plugins/VelvetFDN.cpp
    plugins/RIRFDN.cpp
    plugins/ModalReverbBank.cpp
    plugins/ModalPlate.cpp
    plugins/GeometryReverb.cpp
)

file(GLOB creative_reverbs_sc_files CONFIGURE_DEPENDS
    Classes/*.sc)
file(GLOB creative_reverbs_help_files CONFIGURE_DEPENDS
    HelpSource/Classes/*.schelp)

sc_add_server_plugin(
    "CamaraObscura"
    "CamaraObscura"
    "${creative_reverbs_cpp_files}"
    "${creative_reverbs_sc_files}"
    "${creative_reverbs_help_files}"
)

foreach(target CamaraObscura_scsynth CamaraObscura_supernova)
    if(TARGET ${target})
        target_include_directories(${target} SYSTEM PRIVATE
            "${SC_PATH_FOR_MODULES}/include/plugin_interface"
            "${SC_PATH_FOR_MODULES}/include/common"
            "${SC_PATH_FOR_MODULES}/common")
        target_include_directories(${target} PRIVATE
            include plugins)
        if(STRICT)
            if(MSVC)
                target_compile_options(${target} PRIVATE
                    /W4 /permissive-)
            else()
                target_compile_options(${target} PRIVATE
                    -Wall -Wextra -Wpedantic -Wshadow
                    -Wno-unused-parameter)
            endif()
        endif()
    endif()
endforeach()

if(TARGET CamaraObscura_scsynth)
    set_target_properties(CamaraObscura_scsynth
        PROPERTIES OUTPUT_NAME "CamaraObscura")
    if(MINGW)
        target_link_options(CamaraObscura_scsynth PRIVATE
            -static-libgcc -static-libstdc++)
    endif()
endif()

if(TARGET CamaraObscura_supernova)
    set_target_properties(CamaraObscura_supernova
        PROPERTIES OUTPUT_NAME "CamaraObscura_supernova")
endif()

install(FILES
    README.md CHANGELOG.md LICENSE ORIGINS.md
    THIRD_PARTY_NOTICES.md
    DESTINATION CamaraObscura)
install(DIRECTORY docs/ DESTINATION CamaraObscura/docs)
install(DIRECTORY examples/ DESTINATION CamaraObscura/examples)
install(DIRECTORY tools/ DESTINATION CamaraObscura/tools
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE)
install(DIRECTORY benchmarks/ DESTINATION CamaraObscura/benchmarks)
install(FILES HelpSource/Guides/CamaraObscura.schelp
    DESTINATION CamaraObscura/HelpSource/Guides)
file(GLOB guide_images HelpSource/Guides/*.png)
if(guide_images)
    install(FILES ${guide_images} DESTINATION CamaraObscura/HelpSource/Guides)
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(creative_reverbs_reference_tests
        tests/cpp/CamaraObscuraTests.cpp)
    target_include_directories(
        creative_reverbs_reference_tests PRIVATE include)
    if(STRICT)
        if(MSVC)
            target_compile_options(
                creative_reverbs_reference_tests PRIVATE
                /W4 /permissive-)
        else()
            target_compile_options(
                creative_reverbs_reference_tests PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion
                -Wshadow -Werror)
        endif()
    endif()
    if(CREATIVE_REVERBS_SANITIZE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(
                creative_reverbs_reference_tests PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer)
            target_link_options(
                creative_reverbs_reference_tests PRIVATE
                -fsanitize=address,undefined)
        else()
            message(FATAL_ERROR
                "CREATIVE_REVERBS_SANITIZE requires Clang or GCC")
        endif()
    endif()
    add_test(NAME creative_reverbs_reference_tests
        COMMAND creative_reverbs_reference_tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME creative_reverbs_offline_tools
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/test_offline_tools.py")
    endif()
endif()

if(CREATIVE_REVERBS_BUILD_BENCHMARKS
    AND NOT CMAKE_CROSSCOMPILING)
    add_executable(creative_reverbs_dsp_benchmark
        benchmarks/dsp_benchmark.cpp)
    target_include_directories(
        creative_reverbs_dsp_benchmark PRIVATE include)
endif()
