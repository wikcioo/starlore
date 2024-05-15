#pragma once

#define CLIENT_VERSION_MAJOR 0
#define CLIENT_VERSION_MINOR 1
#define CLIENT_VERSION_PATCH 0

#if defined(DEBUG)
    #define BUILD_MODE_STR "debug"
#else
    #define BUILD_MODE_STR "release"
#endif

#define BUILD_VERSION(major,minor,patch) BUILD_MODE_STR" v"STRINGIFY(major)"."STRINGIFY(minor)"."STRINGIFY(patch)
