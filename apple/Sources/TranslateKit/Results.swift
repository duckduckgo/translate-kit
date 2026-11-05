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

/// Result of ``TranslationModel/translate(_:isHtml:htmlAsJSONTree:)``.
public struct TranslationResult: Equatable {
    /// The translated text or HTML, or the HTML's JSON tree (`"null"` if malformed).
    public let text: String
    /// Optional per-sentence quality estimates; `nil` unless a quality model is
    /// loaded and scores are requested.
    public let qualityScores: [Float]?

    public init(text: String, qualityScores: [Float]? = nil) {
        self.text = text
        self.qualityScores = qualityScores
    }
}

/// Result of ``TranslateKit/detectLanguage(_:)``.
public struct LanguageResult: Equatable {
    /// The detector's overall pick, a BCP-47-ish code such as `"en"`, or `nil`
    /// when undetermined. Usually `topLanguages.first`, but CLD2 may pick
    /// differently (it discounts English in mixed text).
    public let language: String?
    /// CLD2's own reliability verdict. Only act on the result (e.g. offer
    /// translation) when this is `true`.
    public let isReliable: Bool
    /// Up to three languages found in the text, best first.
    public let topLanguages: [LanguageShare]

    public init(language: String?, isReliable: Bool, topLanguages: [LanguageShare]) {
        self.language = language
        self.isReliable = isReliable
        self.topLanguages = topLanguages
    }
}

/// One language found in the text and its share of it.
public struct LanguageShare: Equatable {
    /// BCP-47-ish language code, e.g. `"en"`.
    public let language: String
    /// Share of the scored text, `0...100`.
    public let percent: Int

    public init(language: String, percent: Int) {
        self.language = language
        self.percent = percent
    }
}
