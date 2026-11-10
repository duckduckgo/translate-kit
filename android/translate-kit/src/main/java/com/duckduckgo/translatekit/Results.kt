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
 * Result of [TranslationModel.translate].
 *
 * @param text the translated text or HTML, or the HTML's JSON tree (`"null"` if malformed).
 * @param qualityScores optional per-sentence quality estimates; `null` unless a
 *   quality model is loaded and scores are requested.
 */
data class TranslationResult(
    val text: String,
    val qualityScores: List<Float>? = null,
)

/**
 * Result of [TranslateKit.detectLanguage].
 *
 * @param language the detector's overall pick, a BCP-47-ish code such as "en",
 *   or `null` when undetermined. Usually `topLanguages.first()`, but CLD2 may
 *   pick differently (it discounts English in mixed text).
 * @param isReliable CLD2's own reliability verdict. Only act on the result
 *   (e.g. offer translation) when this is `true`.
 * @param topLanguages up to three languages found in the text, best first.
 */
data class LanguageResult(
    val language: String?,
    val isReliable: Boolean,
    val topLanguages: List<LanguageShare>,
) {
    // Called from JNI with the C struct's fields; empty codes mark unused slots.
    internal constructor(language: String, isReliable: Boolean, codes: Array<String>, percents: IntArray) : this(
        language.ifEmpty { null },
        isReliable,
        codes.indices.filter { codes[it].isNotEmpty() }.map { LanguageShare(codes[it], percents[it]) },
    )
}

/**
 * One language found in the text and its share of it.
 *
 * @param language BCP-47-ish language code, e.g. "en".
 * @param percent share of the scored text, `0..100`.
 */
data class LanguageShare(
    val language: String,
    val percent: Int,
)
