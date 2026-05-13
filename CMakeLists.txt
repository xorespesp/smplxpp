########################################
##### Top-level CMake project file #####
########################################

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

########################################################################################
# Library Root Includes / Policies
########################################################################################

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Use standard install directory variables
#   - CMAKE_INSTALL_LIBDIR
#   - CMAKE_INSTALL_BINDIR
#   - CMAKE_INSTALL_INCLUDEDIR
include(GNUInstallDirs)

# Include module with functions:
# Generates and installs the files needed for find_package(${LIBRARY_NAME}) to work.
#   - write_basic_package_version_file(...)
#   - configure_package_config_file(...)
include(CMakePackageConfigHelpers)

include(FetchContent)

########################################################################################
# Library Variables
########################################################################################

# Root project name
set(SMPLXPP_PROJECT_NAME Smplxpp)

# Set SMPLXPP_PROJECT_NAME_UPPERCASE and SMPLXPP_PROJECT_NAME_LOWERCASE variables
string(TOUPPER ${SMPLXPP_PROJECT_NAME} SMPLXPP_PROJECT_NAME_UPPERCASE)
string(TOLOWER ${SMPLXPP_PROJECT_NAME} SMPLXPP_PROJECT_NAME_LOWERCASE)

# Library name (by default is the project name)
if(NOT SMPLXPP_LIBRARY_NAME)
  set(SMPLXPP_LIBRARY_NAME ${SMPLXPP_PROJECT_NAME_LOWERCASE})
endif()

# 라이브러리 export 시 install(TARGETS ...)와 install(EXPORT ...)에서 사용할 EXPORT 이름. 
# (둘이 반드시 같아야 함)
set(SMPLXPP_TARGETS_EXPORT_NAME "${SMPLXPP_PROJECT_NAME}TargetsExport")

