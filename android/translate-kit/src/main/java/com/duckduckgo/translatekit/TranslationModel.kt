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
     */
    fun translate(input: String, isHtml: Boolean): TranslationResult {
        val ptr = nativePointer
        check(ptr != 0L) { "TranslationModel has been closed" }
        return nativeTranslate(ptr, input, isHtml)
    }

    /** Batch overload. Blocking. */
    fun translate(inputs: List<String>, isHtml: Boolean): List<TranslationResult> =
        inputs.map { translate(it, isHtml) }

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
    ): TranslationResult

    private external fun nativeClose(nativePointer: Long)
}
