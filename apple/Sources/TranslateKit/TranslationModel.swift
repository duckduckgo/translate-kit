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
    ///
    /// With `htmlAsJSONTree` (requires `isHtml`), `text` is the translated HTML as
    /// a JSON array of `{"text": …}` and `{"tag", "attrs", "children"}` nodes, or
    /// the JSON literal `null` when the translated HTML is malformed. Passing it
    /// without `isHtml` throws ``TranslateKitError/invalidArgument(_:)``.
    public func translate(_ input: String, isHtml: Bool, htmlAsJSONTree: Bool = false) throws -> TranslationResult {
        guard let handle else {
            throw TranslateKitError.notInitialized("TranslationModel has been closed")
        }

        var options = tk_translate_options()
        options.is_html = isHtml ? 1 : 0
        options.html_as_json_tree = htmlAsJSONTree ? 1 : 0
        var result = tk_translation_result()
        let status = input.withCString {
            tk_translate_ex(handle, $0, &options, &result)
        }
        // Free the engine-owned buffers on every path; safe on a zeroed struct.
        defer { tk_translation_result_free(&result) }
        try tkCheck(status)

        let text = result.text.map { String(cString: $0) } ?? ""
        var qualityScores: [Float]?
        if let scores = result.quality_scores, result.quality_len > 0 {
            qualityScores = Array(UnsafeBufferPointer(start: scores, count: result.quality_len))
        }
        return TranslationResult(text: text, qualityScores: qualityScores)
    }

    /// Batch overload. Blocking.
    public func translate(_ inputs: [String], isHtml: Bool, htmlAsJSONTree: Bool = false) throws -> [TranslationResult] {
        try inputs.map { try translate($0, isHtml: isHtml, htmlAsJSONTree: htmlAsJSONTree) }
    }

    /// Releases native memory. Idempotent.
    public func close() {
        if let handle {
            self.handle = nil
            tk_model_close(handle)
        }
    }
}
