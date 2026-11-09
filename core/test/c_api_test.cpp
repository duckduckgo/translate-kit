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

// Phase A: exercises the C ABI against the stub engine (echo translate, neutral
// language detection). Phase B/C add golden detection/translation cases here.

#include "translate_kit/translate_kit.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

namespace {

tk_context* MakeContext() {
    tk_context* ctx = nullptr;
    EXPECT_EQ(tk_init(&ctx), TK_OK);
    EXPECT_NE(ctx, nullptr);
    return ctx;
}

tk_model* LoadStubModel(tk_context* ctx) {
    const char* vocab[] = {"/tmp/vocab.spm"};
    tk_model_spec spec = {};
    spec.source_lang = "es";
    spec.target_lang = "en";
    spec.model_path = "/tmp/model.bin";
    spec.vocab_paths = vocab;
    spec.vocab_count = 1;
    spec.shortlist_path = "/tmp/lex.bin";
    spec.config_path = nullptr;
    spec.num_workers = 1;
    tk_model* model = nullptr;
    EXPECT_EQ(tk_load_model(ctx, &spec, &model), TK_OK);
    EXPECT_NE(model, nullptr);
    return model;
}

TEST(CApi, VersionIsNonEmpty) {
    EXPECT_STRNE(tk_version(), "");
}

// The version has two consumers that must agree: the Android AAR reads the
// repo-root VERSION file at build time, and tk_version() gets it from CMake
// (see the Version block in core/CMakeLists.txt). Reading the file again HERE,
// at run time, is what makes that wiring load-bearing: hardcode a literal back
// into c_api.cpp and this fails.
TEST(CApi, VersionMatchesVersionFile) {
    std::ifstream file(TK_VERSION_FILE);
    ASSERT_TRUE(file.is_open()) << "cannot open " << TK_VERSION_FILE;

    std::string expected;
    ASSERT_TRUE(std::getline(file, expected)) << "VERSION file is empty";
    while (!expected.empty() && (expected.back() == '\r' || expected.back() == ' ')) {
        expected.pop_back();
    }

    EXPECT_EQ(std::string(tk_version()), expected);
}

TEST(CApi, InitRejectsNullOutArg) {
    EXPECT_EQ(tk_init(nullptr), TK_ERR_INVALID_ARG);
}

// Too little text for CLD2 to call: undetermined and not reliable, so the app
// should not offer translation.
TEST(CApi, DetectLanguageShortTextIsUndetermined) {
    tk_context* ctx = MakeContext();
    tk_language_result r = {};
    EXPECT_EQ(tk_detect_language(ctx, "hola mundo", &r), TK_OK);
    EXPECT_STREQ(r.language, "");
    EXPECT_EQ(r.is_reliable, 0);
    tk_shutdown(ctx);
}

// Golden language-detection cases against the bundled CLD2 detector. Sentences
// are long enough for CLD2 to score reliably.
TEST(CApi, DetectLanguageGoldens) {
    tk_context* ctx = MakeContext();
    struct Case {
        const char* text;
        const char* expected;
    };
    const Case cases[] = {
        {"This is a simple sentence written in the English language for testing purposes.", "en"},
        {"Esta es una frase sencilla escrita en el idioma español para realizar algunas pruebas.", "es"},
        {"Ceci est une phrase simple écrite en langue française afin d'effectuer quelques tests.", "fr"},
        {"Dies ist ein einfacher Satz, der zu Testzwecken in deutscher Sprache geschrieben wurde.", "de"},
        {"Questa è una frase semplice scritta in lingua italiana per scopi di test e verifica.", "it"},
    };
    for (const Case& c : cases) {
        tk_language_result r = {};
        ASSERT_EQ(tk_detect_language(ctx, c.text, &r), TK_OK) << c.text;
        EXPECT_STREQ(r.language, c.expected) << "text: " << c.text;
        EXPECT_EQ(r.is_reliable, 1) << c.text;
        EXPECT_STREQ(r.top[0].language, c.expected) << c.text;
        EXPECT_GT(r.top[0].percent, 0) << c.text;
    }
    tk_shutdown(ctx);
}

// Spanish then English, about half each. CLD2 ranks English first by share but
// picks Spanish overall (it discounts English in mixed text), so `language`
// and `top[0]` legitimately differ.
TEST(CApi, DetectLanguageMixedReportsTopThree) {
    tk_context* ctx = MakeContext();
    tk_language_result r = {};
    ASSERT_EQ(tk_detect_language(ctx,
                                 "Esta es una frase sencilla escrita en el idioma español para realizar "
                                 "algunas pruebas. This is a simple sentence written in the English "
                                 "language for testing purposes.",
                                 &r),
              TK_OK);
    EXPECT_STREQ(r.language, "es");
    EXPECT_EQ(r.is_reliable, 1);
    EXPECT_STREQ(r.top[0].language, "en");
    EXPECT_STREQ(r.top[1].language, "es");
    EXPECT_GT(r.top[0].percent, 40);
    EXPECT_GT(r.top[1].percent, 40);
    EXPECT_LE(r.top[0].percent + r.top[1].percent, 100);
    EXPECT_STREQ(r.top[2].language, "");
    EXPECT_EQ(r.top[2].percent, 0);
    tk_shutdown(ctx);
}

TEST(CApi, DetectLanguageEmptyIsUnknown) {
    tk_context* ctx = MakeContext();
    tk_language_result r = {};
    EXPECT_EQ(tk_detect_language(ctx, "", &r), TK_OK);
    EXPECT_STREQ(r.language, "");
    EXPECT_EQ(r.is_reliable, 0);
    for (const tk_language_share& s : r.top) {
        EXPECT_STREQ(s.language, "");
        EXPECT_EQ(s.percent, 0);
    }
    tk_shutdown(ctx);
}

TEST(CApi, DetectLanguageWithoutContextFails) {
    tk_language_result r = {};
    EXPECT_EQ(tk_detect_language(nullptr, "x", &r), TK_ERR_NOT_INITIALIZED);
}

#ifndef TRANSLATEKIT_WITH_ENGINE
TEST(CApi, TranslateEchoesInputInStub) {
    tk_context* ctx = MakeContext();
    tk_model* model = LoadStubModel(ctx);

    const std::string input = "Hola <b>mundo</b>";
    tk_translation_result r = {};
    EXPECT_EQ(tk_translate(model, input.c_str(), /*is_html=*/1, &r), TK_OK);
    ASSERT_NE(r.text, nullptr);
    EXPECT_EQ(std::string(r.text), input);  // stub echoes
    tk_translation_result_free(&r);

    tk_model_close(model);
    tk_shutdown(ctx);
}

TEST(CApi, TranslateBatchEchoesInputsInStub) {
    tk_context* ctx = MakeContext();
    tk_model* model = LoadStubModel(ctx);

    const char* inputs[] = {"uno", "<b>dos</b>", ""};
    tk_translation_result r[3] = {};
    ASSERT_EQ(tk_translate_batch(model, inputs, 3, /*is_html=*/1, r), TK_OK) << tk_last_error();
    for (int i = 0; i < 3; ++i) {
        ASSERT_NE(r[i].text, nullptr);
        EXPECT_STREQ(r[i].text, inputs[i]);  // stub echoes, in order
        tk_translation_result_free(&r[i]);
    }

    tk_model_close(model);
    tk_shutdown(ctx);
}
#endif  // !TRANSLATEKIT_WITH_ENGINE

TEST(CApi, TranslateBatchRejectsBadArgs) {
    tk_model* model = reinterpret_cast<tk_model*>(1);  // never dereferenced
    const char* inputs[] = {"a", nullptr};
    tk_translation_result r[2];
    r[0].text = reinterpret_cast<char*>(1);  // must be zeroed on failure
    EXPECT_EQ(tk_translate_batch(model, inputs, 2, 0, r), TK_ERR_INVALID_ARG);
    EXPECT_NE(std::string(tk_last_error()).find("inputs[1]"), std::string::npos);
    EXPECT_EQ(r[0].text, nullptr);
    EXPECT_EQ(tk_translate_batch(nullptr, inputs, 1, 0, r), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_translate_batch(model, nullptr, 1, 0, r), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_translate_batch(model, inputs, 1, 0, nullptr), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_translate_batch(model, nullptr, 0, 0, nullptr), TK_OK);  // empty batch
}

#ifdef TRANSLATEKIT_WITH_ENGINE
// Golden translation against a real tiny en->de model. Provide the extracted
// ende.student.tiny11 directory via TK_TEST_MODEL_DIR (see scripts/fetch-models.sh);
// the test is skipped when it is not set.
TEST(CApi, TranslateRealModelEnDe) {
    const char* dir = std::getenv("TK_TEST_MODEL_DIR");
    if (dir == nullptr || dir[0] == '\0') {
        GTEST_SKIP() << "set TK_TEST_MODEL_DIR to the ende.student.tiny11 directory";
    }
    const std::string d(dir);
    const std::string model = d + "/model.intgemm.alphas.bin";
    const std::string vocab = d + "/vocab.deen.spm";
    const std::string lex = d + "/lex.s2t.bin";
    const std::string config = d + "/config.intgemm8bitalpha.yml";
    const char* vocabs[] = {vocab.c_str()};

    tk_context* ctx = MakeContext();
    tk_model_spec spec = {};
    spec.source_lang = "en";
    spec.target_lang = "de";
    spec.model_path = model.c_str();
    spec.vocab_paths = vocabs;
    spec.vocab_count = 1;
    spec.shortlist_path = lex.c_str();
    spec.config_path = config.c_str();
    spec.num_workers = 1;

    tk_model* m = nullptr;
    ASSERT_EQ(tk_load_model(ctx, &spec, &m), TK_OK) << tk_last_error();

    // (a) plain text
    tk_translation_result r = {};
    ASSERT_EQ(tk_translate(m, "Hello World!", /*is_html=*/0, &r), TK_OK) << tk_last_error();
    ASSERT_NE(r.text, nullptr);
    const std::string out(r.text);
    EXPECT_NE(out.find("Hallo"), std::string::npos) << "plain output: " << out;
    tk_translation_result_free(&r);

    // (b) HTML fragments: tags/attributes round-trip (detag-and-project). These
    // mirror TranslateKitInstrumentedTest.HTML_CASES so the host and on-device
    // goldens stay in lock-step. Tag/attribute substrings are structural
    // invariants (projected via alignment, not generated); the German tokens are
    // the stable greedy (beam-size 1) outputs.
    struct HtmlCase {
        const char* input;
        std::vector<const char*> must_contain;  // checked when `exact` is null
        const char* exact;                      // exact match when non-null
    };
    const std::vector<HtmlCase> html_cases = {
        // Canonical inline tag.
        {"<b>Hello World!</b>", {}, "<b>Hallo Welt!</b>"},
        // Attribute (href) preserved verbatim; content translated.
        {"<a href=\"https://example.com\">Hello World!</a>",
         {"<a href=\"https://example.com\">", "</a>", "Hallo Welt!"}, nullptr},
        // Tag repositioned across sentence reordering.
        {"Click <a href=\"https://example.com\">here</a> to continue.",
         {"<a href=\"https://example.com\">", "</a>", "hier", "Klicken"}, nullptr},
        // Nested inline tags.
        {"<b>Hello <i>beautiful</i> World!</b>",
         {"<b>", "<i>", "</i>", "</b>", "Welt"}, nullptr},
        // Sibling inline tags.
        {"<b>Hello</b> <i>World</i>!", {"<b>Hallo</b>", "<i>Welt</i>"}, nullptr},
        // Void element preserved and repositioned.
        {"Hello<br>World", {"<br>", "Hallo", "Welt"}, nullptr},
        // Block elements translate independently; structure intact.
        {"<p>Hello World!</p><p>Good morning!</p>",
         {"<p>Hallo Welt!</p>", "<p>Guten Morgen!</p>"}, nullptr},
        // Multiple attributes preserved verbatim.
        {"<span class=\"greeting\" data-id=\"42\">Hello World!</span>",
         {"<span class=\"greeting\" data-id=\"42\">", "</span>", "Hallo Welt!"}, nullptr},
        // HTML entity survives.
        {"Hello &amp; goodbye", {"&amp;", "Hallo"}, nullptr},
        // Full combo: block + repositioned link + complex href (query + entity).
        {"<p>Please <a href=\"https://example.com/path?q=1&amp;x=2\">click here</a> now.</p>",
         {"<p>", "<a href=\"https://example.com/path?q=1&amp;x=2\">", "</a>", "</p>"}, nullptr},
    };
    for (const HtmlCase& c : html_cases) {
        tk_translation_result h = {};
        ASSERT_EQ(tk_translate(m, c.input, /*is_html=*/1, &h), TK_OK)
            << "input: " << c.input << " err: " << tk_last_error();
        ASSERT_NE(h.text, nullptr) << "input: " << c.input;
        const std::string html(h.text);
        if (c.exact != nullptr) {
            EXPECT_EQ(html, c.exact) << "input: " << c.input;
        } else {
            for (const char* needle : c.must_contain) {
                EXPECT_NE(html.find(needle), std::string::npos)
                    << "input: " << c.input << "\n  output: " << html
                    << "\n  missing: " << needle;
            }
        }
        tk_translation_result_free(&h);
    }

    // (c) The same HTML cases in one batch. Outputs may differ slightly from the
    // one-at-a-time calls (shared shortlist), but the structural invariants hold.
    std::vector<const char*> batch_inputs;
    for (const HtmlCase& c : html_cases) batch_inputs.push_back(c.input);
    std::vector<tk_translation_result> batch(batch_inputs.size());
    ASSERT_EQ(tk_translate_batch(m, batch_inputs.data(), batch_inputs.size(), /*is_html=*/1,
                                 batch.data()),
              TK_OK)
        << tk_last_error();
    for (size_t i = 0; i < batch.size(); ++i) {
        ASSERT_NE(batch[i].text, nullptr) << "input: " << batch_inputs[i];
        const std::string html(batch[i].text);
        for (const char* needle : html_cases[i].must_contain) {
            EXPECT_NE(html.find(needle), std::string::npos)
                << "input: " << batch_inputs[i] << "\n  output: " << html
                << "\n  missing: " << needle;
        }
        tk_translation_result_free(&batch[i]);
    }

    tk_model_close(m);
    tk_shutdown(ctx);
}

// Loading a second model in the same process must succeed. marian's
// createLoggers() registers spdlog loggers ("general", "valid") on every model
// load, and spdlog throws when a name is already registered; without the
// idempotent-logger engine patch the second load aborts with
// "logger with name 'general' already exists". The app loads several models per
// process (e.g. en->es then en->de) and pivoting needs two loaded at once.
TEST(CApi, LoadMultipleModelsInOneProcess) {
    const char* dir = std::getenv("TK_TEST_MODEL_DIR");
    if (dir == nullptr || dir[0] == '\0') {
        GTEST_SKIP() << "set TK_TEST_MODEL_DIR to the ende.student.tiny11 directory";
    }
    const std::string d(dir);
    const std::string model = d + "/model.intgemm.alphas.bin";
    const std::string vocab = d + "/vocab.deen.spm";
    const std::string lex = d + "/lex.s2t.bin";
    const std::string config = d + "/config.intgemm8bitalpha.yml";
    const char* vocabs[] = {vocab.c_str()};

    auto make_spec = [&]() {
        tk_model_spec spec = {};
        spec.source_lang = "en";
        spec.target_lang = "de";
        spec.model_path = model.c_str();
        spec.vocab_paths = vocabs;
        spec.vocab_count = 1;
        spec.shortlist_path = lex.c_str();
        spec.config_path = config.c_str();
        spec.num_workers = 1;
        return spec;
    };

    tk_context* ctx = MakeContext();

    tk_model_spec spec1 = make_spec();
    tk_model* m1 = nullptr;
    ASSERT_EQ(tk_load_model(ctx, &spec1, &m1), TK_OK) << tk_last_error();

    // The second load is the regression: it used to abort inside spdlog.
    tk_model_spec spec2 = make_spec();
    tk_model* m2 = nullptr;
    ASSERT_EQ(tk_load_model(ctx, &spec2, &m2), TK_OK) << tk_last_error();

    // Both handles still translate.
    for (tk_model* m : {m1, m2}) {
        tk_translation_result r = {};
        ASSERT_EQ(tk_translate(m, "Hello World!", /*is_html=*/0, &r), TK_OK) << tk_last_error();
        ASSERT_NE(r.text, nullptr);
        EXPECT_NE(std::string(r.text).find("Hallo"), std::string::npos);
        tk_translation_result_free(&r);
    }

    tk_model_close(m1);
    tk_model_close(m2);
    tk_shutdown(ctx);
}
#endif  // TRANSLATEKIT_WITH_ENGINE

TEST(CApi, LoadModelRejectsMissingModelPath) {
    tk_context* ctx = MakeContext();
    tk_model_spec spec = {};
    spec.source_lang = "es";
    spec.target_lang = "en";
    spec.model_path = nullptr;
    tk_model* model = nullptr;
    EXPECT_EQ(tk_load_model(ctx, &spec, &model), TK_ERR_INVALID_ARG);
    EXPECT_EQ(model, nullptr);
    tk_shutdown(ctx);
}

TEST(CApi, FreeResultIsIdempotentAndNullSafe) {
    tk_translation_result r = {};
    tk_translation_result_free(&r);  // zero-initialized
    tk_translation_result_free(nullptr);
    tk_model_close(nullptr);
    tk_shutdown(nullptr);
}

/* ---- tk_decompress_zstd ------------------------------------------------- */

// Builds a zstd frame from raw (uncompressed) blocks, so the test needs no
// encoder and controls every byte. No content size, no checksum.
std::string ZstdRaw(const std::string& data) {
    std::string out = {'\x28', '\xb5', '\x2f', '\xfd',  // magic
                       '\x00',                          // frame header: no size/checksum/dict
                       '\x38'};                         // window 2^17 = the max block size
    size_t pos = 0;
    do {
        const size_t len = std::min<size_t>(data.size() - pos, 65536);
        const uint32_t header = static_cast<uint32_t>(len << 3) | (pos + len == data.size() ? 1u : 0u);
        for (int i = 0; i < 3; ++i) out.push_back(static_cast<char>((header >> (8 * i)) & 0xFF));
        out.append(data, pos, len);
        pos += len;
    } while (pos < data.size());
    return out;
}

// "hello model" from the zstd CLI (-19 --check): a real compressed frame with a
// content checksum.
const std::string kZstdHello(
    "\x28\xb5\x2f\xfd\x04\x68\x59\x00\x00\x68\x65\x6c\x6c\x6f\x20\x6d\x6f\x64\x65\x6c"
    "\x95\x9e\x11\x21",
    24);

// Larger than the decompressor's read buffer, so several reads happen.
std::string Payload() {
    std::string s;
    for (int i = 0; i < 300000; ++i) s.push_back(static_cast<char>((i * 7) % 251));
    return s;
}

std::filesystem::path ZstdDir(const char* name) {
    auto dir = std::filesystem::path(::testing::TempDir()) / "tk_zstd" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::filesystem::path& p, const std::string& bytes) {
    std::ofstream(p, std::ios::binary) << bytes;
}

std::string ReadFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

size_t FileCount(const std::filesystem::path& dir) {
    return static_cast<size_t>(std::distance(std::filesystem::directory_iterator(dir), {}));
}

TEST(CApi, DecompressZstdWritesOutput) {
    const auto dir = ZstdDir("ok");
    WriteFile(dir / "m.bin.zst", ZstdRaw(Payload()));
    WriteFile(dir / "m.bin", "stale");  // replaced

    ASSERT_EQ(tk_decompress_zstd((dir / "m.bin.zst").c_str(), (dir / "m.bin").c_str()), TK_OK)
        << tk_last_error();
    EXPECT_EQ(ReadFile(dir / "m.bin"), Payload());
    EXPECT_TRUE(std::filesystem::exists(dir / "m.bin.zst"));
    EXPECT_EQ(FileCount(dir), 2u);  // no temp file left behind
}

TEST(CApi, DecompressZstdRealFrames) {
    const auto dir = ZstdDir("real");
    WriteFile(dir / "h.zst", kZstdHello);
    ASSERT_EQ(tk_decompress_zstd((dir / "h.zst").c_str(), (dir / "h").c_str()), TK_OK)
        << tk_last_error();
    EXPECT_EQ(ReadFile(dir / "h"), "hello model");

    // Concatenated frames decode to the concatenated content; empty content is fine.
    WriteFile(dir / "multi.zst", kZstdHello + ZstdRaw("") + ZstdRaw("!"));
    ASSERT_EQ(tk_decompress_zstd((dir / "multi.zst").c_str(), (dir / "multi").c_str()), TK_OK)
        << tk_last_error();
    EXPECT_EQ(ReadFile(dir / "multi"), "hello model!");
}

TEST(CApi, DecompressZstdRejectsBadInput) {
    const std::string good = ZstdRaw(Payload());
    std::string bad_block_type = good;
    bad_block_type[6] |= 0x06;  // first block type 3: reserved
    std::string bad_checksum = kZstdHello;
    bad_checksum.back() ^= 0x01;

    const struct {
        const char* name;
        std::string bytes;
    } cases[] = {
        {"truncated", good.substr(0, good.size() / 2)},
        {"header only", good.substr(0, 6)},
        {"missing last block", good.substr(0, 6 + 3 + 65536)},
        {"reserved block type", bad_block_type},
        {"bad checksum", bad_checksum},
        {"missing checksum", kZstdHello.substr(0, kZstdHello.size() - 4)},
        {"trailing garbage", kZstdHello + "junk"},
        {"not zstd", "plain model bytes"},
        {"empty file", ""},
    };
    for (const auto& c : cases) {
        const auto dir = ZstdDir("bad");
        WriteFile(dir / "in.zst", c.bytes);
        WriteFile(dir / "out", "previous");
        EXPECT_EQ(tk_decompress_zstd((dir / "in.zst").c_str(), (dir / "out").c_str()), TK_ERR_IO)
            << c.name;
        EXPECT_STRNE(tk_last_error(), "") << c.name;
        EXPECT_EQ(ReadFile(dir / "out"), "previous") << c.name;  // untouched
        EXPECT_EQ(FileCount(dir), 2u) << c.name;                  // no temp file left
    }
}

TEST(CApi, DecompressZstdRejectsBadArgs) {
    const auto dir = ZstdDir("args");
    EXPECT_EQ(tk_decompress_zstd(nullptr, "x"), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_decompress_zstd("x", nullptr), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_decompress_zstd("", "x"), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_decompress_zstd("x", ""), TK_ERR_INVALID_ARG);
    EXPECT_EQ(tk_decompress_zstd((dir / "missing.zst").c_str(), (dir / "out").c_str()), TK_ERR_IO);
    EXPECT_EQ(tk_decompress_zstd((dir / "missing.zst").c_str(), (dir / "no" / "out").c_str()),
              TK_ERR_IO);
    EXPECT_EQ(FileCount(dir), 0u);
}

// Several threads and processes writing the same out_path must not share a
// temp file: each gets its own exclusive one, and the last rename wins whole.
TEST(CApi, DecompressZstdSameOutputConcurrently) {
    const auto dir = ZstdDir("same_out");
    const std::string payload = Payload();
    WriteFile(dir / "in.zst", ZstdRaw(payload));
    const auto in = dir / "in.zst";
    const auto out = dir / "out";

    constexpr int kProcesses = 4;
    std::vector<pid_t> children;
    for (int p = 0; p < kProcesses; ++p) {
        const pid_t pid = fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) _exit(tk_decompress_zstd(in.c_str(), out.c_str()) == TK_OK ? 0 : 1);
        children.push_back(pid);
    }
    std::vector<std::thread> threads;
    std::vector<tk_status> status(4);
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] { status[t] = tk_decompress_zstd(in.c_str(), out.c_str()); });
    }
    for (auto& t : threads) t.join();
    for (pid_t pid : children) {
        int wstatus = 0;
        ASSERT_EQ(waitpid(pid, &wstatus, 0), pid);
        EXPECT_TRUE(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
    }
    for (tk_status s : status) EXPECT_EQ(s, TK_OK);
    EXPECT_EQ(ReadFile(out), payload);
    EXPECT_EQ(FileCount(dir), 2u);  // in.zst + out: no temp files left
}

