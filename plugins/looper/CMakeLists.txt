INCLUDE(BuildPlugin)

BUILD_PLUGIN(looper
    looper.cpp
    looper.h
    MidiConnectionDialog.h
    MidiConnectionDialog.cpp

    MOCFILES
    looper.h
    MidiConnectionDialog.h

    EMBEDDED_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
)

set(CMAKE_BUILD_TYPE "Debug")

set (BuildOptionsDebug
    "-O0 -g3 -ggdb -Wall -Wpointer-arith" CACHE STRING "Debug build flags")