# 라이브러리 export 시 생성된 config 파일들이 install될 경로.
# This works for all platforms:
#   - <prefix>/lib*/cmake/<PROJECT-NAME>
#   - <prefix>/lib*/
#   - <prefix>/include/
set(SMPLXPP_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${SMPLXPP_PROJECT_NAME}")

# Use `SMPLXPP_PROJECT_ROOT_DIR` instead of `CMAKE_SOURCE_DIR`, due to avoid FetchContent issue.
# (see: https://stackoverflow.com/q/76539375)
set(SMPLXPP_PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

########################################################################################
# Library Configure Options

option(SMPLXPP_BUILD_EXAMPLES "Build examples" ON)
option(SMPLXPP_BUILD_SHARED_LIB "Build shared library" OFF)
option(SMPLXPP_INSTALL_MODEL_FILES "Install model files along with the library" ON)
option(SMPLXPP_USE_CUDA "Use cuda if available" ON)

message(STATUS "--------------------------------------------------")
message(STATUS "SMPLXPP_LIBRARY_NAME: ${SMPLXPP_LIBRARY_NAME}")
message(STATUS "SMPLXPP_TARGETS_EXPORT_NAME: ${SMPLXPP_TARGETS_EXPORT_NAME}")
message(STATUS "SMPLXPP_CONFIG_INSTALL_DIR: ${SMPLXPP_CONFIG_INSTALL_DIR}")
message(STATUS "SMPLXPP_PROJECT_ROOT_DIR: ${SMPLXPP_PROJECT_ROOT_DIR}")
message(STATUS "SMPLXPP_BUILD_EXAMPLES: ${SMPLXPP_BUILD_EXAMPLES}")
message(STATUS "SMPLXPP_BUILD_SHARED_LIB: ${SMPLXPP_BUILD_SHARED_LIB}")
message(STATUS "SMPLXPP_INSTALL_MODEL_FILES: ${SMPLXPP_INSTALL_MODEL_FILES}")
message(STATUS "--------------------------------------------------")

########################################################################################
# Library Root Project Setup
########################################################################################

project(${SMPLXPP_PROJECT_NAME} 
    VERSION 0.0.1 
    LANGUAGES C CXX
    DESCRIPTION "SMPLXpp Library"
)

# Standard C++ Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Make sure different configurations don't collide
set(CMAKE_DEBUG_POSTFIX "d")

# --- Default Install Prefix Handling (Optional but recommended) ---
# Sets a default installation path if the user doesn't specify one via -DCMAKE_INSTALL_PREFIX
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMAKE_INSTALL_PREFIX has been not set
    message(WARNING "Overriding CMAKE_INSTALL_PREFIX to: ${SMPLXPP_PROJECT_ROOT_DIR}/install")
    message(WARNING "(Original CMAKE_INSTALL_PREFIX was: ${CMAKE_INSTALL_PREFIX})")
    set(CMAKE_INSTALL_PREFIX
        "${SMPLXPP_PROJECT_ROOT_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
endif()
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Install directories: LIB=${CMAKE_INSTALL_LIBDIR}, INCLUDE=${CMAKE_INSTALL_INCLUDEDIR}, BIN=${CMAKE_INSTALL_BINDIR}, CMAKE=${CMAKE_INSTALL_DATADIR}/cmake")

# where to find our CMake modules
# https://cmake.org/cmake/help/latest/variable/CMAKE_MODULE_PATH.html
list(APPEND CMAKE_MODULE_PATH "${SMPLXPP_PROJECT_ROOT_DIR}/cmake")

# where to find LibTorch CMake modules
# Ref: https://pytorch.org/cppdocs/installing.html#minimal-example
list(APPEND CMAKE_PREFIX_PATH "${SMPLXPP_LIBTORCH_INSTALL_DIR}")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

########################################################################################
# Library Root Compile Options
########################################################################################

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_ID MATCHES "Clang")
    # Enable all warnings and treat them as errors
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (MSVC)
    # Guard MSVC `/X` switches to C/CXX only (NVCC misreads them as input paths).
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W4>)

    message(STATUS "Suppress MSVC warnings C4819 C4127 globally...")
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4819>
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4127>
    )

    message(STATUS "Enable MSVC /utf-8 switch globally...")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")

    # Ref: https://devblogs.microsoft.com/cppblog/cpp-dynamic-debugging-full-debuggability-for-optimized-builds/#other-build-systems
    #message(STATUS "Enable MSVC dynamic-debugging globally...")

    # Pass /dynamicdeopt switch to cl.exe (requires /Zi or /Z7)
    add_compile_options(
        "$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:/dynamicdeopt>"
        "$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:/Zi>"
    )

    # Pass /dynamicdeopt switch to link.exe (requires /DEBUG:FULL)
    add_link_options(
        "$<$<NOT:$<CONFIG:Debug>>:/dynamicdeopt>"
        "$<$<NOT:$<CONFIG:Debug>>:/DEBUG:FULL>"
    )

    # Pass /dynamicdeopt switch to lib.exe (release only)
    string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELEASE " /dynamicdeopt") # Release
    string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO " /dynamicdeopt") # RelWithDebInfo

endif()

########################################################################################
# CUDA Setup
########################################################################################

set(SMPLXPP_CUDA_ENABLED OFF)
if (SMPLXPP_USE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)

    if (CMAKE_CUDA_COMPILER)
        # CUDA C++ standard (requires CUDA Toolkit 11.0+)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)

        # GPU arch: 'native' auto-detects the compiling host's GPU (CMake 3.24+).
        # Override at configure time via -DCMAKE_CUDA_ARCHITECTURES=...
        # (e.g. "75;86;89" for portable builds across Turing/Ampere/Ada).
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
            set(CMAKE_CUDA_ARCHITECTURES native)
        endif()

        enable_language(CUDA)

        set(SMPLXPP_CUDA_ENABLED ON) # version.hh #cmakedefine flag

        # Allow constexpr host functions on device side (useful for Eigen templates)
        add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>)

        message(STATUS "smplxpp: CUDA enabled (arch: ${CMAKE_CUDA_ARCHITECTURES})")
    else()
        message(STATUS "smplxpp: CUDA compiler not found, CUDA disabled")
    endif()
else()
    message(STATUS "smplxpp: CUDA disabled")
endif()


########################################################################################
# Library Source Files
########################################################################################

set(SMPLXPP_LIBRARY_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SMPLXPP_LIBRARY_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Generate version.hh
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.hh.in" # template file path
    "${CMAKE_CURRENT_BINARY_DIR}/include/smplx/version.hh" # output file path
)