// A temp file left by a killed process doesn't get in the way.
TEST(CApi, DecompressZstdIgnoresStaleTempFile) {
    const auto dir = ZstdDir("stale");
    WriteFile(dir / "in.zst", kZstdHello);
    WriteFile(dir / "out.tmp-AAAAAA", "stale partial output");
    ASSERT_EQ(tk_decompress_zstd((dir / "in.zst").c_str(), (dir / "out").c_str()), TK_OK)
        << tk_last_error();
    EXPECT_EQ(ReadFile(dir / "out"), "hello model");
    EXPECT_EQ(ReadFile(dir / "out.tmp-AAAAAA"), "stale partial output");  // the caller's to sweep
}

TEST(CApi, DecompressZstdInParallel) {
    const auto dir = ZstdDir("parallel");
    const std::string payload = Payload();
    WriteFile(dir / "in.zst", ZstdRaw(payload));
    WriteFile(dir / "bad.zst", "not zstd");

    // Half the threads succeed and half fail, so per-thread errors are checked too.
    constexpr int kThreads = 8;
    std::vector<tk_status> status(kThreads);
    std::vector<std::string> error(kThreads);
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&, i] {
            const auto in = dir / (i % 2 == 0 ? "in.zst" : "bad.zst");
            const auto out = dir / ("out" + std::to_string(i));
            status[i] = tk_decompress_zstd(in.c_str(), out.c_str());
            error[i] = tk_last_error();
        });
    }
    for (auto& t : threads) t.join();
    for (int i = 0; i < kThreads; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(status[i], TK_OK) << i;
            EXPECT_EQ(error[i], "") << i;
            EXPECT_EQ(ReadFile(dir / ("out" + std::to_string(i))), payload) << i;
        } else {
            EXPECT_EQ(status[i], TK_ERR_IO) << i;
            EXPECT_NE(error[i].find("bad.zst"), std::string::npos) << i;
        }
    }
    EXPECT_EQ(FileCount(dir), 2u + kThreads / 2);
}

}  // namespace
