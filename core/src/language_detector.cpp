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

// PHASE A STUB. This compiles and links with no third-party dependencies so the
// AAR/distribution path can be proven end-to-end before the real engine lands.
// Phase B replaces the body with a fastText (lid.176.ftz) implementation while
// keeping this header/signature unchanged.

#include "language_detector.h"

namespace translatekit {

struct LanguageDetector::Impl {
    std::string model_path;
};

LanguageDetector::LanguageDetector(const std::string& model_path)
    : impl_(new Impl{model_path}) {
    // Phase B: load the fastText model here and throw std::runtime_error if the
    // file is missing/unreadable. The stub intentionally does not touch disk.
}

LanguageDetector::~LanguageDetector() {
    delete impl_;
}

DetectedLanguage LanguageDetector::detect(const std::string& /*text*/) const {
    // Phase B: real fastText prediction. Stub returns a neutral default.
    return DetectedLanguage{"en", 0.0f};
}

}  // namespace translatekit
