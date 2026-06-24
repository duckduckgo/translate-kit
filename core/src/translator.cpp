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

// PHASE A STUB. Echoes the input so the JNI/AAR path is verifiable without the
// Bergamot engine. Phase C replaces the body with a real BlockingService +
// TranslationModel + ResponseOptions{ html = true } implementation, keeping
// this header/signature unchanged.

#include "translator.h"

namespace translatekit {

struct Translator::Impl {
    ModelSpec spec;
};

Translator::Translator(const ModelSpec& spec) : impl_(new Impl{spec}) {
    // Phase C: open/mmap the model, vocab and shortlist files, build the
    // bergamot config, and throw std::runtime_error on missing files or engine
    // rejection. The stub intentionally does not touch disk so the echo path
    // works without real model artifacts.
}

Translator::~Translator() {
    delete impl_;
}

TranslationResult Translator::translate(const std::string& input,
                                        bool /*is_html*/) const {
    // Phase C: real translation. Stub echoes input verbatim.
    TranslationResult result;
    result.text = input;
    return result;
}

}  // namespace translatekit
