
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
    logging/native_log.cpp
    runtime/preflight.cpp
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
    ${FEX_ROOT}/External/fmt/include
    ${FEX_BUILD}/include)

add_library(FEXCore SHARED IMPORTED)
set_target_properties(FEXCore PROPERTIES IMPORTED_LOCATION "${FEX_BUILD}/FEXCore/Source/libFEXCore.so")

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
    # List libraries link to the target library
    android
    log
    FEXCore)
