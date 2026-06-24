# Copyright 2026 Marcos Holgado
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Platform-agnostic translate-kit engine core.
#
# Produces a static library `translatekit_core` that platform wrappers link
# into their own shared library:
#   - Android: android/translate-kit builds it via externalNativeBuild and links
#     it into libtranslate-kit.so alongside the JNI shim.
#   - Other platforms: scripts/build-*.sh build it for their toolchains.
#
# Optionally builds the host gtest suite when -DTRANSLATEKIT_BUILD_TESTS=ON.

cmake_minimum_required(VERSION 3.18.1)

project(translatekit_core LANGUAGES CXX)

option(TRANSLATEKIT_BUILD_TESTS "Build the translate-kit core host unit tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(translatekit_core STATIC
        src/c_api.cpp
        src/language_detector.cpp
        src/translator.cpp)

# Public: the C ABI header. Private: internal C++ headers under src/.
target_include_directories(translatekit_core
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Position-independent so it can be linked into a shared object (the .so / JNI lib).
set_target_properties(translatekit_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(TRANSLATEKIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