# library sources
set(SMPLXPP_LIBRARY_SOURCES
    "${SMPLXPP_LIBRARY_SOURCES_DIR}/body.cc"
    "${SMPLXPP_LIBRARY_SOURCES_DIR}/model.cc"
    "${SMPLXPP_LIBRARY_SOURCES_DIR}/util_cbor.cc"
)

# library sources (CUDA)
set(SMPLXPP_LIBRARY_CUDA_SOURCES
    "${SMPLXPP_LIBRARY_SOURCES_DIR}/cuda/body.cu"
    "${SMPLXPP_LIBRARY_SOURCES_DIR}/cuda/model.cu"
)

# library headers (public)
set(SMPLXPP_LIBRARY_HEADERS_PUBLIC
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/defs.hh"
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/model_config.hh"
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/smplx.hh"
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/util.hh"
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/util_cbor.hh"
)

# library headers (CUDA, private)
set(SMPLXPP_LIBRARY_CUDA_HEADERS_PRIVATE
    "${SMPLXPP_LIBRARY_HEADERS_DIR}/smplx/internal/cuda_util.cuh"
)

########################################################################################
# Library Target Definition
########################################################################################

if(SMPLXPP_BUILD_SHARED_LIB)
    # Set default visibility of all symbols to hidden
    set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
    set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
    add_library(${SMPLXPP_LIBRARY_NAME} SHARED)
else()
    add_library(${SMPLXPP_LIBRARY_NAME} STATIC)
endif()

# Library Target Alias Definition
#   - <SMPLXPP_PROJECT_NAME>::<SMPLXPP_LIBRARY_NAME> is alias of: <SMPLXPP_LIBRARY_NAME>
add_library(${SMPLXPP_PROJECT_NAME}::${SMPLXPP_LIBRARY_NAME} ALIAS ${SMPLXPP_LIBRARY_NAME})

# Generate the library's export header
# See: https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html#example-generating-export-header
include(GenerateExportHeader)
generate_export_header(${SMPLXPP_LIBRARY_NAME}
    BASE_NAME "${SMPLXPP_PROJECT_NAME_UPPERCASE}" # Export header's macro prefix (`SMPLXPP`)
    EXPORT_FILE_NAME "${SMPLXPP_LIBRARY_NAME}/smplxpp_export.h" # Export header's output file path (relative to BINARY_DIR)
)

# Add sources and headers to the library target
target_sources(${SMPLXPP_LIBRARY_NAME}
    PRIVATE
        ${SMPLXPP_LIBRARY_SOURCES}
    PUBLIC
        FILE_SET smplxpp_public_headers
            TYPE HEADERS
            FILES ${SMPLXPP_LIBRARY_HEADERS_PUBLIC}
            BASE_DIRS "${SMPLXPP_LIBRARY_HEADERS_DIR}"
        FILE_SET smplxpp_generated_headers
            TYPE HEADERS
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${SMPLXPP_LIBRARY_NAME}/smplxpp_export.h" # Export header file path
            # Set `BASE_DIRS` to the library target's binary directory to ensure that 
            # export header is installed into the `${SMPLXPP_LIBRARY_NAME}` subdirectory.
            BASE_DIRS $<TARGET_PROPERTY:${SMPLXPP_LIBRARY_NAME},BINARY_DIR> 
)

if(SMPLXPP_CUDA_ENABLED)
    target_sources(${SMPLXPP_LIBRARY_NAME} 
        PRIVATE
            ${SMPLXPP_LIBRARY_CUDA_SOURCES}
            ${SMPLXPP_LIBRARY_CUDA_HEADERS_PRIVATE}
    )
endif()

target_compile_features(${SMPLXPP_LIBRARY_NAME} PUBLIC cxx_std_17)

# Add compile definitions for targets
target_compile_definitions(${SMPLXPP_LIBRARY_NAME} PUBLIC
    "${SMPLXPP_PROJECT_NAME_UPPERCASE}_DEBUG_MODE=$<CONFIG:Debug>"
)

