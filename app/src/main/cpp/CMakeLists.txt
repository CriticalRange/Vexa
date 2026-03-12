
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("vexa")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
    # List C/C++ source files with relative paths to this CMakeLists.txt.
    common/path_checks.cpp
    logging/native_log.cpp
    logging/crash_signals.cpp
    runtime/init/preflight.cpp
    runtime/init/logging.cpp
    runtime/init/config.cpp
    runtime/init/core.cpp
    runtime/init/thread.cpp
    runtime/init/thunk.cpp
    runtime/init/syscalls.cpp
    runtime/init/execute.cpp
    runtime/launch.cpp
    wrapper/jni.cpp
    main.cpp)

set(FEX_ROOT "/home/critical/FEX" CACHE PATH "Path to FEX source")
set(FEX_BUILD "/home/critical/FEX/build-android-arm64-ninja" CACHE PATH "Path to FEX build")

if(NOT EXISTS "${FEX_ROOT}/FEXCore/include/FEXCore/Core/Context.h")
    message(FATAL_ERROR "Invalid FEX_ROOT: ${FEX_ROOT}")
endif()
if(NOT EXISTS "${FEX_BUILD}/FEXCore/Source/libFEXCore.so")
    message(FATAL_ERROR "Invalid FEX_BUILD: ${FEX_BUILD}")
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/rapidjson/include
    ${FEX_ROOT}/FEXCore/include
    ${FEX_ROOT}/FEXHeaderUtils
    ${FEX_ROOT}/Source
    ${FEX_ROOT}/Source/Tools/CommonTools
    ${FEX_ROOT}/Source/Tools/LinuxEmulation
    ${FEX_ROOT}/External/fmt/include
    ${FEX_ROOT}/External/robin-map/include
    ${FEX_BUILD}/include)

add_library(FEXJemallocLibs STATIC IMPORTED)
set_target_properties(FEXJemallocLibs PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/FEXCore/Source/libJemallocLibs.a")
add_library(FEXFmt STATIC IMPORTED)
set_target_properties(FEXFmt PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/External/fmt/libfmt.a")
add_library(FEXXXHash STATIC IMPORTED)
set_target_properties(FEXXXHash PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/External/xxhash/cmake_unofficial/libxxhash.a")
add_library(FEXCephes STATIC IMPORTED)
set_target_properties(FEXCephes PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/External/cephes/libcephes_128bit.a")
add_library(FEXSoftFloat STATIC IMPORTED)
set_target_properties(FEXSoftFloat PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/External/SoftFloat-3e/libsoftfloat_3e.a")
add_library(FEXTinyJson STATIC IMPORTED)
set_target_properties(FEXTinyJson PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/External/tiny-json/libtiny-json.a")
add_library(FEXCommon STATIC IMPORTED)
set_target_properties(FEXCommon PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/Source/Common/libCommon.a")
add_library(FEXCommonTools STATIC IMPORTED)
set_target_properties(FEXCommonTools PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/Source/Tools/CommonTools/libCommonTools.a")
add_library(LinuxEmulation STATIC IMPORTED)
set_target_properties(LinuxEmulation PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/Source/Tools/LinuxEmulation/libLinuxEmulation.a")
add_library(FEXCore SHARED IMPORTED)
set_target_properties(FEXCore PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/FEXCore/Source/libFEXCore.so")
add_library(FEXCoreBase SHARED IMPORTED)
set_target_properties(FEXCoreBase PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/FEXCore/Source/libFEXCore_Base.a")
add_library(FEXCoreStatic SHARED IMPORTED)
set_target_properties(FEXCoreStatic PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/FEXCore/Source/libFEXCore.a")

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

target_compile_definitions(${CMAKE_PROJECT_NAME}
PRIVATE
ARCHITECTURE_arm64=1 # TODO: Implement 32bit if needed later with a check here
ASSERTIONS_ENABLED=1
)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
    # List libraries link to the target library
    android
          log
          FEXCoreStatic
          FEXCommon
          FEXJemallocLibs
          LinuxEmulation
          FEXCommonTools
          FEXTinyJson
          FEXCoreBase
          FEXFmt
          FEXXXHash
          FEXCephes
          FEXSoftFloat
          dl
          atomic
          m
    )
