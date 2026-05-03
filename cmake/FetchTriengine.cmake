if(NOT TARGET Triengine::triengine)
    message(STATUS "Fetching Triengine...")

    # NOTE: When using a shallow clone (GIT_SHALLOW TRUE), specify the GIT_TAG with a tag name (e.g., v1.2.3) rather than a full commit hash. 
    # A shallow clone is designed to pull only the tip of a named reference. 
    # Using a full commit hash with this option will likely fail for older commits, as the hash won't be found in the default branch's shallow history.
    FetchContent_Declare(
        Triengine
        GIT_REPOSITORY https://github.com/xorespesp/triengine.git
        GIT_TAG ad8d5a464305ca569551eae4feb3cda8d8280bda
        #GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
    )

    set(TRIENGINE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(TRIENGINE_BUILD_EXTERNAL_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(Triengine)

    message(STATUS "Triengine_SOURCE_DIR: ${Triengine_SOURCE_DIR}")
    message(STATUS "Triengine_BINARY_DIR: ${Triengine_BINARY_DIR}")
endif()