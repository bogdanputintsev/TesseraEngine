#pragma once

#ifndef NDEBUG
    #define IN_DEBUG_MODE 1
#endif

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) 
    #define WITH_WINDOWS_PLATFORM 1
    #ifndef _WIN64
        #error "64-bit is required on Windows!"
    #endif
#elif defined(__linux__) || defined(__gnu_linux__)
    #define WITH_LINUX_PLATFORM 1
    #if defined(__ANDROID__)
        #define WITH_ANDROID_PLATFORM 1
    #endif
    #error "Linux platform is not supported"
#elif defined(__unix__)
    #define WITH_UNIX_PLATFORM 1
    #error "Unix platform is not supported"
#elif defined(_POSIX_VERSION)
    // Posix
    #define WITH_POSIX_PLATFORM 1
    #error "Posix platform is not supported"
#elif __APPLE__
    // Apple platforms
    #define WITH_APPLE_PLATFORM 1
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        // iOS Simulator
        #define WITH_IOS_PLATFORM 1
        #define WITH_IOS_SIMULATOR_PLATFORM 1
    #elif TARGET_OS_IPHONE
        #define WITH_IOS_PLATFORM 1
        // iOS device
    #elif TARGET_OS_MAC
        // Other kinds of Mac OS
    #else
        #error "Unknown Apple platform"
    #endif
    #error "iOS platform is not supported"
#else
    #error "Unknown platform"
#endif

// Get filename macro
#if WITH_WINDOWS_PLATFORM
    #define GET_FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
    #define GET_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
