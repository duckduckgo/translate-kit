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

package com.duckduckgo.translatekit

/**
 * A loaded (source -> target) translation model. Obtain one via
 * [TranslateKit.loadModel]; it owns mmap'd native memory and must be released
 * with [close] (also wired to [finalize] as a backstop), mirroring the bloom
 * filter's `create…/release…` native-pointer lifecycle.
 *
 * All calls are **blocking** — invoke them off the main thread.
 */
class TranslationModel internal constructor(nativePointer: Long) {

    @Volatile
    private var nativePointer: Long = nativePointer

    /**
     * Translate plain text, or an HTML fragment when [isHtml] is `true`. With
     * [isHtml], inline tags are preserved and re-aligned by the engine
     * (detag-and-project). Blocking.
     *
     * With [htmlAsJsonTree] (requires [isHtml]), the result's text is the
     * translated HTML as a JSON array of `{"text": …}` and
     * `{"tag", "attrs", "children"}` nodes, or the JSON literal `null` when the
     * translated HTML is malformed. Passing it without [isHtml] throws.
     */
    @JvmOverloads
    fun translate(input: String, isHtml: Boolean, htmlAsJsonTree: Boolean = false): TranslationResult {
        val ptr = nativePointer
        check(ptr != 0L) { "TranslationModel has been closed" }
        return nativeTranslate(ptr, input, isHtml, htmlAsJsonTree)
    }

    /**
     * Translate all [inputs] in one engine call, which is faster than one call
     * each. A sentence's output can differ slightly from the single-input
     * overload's, because the engine's output-vocabulary shortlist is shared
     * across the batch. Throws if any input fails, returning no results.
     * [htmlAsJsonTree] applies to every input as in the single-input overload.
     * Blocking.
     */
    @JvmOverloads
    fun translate(inputs: List<String>, isHtml: Boolean, htmlAsJsonTree: Boolean = false): List<TranslationResult> {
        val ptr = nativePointer
        check(ptr != 0L) { "TranslationModel has been closed" }
        return nativeTranslateBatch(ptr, inputs.toTypedArray(), isHtml, htmlAsJsonTree).asList()
    }

    /** Releases native memory. Idempotent. */
    @Synchronized
    fun close() {
        val ptr = nativePointer
        if (ptr != 0L) {
            nativePointer = 0L
            nativeClose(ptr)
        }
    }

    @Suppress("unused", "ProtectedInFinal")
    protected fun finalize() {
        close()
    }

    private external fun nativeTranslate(
        nativePointer: Long,
        input: String,
        isHtml: Boolean,
        htmlAsJsonTree: Boolean,
    ): TranslationResult

    private external fun nativeTranslateBatch(
        nativePointer: Long,
        inputs: Array<String>,
        isHtml: Boolean,
        htmlAsJsonTree: Boolean,
    ): Array<TranslationResult>

    private external fun nativeClose(nativePointer: Long)
}
