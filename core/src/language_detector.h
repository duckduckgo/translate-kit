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

#ifndef TRANSLATE_KIT_LANGUAGE_DETECTOR_H
#define TRANSLATE_KIT_LANGUAGE_DETECTOR_H

#include <string>
#include <vector>

namespace translatekit {

struct LanguageShare {
    std::string language;  // e.g. "en"
    int percent = 0;       // share of the scored text, 0-100
};

struct DetectedLanguage {
    std::string language;      // ISO-639-1 / BCP-47-ish, e.g. "en"; empty if unknown
    bool is_reliable = false;  // CLD2's own reliability verdict
    std::vector<LanguageShare> top;  // up to 3, best first; no unknown entries
};

// Offline language identification backed by the bundled CLD2 detector
// (Apache-2.0). CLD2 is self-contained: its language profiles are compiled into
// the library, so there is no model file to load and detection is stateless.
class LanguageDetector {
public:
    LanguageDetector() = default;

    DetectedLanguage detect(const std::string& text) const;
};

}  // namespace translatekit

#endif  // TRANSLATE_KIT_LANGUAGE_DETECTOR_H
