# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DSOCKLEN_T=socklen_t)
add_definitions(-DNO_SSTREAM=1)
add_definitions(-DNEWLOCALE_NOT_USED)
#add_definitions(-D_LARGEFILE_SOURCE=1)
#add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-DBSD=1)

ADD_SUBDIRECTORY(src/main/cpp/BasicUsageEnvironment)
ADD_SUBDIRECTORY(src/main/cpp/groupsock)
ADD_SUBDIRECTORY(src/main/cpp/liveMedia)
ADD_SUBDIRECTORY(src/main/cpp/UsageEnvironment)
include_directories(src/main/cpp/BasicUsageEnvironment/include)
include_directories(src/main/cpp/groupsock/include)
include_directories(src/main/cpp/liveMedia/include)
include_directories(src/main/cpp/UsageEnvironment/include)
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
#file(GLOB BasicUsageEnvironmentFiles
#        src/main/cpp/live/BasicUsageEnvironment/*.cpp)
#
#add_library(BasicUsageEnvironment
#        ${BasicUsageEnvironmentFiles})
#
#file(GLOB groupsockfiles
#        src/main/cpp/live/groupsock/*.cpp src/main/cpp/live/groupsock/*.c)
#add_library(groupsock ${groupsockfiles})
#
#file(GLOB liveMediaFiles
#        src/main/cpp/live/liveMedia/*.hh src/main/cpp/live/liveMedia/*.cpp src/main/cpp/live/liveMedia/*.c)
#add_library(liveMedia ${liveMediaFiles})
#
#file(GLOB UsageEnvironmentFiles
#        src/main/cpp/live/UsageEnvironment/*.cpp)
#add_library(UsageEnvironment ${UsageEnvironmentFiles})

add_library( # Sets the name of the library.
             live555-rtsp
             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/cpp/live555-rtsp.cpp
              )
#add_library( # Sets the name of the library.
#        native-lib
#        # Sets the library as a shared library.
#        SHARED
#
#        # Provides a relative path to your source file(s).
#        src/main/cpp/native-lib.cpp
#        )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib


              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(
#        native-lib
        live555-rtsp
        ${log-lib}
        liveMedia
        groupsock
        BasicUsageEnvironment
        UsageEnvironment )

