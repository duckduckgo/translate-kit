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

# Android JNI library. Pulls in the platform-agnostic core (../../core) and
# wraps it with a JNI shim, producing libtranslate-kit.so. Mirrors the DDG
# monorepo's httpsupgrade-impl/CMakeLists.txt (reproducible-build + 16 KB page
# link options).

cmake_minimum_required(VERSION 3.22.1)

project(translate_kit_jni LANGUAGES CXX)

# Build the shared core (static lib) from the repo root. An explicit binary dir
# is required because the source lives outside this directory tree.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_CURRENT_BINARY_DIR}/translatekit_core)

add_library(translate-kit SHARED
        src/main/cpp/translate.cpp)

target_link_libraries(translate-kit
        PRIVATE
        translatekit_core
        log)

# Normalise source/build paths in debug metadata so native outputs are
# reproducible across different checkout locations.
target_compile_options(translate-kit
        PRIVATE
        "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=."
        "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.")

target_link_options(translate-kit
        PRIVATE
        "-Wl,--build-id=none"
        "-Wl,-z,common-page-size=16384"
        "-Wl,-z,max-page-size=16384")
