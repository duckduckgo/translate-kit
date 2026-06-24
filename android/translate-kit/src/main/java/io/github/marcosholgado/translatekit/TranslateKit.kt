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

package io.github.marcosholgado.translatekit

import android.content.Context

/**
 * Entry point to translate-kit: on-device neural machine translation + offline
 * language detection.
 *
 * Usage mirrors a small facade with a lazy [init] driven from the caller's IO
 * dispatcher. All native calls are **blocking**; never call them on the main
 * thread.
 *
 * The library never touches the network. Translation models are supplied by the
 * caller as on-disk paths ([loadModel]); the language detector is bundled and
 * self-contained (no model file to load).
 */
object TranslateKit {

    private const val LIBRARY_NAME = "translate-kit"

    @Volatile
    private var nativeContextPtr: Long = 0L

    /**
     * Loads the native library and initializes the engine. Idempotent.
     * Blocking — call from an IO dispatcher.
     */
    @Synchronized
    fun init(context: Context) {
        if (nativeContextPtr != 0L) return
        System.loadLibrary(LIBRARY_NAME)
        val ptr = nativeInit()
        check(ptr != 0L) { "translate-kit init failed: ${nativeLastError()}" }
        nativeContextPtr = ptr
    }

    fun isInitialized(): Boolean = nativeContextPtr != 0L

    /** Offline language detection using the bundled model. Blocking. */
    fun detectLanguage(text: String): LanguageResult {
        val ptr = nativeContextPtr
        check(ptr != 0L) { "TranslateKit.init(context) must be called before detectLanguage" }
        return nativeDetectLanguage(ptr, text)
    }

    /**
     * Load a translation model from on-disk files. Returns a handle used for
     * subsequent [TranslationModel.translate] calls. Throws on bad/missing
     * files. Blocking.
     */
    fun loadModel(spec: ModelSpec): TranslationModel {
        val ptr = nativeContextPtr
        check(ptr != 0L) { "TranslateKit.init(context) must be called before loadModel" }
        val modelPtr = nativeLoadModel(
            ptr,
            spec.sourceLang,
            spec.targetLang,
            spec.modelPath,
            spec.vocabPaths.toTypedArray(),
            spec.shortlistPath,
            spec.configYaml,
            spec.numWorkers,
        )
        if (modelPtr == 0L) {
            throw IllegalArgumentException("translate-kit loadModel failed: ${nativeLastError()}")
        }
        return TranslationModel(modelPtr)
    }

    /** Releases a loaded model. Equivalent to [TranslationModel.close]. */
    fun unloadModel(model: TranslationModel) {
        model.close()
    }

    private external fun nativeInit(): Long

    private external fun nativeDetectLanguage(nativeContextPtr: Long, text: String): LanguageResult

    private external fun nativeLoadModel(
        nativeContextPtr: Long,
        sourceLang: String,
        targetLang: String,
        modelPath: String,
        vocabPaths: Array<String>,
        shortlistPath: String,
        configPath: String?,
        numWorkers: Int,
    ): Long

    private external fun nativeLastError(): String
}
