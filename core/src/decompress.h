/*
 * Copyright 2026 Marcos Holgado
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TRANSLATE_KIT_DECOMPRESS_H
#define TRANSLATE_KIT_DECOMPRESS_H

#include <string>

namespace translatekit {

// Streams the zstd file at zst_path, decompressed, to out_path. Writes a
// temporary file next to out_path and renames it into place only when the
// whole stream decoded cleanly, so out_path is never left half-written.
// Replaces an existing out_path; leaves zst_path alone. Throws
// std::runtime_error on unreadable, non-zstd, truncated or malformed input and
// on write failures. Damaged data in a frame without a checksum can decode to
// wrong bytes undetected. Shares no state, so concurrent calls are safe.
void decompress_zstd(const std::string& zst_path, const std::string& out_path);

}  // namespace translatekit

#endif  // TRANSLATE_KIT_DECOMPRESS_H
