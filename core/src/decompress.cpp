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

#include "decompress.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <zstd.h>
#include <zstd_errors.h>

namespace translatekit {

namespace {

std::atomic<unsigned long> g_temp_counter{0};

[[noreturn]] void fail(const std::string& what, const std::string& path) {
    throw std::runtime_error("decompress_zstd: " + what + ": " + path);
}

[[noreturn]] void fail_errno(const std::string& what, const std::string& path) {
    fail(what + " (" + std::strerror(errno) + ")", path);
}

struct FileCloser {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using File = std::unique_ptr<std::FILE, FileCloser>;

struct DCtxFreer {
    void operator()(ZSTD_DCtx* d) const { ZSTD_freeDCtx(d); }
};

// Decodes zst_path into the already-open `out`. Throws on any error.
void decompress_to(const std::string& zst_path, std::FILE* out, const std::string& out_name) {
    File in(std::fopen(zst_path.c_str(), "rb"));
    if (!in) fail_errno("cannot open", zst_path);
    std::unique_ptr<ZSTD_DCtx, DCtxFreer> dctx(ZSTD_createDCtx());
    if (!dctx) throw std::bad_alloc();

    std::vector<char> in_buf(ZSTD_DStreamInSize());
    std::vector<char> out_buf(ZSTD_DStreamOutSize());
    bool any_input = false;
    size_t frame_left = 0;  // 0: between frames; otherwise the current frame is unfinished
    size_t n;
    while ((n = std::fread(in_buf.data(), 1, in_buf.size(), in.get())) > 0) {
        any_input = true;
        ZSTD_inBuffer input = {in_buf.data(), n, 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output = {out_buf.data(), out_buf.size(), 0};
            frame_left = ZSTD_decompressStream(dctx.get(), &output, &input);
            if (ZSTD_isError(frame_left)) {
                if (ZSTD_getErrorCode(frame_left) == ZSTD_error_prefix_unknown) {
                    fail("not a zstd file", zst_path);
                }
                fail(std::string("corrupt zstd data (") + ZSTD_getErrorName(frame_left) + ")",
                     zst_path);
            }
            if (std::fwrite(out_buf.data(), 1, output.pos, out) != output.pos) {
                fail_errno("write failed", out_name);
            }
        }
    }
    if (std::ferror(in.get())) fail_errno("read failed", zst_path);
    if (!any_input) fail("not a zstd file", zst_path);
    if (frame_left != 0) fail("truncated zstd data", zst_path);
}

}  // namespace

void decompress_zstd(const std::string& zst_path, const std::string& out_path) {
    // shortcut: unique per process only; two processes writing the same
    // out_path at once could share a temp name. Fine for one app process.
    const std::string temp_path = out_path + ".tmp-" + std::to_string(++g_temp_counter);

    std::FILE* out = std::fopen(temp_path.c_str(), "wb");
    if (out == nullptr) fail_errno("cannot create", temp_path);
    try {
        decompress_to(zst_path, out, out_path);
    } catch (...) {
        std::fclose(out);
        std::remove(temp_path.c_str());
        throw;
    }
    if (std::fclose(out) != 0) {
        const int saved = errno;
        std::remove(temp_path.c_str());
        errno = saved;
        fail_errno("write failed", out_path);
    }
    if (std::rename(temp_path.c_str(), out_path.c_str()) != 0) {
        const int saved = errno;
        std::remove(temp_path.c_str());
        errno = saved;
        fail_errno("cannot rename into place", out_path);
    }
}

}  // namespace translatekit
