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
 * Describes one (source -> target) translation model to load.
 *
 * All paths point at **decompressed** files on disk (the caller un-gzips
 * Mozilla's `.gz` artifacts). File-path based so the engine can mmap large
 * models, keeping RSS near the on-disk size and letting callers share pages.
 *
 * @param sourceLang source language code, e.g. "es".
 * @param targetLang target language code, e.g. "en".
 * @param modelPath int8 model, e.g. `model.<pair>.intgemm.alphas.bin`.
 * @param vocabPaths shared vocab (1) or src+tgt (2) SentencePiece `.spm` files.
 * @param shortlistPath lexical shortlist, e.g. `lex.<...>.s2t.bin`.
 * @param configYaml path to a pre-generated Bergamot config YAML (produced
 *   off-device; see SPEC-1 §10a). `null` falls back to engine defaults.
 * @param numWorkers engine worker threads (>= 1).
 */
data class ModelSpec(
    val sourceLang: String,
    val targetLang: String,
    val modelPath: String,
    val vocabPaths: List<String>,
    val shortlistPath: String,
    val configYaml: String? = null,
    val numWorkers: Int = 1,
)
