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

namespace translatekit {

struct DetectedLanguage {
    std::string language;  // BCP-47-ish, e.g. "en"
    float confidence = 0.0f;
};

// Offline language identification, backed by a bundled fastText model
// (lid.176.ftz). Phase A is a stub; Phase B loads the real model.
class LanguageDetector {
public:
    // Loads the model at `model_path`. Throws std::runtime_error on failure.
    explicit LanguageDetector(const std::string& model_path);
    ~LanguageDetector();

    DetectedLanguage detect(const std::string& text) const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace translatekit

#endif  // TRANSLATE_KIT_LANGUAGE_DETECTOR_H
