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

import CTranslateKit

#if canImport(Glibc)
import Glibc
#else
import Darwin
#endif

/// A loaded (source -> target) translation model. Obtain one via
/// ``TranslateKit/loadModel(_:)``; it owns mmap'd native memory and must be
/// released with ``close()`` (also wired to `deinit` as a backstop), mirroring
/// the Android `TranslationModel` lifecycle.
///
/// - Important: All calls are **blocking** — invoke them off the main thread,
///   and do not use a model from more than one thread at a time.
public final class TranslationModel {

    private var handle: OpaquePointer?

    init(handle: OpaquePointer) {
        self.handle = handle
    }

    deinit {
        close()
    }

    /// Translates plain text, or an HTML fragment when `isHtml` is `true`. With
    /// `isHtml`, inline tags are preserved and re-aligned by the engine
    /// (detag-and-project). Blocking.
    public func translate(_ input: String, isHtml: Bool) throws -> TranslationResult {
        guard let handle else {
            throw TranslateKitError.notInitialized("TranslationModel has been closed")
        }

        var result = tk_translation_result()
        let status = input.withCString {
            tk_translate(handle, $0, Int32(isHtml ? 1 : 0), &result)
        }
        // Free the engine-owned buffers on every path; safe on a zeroed struct.
        defer { tk_translation_result_free(&result) }
        try tkCheck(status)
        return Self.convert(result)
    }

    /// Translates all `inputs` in one engine call, which is faster than one call
    /// each. A sentence's output can differ slightly from the single-input
    /// overload's, because the engine's output-vocabulary shortlist is shared
    /// across the batch. Throws if any input fails, returning no results.
    /// Blocking.
    public func translate(_ inputs: [String], isHtml: Bool) throws -> [TranslationResult] {
        guard let handle else {
            throw TranslateKitError.notInitialized("TranslationModel has been closed")
        }
        // Own a C copy of every input for the duration of the call.
        let copies = inputs.map { strdup($0)! }
        defer { copies.forEach { free($0) } }
        let pointers: [UnsafePointer<CChar>?] = copies.map { UnsafePointer($0) }

        var results = [tk_translation_result](repeating: tk_translation_result(), count: inputs.count)
        let status = pointers.withUnsafeBufferPointer {
            tk_translate_batch(handle, $0.baseAddress, inputs.count, Int32(isHtml ? 1 : 0), &results)
        }
        defer { for i in results.indices { tk_translation_result_free(&results[i]) } }
        try tkCheck(status)
        return results.map(Self.convert)
    }

    private static func convert(_ result: tk_translation_result) -> TranslationResult {
        let text = result.text.map { String(cString: $0) } ?? ""
        var qualityScores: [Float]?
        if let scores = result.quality_scores, result.quality_len > 0 {
            qualityScores = Array(UnsafeBufferPointer(start: scores, count: result.quality_len))
        }
        return TranslationResult(text: text, qualityScores: qualityScores)
    }

    /// Releases native memory. Idempotent.
    public func close() {
        if let handle {
            self.handle = nil
            tk_model_close(handle)
        }
    }
}
