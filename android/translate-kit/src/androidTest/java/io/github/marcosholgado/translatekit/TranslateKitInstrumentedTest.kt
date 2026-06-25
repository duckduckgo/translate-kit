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

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.AfterClass
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/**
 * On-device validation of the real JNI + native engine path (arm64 ruy/NEON).
 *
 * Native macOS already proves the engine code path via gtest goldens; this test
 * proves the *shipped artifact* end-to-end: Kotlin facade -> JNI shim ->
 * libtranslate-kit.so -> Bergamot + CLD2, running on a device/emulator.
 *
 * The en->de translation cases mirror the native goldens in
 * core/test/c_api_test.cpp and were captured from the real engine, so the two
 * suites stay in lock-step. Tag/attribute assertions are structural invariants
 * (tags are *projected* via alignment, not generated), so they hold regardless
 * of any tiny float differences between the host and NDK toolchains.
 *
 * The translation tests need the tiny en->de test model bundled as an asset
 * (run scripts/fetch-models.sh); they self-skip via assumeTrue when it is
 * absent so a clean checkout still builds and runs.
 */
@RunWith(AndroidJUnit4::class)
class TranslateKitInstrumentedTest {

    private val context: Context get() = ApplicationProvider.getApplicationContext()

    @Before
    fun setUp() {
        TranslateKit.init(context)
        assertTrue(TranslateKit.isInitialized())
    }

    /* ---- language detection (CLD2, self-contained: no model files) -------- */

    @Test
    fun detectLanguage_goldens() {
        val cases = mapOf(
            "This is a simple sentence written in the English language for testing purposes." to "en",
            "Esta es una frase sencilla escrita en el idioma español para realizar algunas pruebas." to "es",
            "Ceci est une phrase simple écrite en langue française afin d'effectuer quelques tests." to "fr",
            "Dies ist ein einfacher Satz, der zu Testzwecken in deutscher Sprache geschrieben wurde." to "de",
            "Questa è una frase semplice scritta in lingua italiana per scopi di test e verifica." to "it",
        )
        for ((text, expected) in cases) {
            val result = TranslateKit.detectLanguage(text)
            assertEquals("detected wrong language for: $text", expected, result.language)
            assertTrue("confidence should be > 0 for: $text", result.confidence > 0f)
        }
    }

    @Test
    fun detectLanguage_emptyIsUnknown() {
        val result = TranslateKit.detectLanguage("")
        assertEquals("und", result.language)
        assertEquals(0f, result.confidence, 0f)
    }

    /* ---- translation (real Bergamot engine, en->de tiny model) ------------ */

    @Test
    fun translatePlainText_enDe() {
        val model = enDeModelOrSkip()
        val out = model.translate("Hello World!", isHtml = false).text
        assertTrue("plain output should contain \"Hallo\": $out", out.contains("Hallo"))
    }

    @Test
    fun translateHtml_complicatedCasesRoundTrip() {
        val model = enDeModelOrSkip()
        val failures = mutableListOf<String>()
        for (case in HTML_CASES) {
            val out = model.translate(case.input, isHtml = true).text
            if (case.exact != null && out != case.exact) {
                failures += "input=${case.input}\n  expected exactly: ${case.exact}\n  got:              $out"
                continue
            }
            val missing = case.mustContain.filterNot { out.contains(it) }
            if (missing.isNotEmpty()) {
                failures += "input=${case.input}\n  output:  $out\n  missing: $missing"
            }
        }
        assertTrue(
            "HTML round-trip failures (${failures.size}/${HTML_CASES.size}):\n" +
                failures.joinToString("\n"),
            failures.isEmpty(),
        )
    }

