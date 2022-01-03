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
