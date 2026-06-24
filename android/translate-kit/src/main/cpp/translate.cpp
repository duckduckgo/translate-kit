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

// JNI shim: bridges the Kotlin facade (TranslateKit / TranslationModel) to the
// platform-neutral C ABI in translate_kit.h. Native handles travel as jlong
// pointers (mirrors the DDG bloom-filter create/release pattern). Strings are
// marshaled as UTF-8.

#include <jni.h>

#include <string>
#include <vector>

#include "translate_kit/translate_kit.h"

namespace {

std::string ToString(JNIEnv* env, jstring s) {
    if (s == nullptr) return std::string();
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out(chars != nullptr ? chars : "");
    if (chars != nullptr) env->ReleaseStringUTFChars(s, chars);
    return out;
}

void ThrowRuntime(JNIEnv* env, const char* message) {
    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls != nullptr) env->ThrowNew(cls, message != nullptr ? message : "translate-kit error");
}

}  // namespace

extern "C" {

/* ---- TranslateKit (object) --------------------------------------------- */

JNIEXPORT jlong JNICALL
Java_io_github_marcosholgado_translatekit_TranslateKit_nativeInit(
        JNIEnv* env, jobject, jstring langid_model_path) {
    const std::string path = ToString(env, langid_model_path);
    tk_context* ctx = nullptr;
    if (tk_init(path.c_str(), &ctx) != TK_OK) return 0;
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT jstring JNICALL
Java_io_github_marcosholgado_translatekit_TranslateKit_nativeLastError(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(tk_last_error());
}

JNIEXPORT jobject JNICALL
Java_io_github_marcosholgado_translatekit_TranslateKit_nativeDetectLanguage(
        JNIEnv* env, jobject, jlong context_ptr, jstring text) {
    auto* ctx = reinterpret_cast<tk_context*>(context_ptr);
    const std::string in = ToString(env, text);

    tk_language_result result = {};
    if (tk_detect_language(ctx, in.c_str(), &result) != TK_OK) {
        ThrowRuntime(env, tk_last_error());
        return nullptr;
    }

    jclass cls = env->FindClass("io/github/marcosholgado/translatekit/LanguageResult");
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;F)V");
    jstring language = env->NewStringUTF(result.language);
    jobject obj = env->NewObject(cls, ctor, language, static_cast<jfloat>(result.confidence));
    env->DeleteLocalRef(language);
    env->DeleteLocalRef(cls);
    return obj;
}

JNIEXPORT jlong JNICALL
Java_io_github_marcosholgado_translatekit_TranslateKit_nativeLoadModel(
        JNIEnv* env, jobject, jlong context_ptr, jstring source_lang, jstring target_lang,
        jstring model_path, jobjectArray vocab_paths, jstring shortlist_path,
        jstring config_path, jint num_workers) {
    auto* ctx = reinterpret_cast<tk_context*>(context_ptr);

    const std::string s_source = ToString(env, source_lang);
    const std::string s_target = ToString(env, target_lang);
    const std::string s_model = ToString(env, model_path);
    const std::string s_shortlist = ToString(env, shortlist_path);
    const bool has_config = config_path != nullptr;
    const std::string s_config = ToString(env, config_path);

    // Own the vocab strings, then hand the C ABI an array of stable pointers.
    std::vector<std::string> vocab_strings;
    const jsize vocab_count = vocab_paths != nullptr ? env->GetArrayLength(vocab_paths) : 0;
    vocab_strings.reserve(static_cast<size_t>(vocab_count));
    for (jsize i = 0; i < vocab_count; ++i) {
        auto element = reinterpret_cast<jstring>(env->GetObjectArrayElement(vocab_paths, i));
        vocab_strings.push_back(ToString(env, element));
        env->DeleteLocalRef(element);
    }
    std::vector<const char*> vocab_ptrs;
    vocab_ptrs.reserve(vocab_strings.size());
    for (const std::string& v : vocab_strings) vocab_ptrs.push_back(v.c_str());

    tk_model_spec spec = {};
    spec.source_lang = s_source.c_str();
    spec.target_lang = s_target.c_str();
    spec.model_path = s_model.c_str();
    spec.vocab_paths = vocab_ptrs.empty() ? nullptr : vocab_ptrs.data();
    spec.vocab_count = vocab_ptrs.size();
    spec.shortlist_path = s_shortlist.c_str();
    spec.config_path = has_config ? s_config.c_str() : nullptr;
    spec.num_workers = num_workers;

    tk_model* model = nullptr;
    if (tk_load_model(ctx, &spec, &model) != TK_OK) return 0;
    return reinterpret_cast<jlong>(model);
}

/* ---- TranslationModel -------------------------------------------------- */

JNIEXPORT jobject JNICALL
Java_io_github_marcosholgado_translatekit_TranslationModel_nativeTranslate(
        JNIEnv* env, jobject, jlong model_ptr, jstring input, jboolean is_html) {
    auto* model = reinterpret_cast<tk_model*>(model_ptr);
    const std::string in = ToString(env, input);

    tk_translation_result result = {};
    if (tk_translate(model, in.c_str(), is_html ? 1 : 0, &result) != TK_OK) {
        ThrowRuntime(env, tk_last_error());
        return nullptr;
    }

    jstring text = env->NewStringUTF(result.text != nullptr ? result.text : "");
    // Phase A: quality scores are not surfaced (always null). The C ABI carries
    // them (result.quality_scores) for Phase C; build a List<Float> here then.
    tk_translation_result_free(&result);

    jclass cls = env->FindClass("io/github/marcosholgado/translatekit/TranslationResult");
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/util/List;)V");
    jobject obj = env->NewObject(cls, ctor, text, nullptr);
    env->DeleteLocalRef(text);
    env->DeleteLocalRef(cls);
    return obj;
}

JNIEXPORT void JNICALL
Java_io_github_marcosholgado_translatekit_TranslationModel_nativeClose(
        JNIEnv*, jobject, jlong model_ptr) {
    tk_model_close(reinterpret_cast<tk_model*>(model_ptr));
}

}  // extern "C"