    private fun enDeModelOrSkip(): TranslationModel {
        sharedModel?.let { return it }
        synchronized(LOCK) {
            sharedModel?.let { return it }
            val modelFile = copyAssetOrNull("$MODEL_DIR/model.intgemm.alphas.bin", "model.intgemm.alphas.bin")
            assumeTrue(
                "tiny en->de test model not bundled (run scripts/fetch-models.sh); skipping engine tests",
                modelFile != null,
            )
            val vocab = copyAsset("$MODEL_DIR/vocab.deen.spm", "vocab.deen.spm")
            val lex = copyAsset("$MODEL_DIR/lex.s2t.bin", "lex.s2t.bin")
            val config = copyAsset("$MODEL_DIR/config.intgemm8bitalpha.yml", "config.intgemm8bitalpha.yml")
            val model = TranslateKit.loadModel(
                ModelSpec(
                    sourceLang = "en",
                    targetLang = "de",
                    modelPath = modelFile!!.absolutePath,
                    vocabPaths = listOf(vocab.absolutePath),
                    shortlistPath = lex.absolutePath,
                    configYaml = config.absolutePath,
                ),
            )
            sharedModel = model
            return model
        }
    }

    private fun copyAsset(assetPath: String, outName: String): File =
        copyAssetOrNull(assetPath, outName)
            ?: throw IllegalStateException("missing bundled asset: $assetPath")

    private fun copyAssetOrNull(assetPath: String, outName: String): File? {
        val out = File(context.filesDir, outName)
        return try {
            context.assets.open(assetPath).use { input ->
                out.outputStream().use { input.copyTo(it) }
            }
            out
        } catch (e: java.io.IOException) {
            null
        }
    }

    private data class HtmlCase(
        val input: String,
        val mustContain: List<String> = emptyList(),
        val exact: String? = null,
    )

    companion object {
        private const val MODEL_DIR = "test-models/ende.student.tiny11"
        private val LOCK = Any()

        @Volatile
        private var sharedModel: TranslationModel? = null

        // Captured from the real engine (see core/test/c_api_test.cpp). Tag and
        // attribute substrings are structural invariants (projected, not
        // generated); "Hallo"/"Welt" etc. are the stable greedy translations.
        private val HTML_CASES = listOf(
            // Canonical inline tag (exact, mirrors the native golden).
            HtmlCase("<b>Hello World!</b>", exact = "<b>Hallo Welt!</b>"),
            // Attribute (href) preserved verbatim; content translated.
            HtmlCase(
                "<a href=\"https://example.com\">Hello World!</a>",
                mustContain = listOf("<a href=\"https://example.com\">", "</a>", "Hallo Welt!"),
            ),
            // Tag repositioned across sentence reordering (detag-and-project).
            HtmlCase(
                "Click <a href=\"https://example.com\">here</a> to continue.",
                mustContain = listOf("<a href=\"https://example.com\">", "</a>", "hier", "Klicken"),
            ),
            // Nested inline tags both project onto the right words.
            HtmlCase(
                "<b>Hello <i>beautiful</i> World!</b>",
                mustContain = listOf("<b>", "<i>", "</i>", "</b>", "Welt"),
            ),
            // Sibling inline tags.
            HtmlCase(
                "<b>Hello</b> <i>World</i>!",
                mustContain = listOf("<b>Hallo</b>", "<i>Welt</i>"),
            ),
            // Void element preserved and repositioned.
            HtmlCase(
                "Hello<br>World",
                mustContain = listOf("<br>", "Hallo", "Welt"),
            ),
            // Block elements translate independently; structure intact.
            HtmlCase(
                "<p>Hello World!</p><p>Good morning!</p>",
                mustContain = listOf("<p>Hallo Welt!</p>", "<p>Guten Morgen!</p>"),
            ),
            // Multiple attributes preserved verbatim.
            HtmlCase(
                "<span class=\"greeting\" data-id=\"42\">Hello World!</span>",
                mustContain = listOf("<span class=\"greeting\" data-id=\"42\">", "</span>", "Hallo Welt!"),
            ),
            // HTML entity survives.
            HtmlCase(
                "Hello &amp; goodbye",
                mustContain = listOf("&amp;", "Hallo"),
            ),
            // The full combo: block + repositioned link + complex href (query +
            // entity) preserved verbatim.
            HtmlCase(
                "<p>Please <a href=\"https://example.com/path?q=1&amp;x=2\">click here</a> now.</p>",
                mustContain = listOf(
                    "<p>",
                    "<a href=\"https://example.com/path?q=1&amp;x=2\">",
                    "</a>",
                    "</p>",
                ),
            ),
        )

        @AfterClass
        @JvmStatic
        fun tearDownClass() {
            sharedModel?.close()
            sharedModel = null
        }
    }
}
