cmake_minimum_required(VERSION 3.20)
project(FilesystemSwap VERSION 1.0.0 LANGUAGES C)
include(CTest)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(infiltratr_common
    GIT_REPOSITORY https://github.com/Infiltrator-Projects/Libraries.git
    GIT_TAG 504f889875d8b7c48747db529c19258280175179)
FetchContent_MakeAvailable(infiltratr_common)
# SWAP.so links Common; Common's development files must not be installed.
set_property(DIRECTORY "${infiltratr_common_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.h" "#define LD_VERSION \"1.0.0\"\n")
add_library(swap-driver SHARED
    native/swap_native.c
    native/swap_worker.c
    module/swap_module.c)
set_source_files_properties(native/swap_worker.c
    PROPERTIES COMPILE_DEFINITIONS "main=ifs_swap_worker_main")
set_target_properties(swap-driver PROPERTIES PREFIX "" OUTPUT_NAME "SWAP" SUFFIX ".so")
target_compile_definitions(swap-driver PRIVATE _GNU_SOURCE _FILE_OFFSET_BITS=64)
target_include_directories(swap-driver PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/native"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(swap-driver PRIVATE InfiltratrCommon::Common)
if(BUILD_TESTING)
    add_executable(swap-native-test tests/test_swap_native.c)
    target_compile_definitions(swap-native-test PRIVATE _GNU_SOURCE _FILE_OFFSET_BITS=64)
    target_include_directories(swap-native-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/native")
    target_link_libraries(swap-native-test PRIVATE swap-driver)
    add_test(NAME swap-native-parsing COMMAND swap-native-test)
    add_executable(swap-module-test tests/test_swap_module.c)
    target_compile_definitions(swap-module-test PRIVATE _GNU_SOURCE _FILE_OFFSET_BITS=64)
    target_include_directories(swap-module-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(swap-module-test PRIVATE swap-driver InfiltratrCommon::Common)
    add_test(NAME swap-so-interface-and-format COMMAND swap-module-test)
endif()
# Generate the same versioned PDF as the standalone native source installer.
set(SWAP_GENERATED_PDF "${CMAKE_CURRENT_BINARY_DIR}/Filesystem-Swap.pdf")
add_custom_command(
 OUTPUT "${SWAP_GENERATED_PDF}"
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/standalone/generate_manual.py"
         "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
         "${CMAKE_CURRENT_SOURCE_DIR}/docs/Swap.pages"
         "${SWAP_GENERATED_PDF}"
 DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
         "${CMAKE_CURRENT_SOURCE_DIR}/docs/Swap.pages"
         "${CMAKE_CURRENT_SOURCE_DIR}/standalone/manual.py"
         "${CMAKE_CURRENT_SOURCE_DIR}/standalone/generate_manual.py"
 VERBATIM)
add_custom_target(swap-manual ALL DEPENDS "${SWAP_GENERATED_PDF}")
install(TARGETS swap-driver LIBRARY DESTINATION /System/Filesystems)
install(FILES "${SWAP_GENERATED_PDF}"
        DESTINATION /System/Resources/Documentation)
