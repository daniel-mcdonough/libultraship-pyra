cmake_minimum_required(VERSION 3.24.0)

option(NON_PORTABLE "Build a non-portable version" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    option(SIGN_LIBRARY "Enable xcode signing" OFF)
    option(BUNDLE_ID "Bundle ID for xcode signing" "com.example.libultraship")
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
endif()

project(libultraship LANGUAGES C CXX)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(cmake/Utils.cmake)
set(ADDITIONAL_LIB_INCLUDES "")

# ========= Configuration Options =========
option(INCLUDE_MPQ_SUPPORT "Enable StormLib and MPQ archive support" OFF)
set(GBI_UCODE F3DEX_GBI_2 CACHE STRING "Specify the GBI ucode version")

# Declared here rather than in src/, because the per-platform dependency
# scripts included below branch on USE_OPENGLES to pick ImGui's GL backend.
option(USE_OPENGLES "Enable GLES3" OFF)
option(USE_OPENGLES2 "Enable GLES2 (GLSL ES 1.00); implies USE_OPENGLES" OFF)
if (USE_OPENGLES2)
    set(USE_OPENGLES ON CACHE BOOL "Enable GLES3" FORCE)
endif()

# =========== Dependencies =============
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(cmake/dependencies/windows-vcpkg.cmake)
endif()

include(cmake/dependencies/common.cmake)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
	include(cmake/dependencies/android.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(cmake/dependencies/mac.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(cmake/dependencies/ios.cmake)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(cmake/dependencies/linux.cmake)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    include(cmake/dependencies/openbsd.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(cmake/dependencies/windows.cmake)
endif()

# === Platform Specific Configuration ===
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(cmake/ios-toolchain-populate.cmake)
endif()

# =========== Sources =============
add_subdirectory("src")