if(WIN32)
    target_compile_definitions(${SMPLXPP_LIBRARY_NAME} PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Global includes. Used by all targets
# Use generator expressions to specify different paths for the build tree
# and the install tree. This is the standard way to handle installable targets.
target_include_directories(${SMPLXPP_LIBRARY_NAME}
    PUBLIC
        # $<BUILD_INTERFACE:...>: Path used by consumers within the same build tree.
        # $<INSTALL_INTERFACE:...>: Path relative to CMAKE_INSTALL_INCLUDEDIR used by consumers after installation
        $<BUILD_INTERFACE:${SMPLXPP_LIBRARY_HEADERS_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    PRIVATE
        # Path for library's export header(`smplxpp/smplxpp_export.h`).
        # (auto-generated by CMake module `GenerateExportHeader`)
        ${CMAKE_CURRENT_BINARY_DIR}
)


########################################################################################
# Library Dependencies
########################################################################################

include(FetchEigen)
include(FetchFmt)
add_subdirectory(deps)

target_link_libraries(${SMPLXPP_LIBRARY_NAME} 
    PUBLIC
        fmt::fmt-header-only
        Eigen3::Eigen
)

# Add nlohmann json as private include (internal header-only library)
# NOTE: When an INTERFACE library target (like nlohmann-json) is linked to another
#       target (even with PRIVATE linkage), CMake's `install(EXPORT ...)` command still
#       requires that the INTERFACE target be included in the same export set.
#       This causes the following error:
#         CMake Error: install(EXPORT "MyLibTargetsExport" ...) includes target
#         "mylib" which requires target "nlohmann-json" that is not in any export set.
#       To avoid this, we manually add the include directories and compile features 
#       from the nlohmann-json target, not linking the target directly.
target_include_directories(${SMPLXPP_LIBRARY_NAME} PRIVATE
    $<TARGET_PROPERTY:nlohmann-json,INTERFACE_INCLUDE_DIRECTORIES>
)

if(SMPLXPP_CUDA_ENABLED)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(${SMPLXPP_LIBRARY_NAME} PUBLIC CUDA::cudart)
endif()

# Unix systems: link pthread (refer to original)
if(UNIX AND NOT APPLE AND NOT CYGWIN)
    find_package(Threads REQUIRED)
    target_link_libraries(${SMPLXPP_LIBRARY_NAME} PUBLIC Threads::Threads)
endif()


########################################################################################
# Library Installation
########################################################################################

# Install Library Targets & Library Public Headers
install(
    TARGETS "${SMPLXPP_LIBRARY_NAME}"
    EXPORT "${SMPLXPP_TARGETS_EXPORT_NAME}" # Associates this target with the export set named below
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs static libraries (.a, .lib)
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs shared libraries (.so, .dll) - safe to include even for STATIC only
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" # Installs executables or Windows DLLs
    # INCLUDES: Specifies the base directory for <INSTALL_INTERFACE:...> used above.
    # Tells consuming projects that include paths are relative to <prefix>/${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILE_SET smplxpp_public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILE_SET smplxpp_generated_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)


if (SMPLXPP_INSTALL_MODEL_FILES)
    # Install Library Model Files
    #   - models/*.cbor -> <prefix>/models
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/models/" # The trailing '/' copies the *contents* of "models"
        DESTINATION "models" # Target: "<prefix>/models"
        FILES_MATCHING PATTERN "*.cbor"
    )
endif()

# Install Library Config
# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
install(
    EXPORT "${SMPLXPP_TARGETS_EXPORT_NAME}"
    FILE "${SMPLXPP_PROJECT_NAME}Targets.cmake"
    DESTINATION "${SMPLXPP_CONFIG_INSTALL_DIR}"
    NAMESPACE "${SMPLXPP_PROJECT_NAME}::"
)

# Configure '<SMPLXPP_PROJECT_NAME>Config.cmake'
# Use variables:
#   - TARGETS_EXPORT_NAME
#   - SMPLXPP_PROJECT_NAME
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/LibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${SMPLXPP_PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${SMPLXPP_CONFIG_INSTALL_DIR}"
)

# Copy generated config file '<SMPLXPP_PROJECT_NAME>Config.cmake' to install directory
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${SMPLXPP_PROJECT_NAME}Config.cmake"
    DESTINATION "${SMPLXPP_CONFIG_INSTALL_DIR}"
)


########################################################################################
# Library Examples
########################################################################################

if(SMPLXPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()