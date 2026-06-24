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
import java.io.File
import java.io.FileNotFoundException

/**
 * Entry point to translate-kit: on-device neural machine translation + offline
 * language detection.
 *
 * Usage mirrors a small facade with a lazy [init] driven from the caller's IO
 * dispatcher. All native calls are **blocking**; never call them on the main
 * thread.
 *
 * The library never touches the network. Translation models are supplied by the
 * caller as on-disk paths ([loadModel]); the language-ID model is bundled.
 */
object TranslateKit {

    private const val LIBRARY_NAME = "translate-kit"

    /** Bundled fastText language-ID model (packaged as an asset). */
    private const val LANGID_ASSET = "lid.176.ftz"

    @Volatile
    private var nativeContextPtr: Long = 0L

    /**
     * Loads the native library and the bundled language-ID model. Idempotent.
     * Blocking — call from an IO dispatcher.
     */
    @Synchronized
    fun init(context: Context) {
        if (nativeContextPtr != 0L) return
        System.loadLibrary(LIBRARY_NAME)
        val langIdPath = extractLangIdModel(context)
        val ptr = nativeInit(langIdPath)
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

    /**
     * Copies the bundled language-ID asset to a stable on-disk path the native
     * engine can read. If the asset is not bundled yet (early phases), returns
     * the intended path without failing.
     */
    private fun extractLangIdModel(context: Context): String {
        val dir = File(context.filesDir, "translate-kit").apply { mkdirs() }
        val dest = File(dir, LANGID_ASSET)
        if (!dest.exists()) {
            try {
                context.assets.open(LANGID_ASSET).use { input ->
                    dest.outputStream().use { output -> input.copyTo(output) }
                }
            } catch (_: FileNotFoundException) {
                // Not bundled yet (Phase A). The native stub does not read the file.
            }
        }
        return dest.absolutePath
    }

    private external fun nativeInit(langIdModelPath: String): Long

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
