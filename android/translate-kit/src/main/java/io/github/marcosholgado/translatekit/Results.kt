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

/**
 * Result of [TranslationModel.translate].
 *
 * @param text the translated text or HTML.
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
 * @param language BCP-47-ish language code, e.g. "en".
 * @param confidence detector confidence in `[0, 1]`.
 */
data class LanguageResult(
    val language: String,
    val confidence: Float,
)
