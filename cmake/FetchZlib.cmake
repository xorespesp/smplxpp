if (NOT TARGET zlibstatic)
    message(STATUS "Fetching zlib...")

    FetchContent_Declare(zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
        GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
    )

    set(ZLIB_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(zlib)
endif()