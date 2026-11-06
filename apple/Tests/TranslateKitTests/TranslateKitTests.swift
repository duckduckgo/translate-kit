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

import XCTest
import TranslateKit

/// Swift goldens mirroring core/test/c_api_test.cpp (and the Android
/// instrumented goldens) so the macOS Swift layer stays in lock-step with the
/// C ABI. Translation cases need a real tiny en->de model: set TK_TEST_MODEL_DIR
/// to an extracted ende.student.tiny11 directory (see scripts/fetch-models.sh);
/// they are skipped when it is not set.
final class TranslateKitTests: XCTestCase {

    private var modelDirectory: String? {
        guard let dir = ProcessInfo.processInfo.environment["TK_TEST_MODEL_DIR"],
              !dir.isEmpty else { return nil }
        return dir
    }

    private func requireModelDirectory() throws -> String {
        guard let dir = modelDirectory else {
            throw XCTSkip("set TK_TEST_MODEL_DIR to the ende.student.tiny11 directory")
        }
        return dir
    }

    private func enDeSpec(_ dir: String) -> ModelSpec {
        ModelSpec(
            sourceLang: "en",
            targetLang: "de",
            modelPath: "\(dir)/model.intgemm.alphas.bin",
            vocabPaths: ["\(dir)/vocab.deen.spm"],
            shortlistPath: "\(dir)/lex.s2t.bin",
            configYaml: "\(dir)/config.intgemm8bitalpha.yml",
            numWorkers: 1
        )
    }

    // MARK: - Version

    func testVersionIsNonEmpty() {
        XCTAssertFalse(TranslateKit.version.isEmpty)
    }

    // MARK: - Language detection (CLD2, no model file)

    func testDetectLanguageGoldens() throws {
        let kit = try TranslateKit()
        let cases: [(text: String, expected: String)] = [
            ("This is a simple sentence written in the English language for testing purposes.", "en"),
            ("Esta es una frase sencilla escrita en el idioma español para realizar algunas pruebas.", "es"),
            ("Ceci est une phrase simple écrite en langue française afin d'effectuer quelques tests.", "fr"),
            ("Dies ist ein einfacher Satz, der zu Testzwecken in deutscher Sprache geschrieben wurde.", "de"),
            ("Questa è una frase semplice scritta in lingua italiana per scopi di test e verifica.", "it"),
        ]
        for testCase in cases {
            let result = try kit.detectLanguage(testCase.text)
            XCTAssertEqual(result.language, testCase.expected, testCase.text)
            XCTAssertTrue(result.isReliable, testCase.text)
            XCTAssertEqual(result.topLanguages.first?.language, testCase.expected, testCase.text)
        }
    }

    func testDetectEmptyIsUndetermined() throws {
        let kit = try TranslateKit()
        let result = try kit.detectLanguage("")
        XCTAssertEqual(result, LanguageResult(language: nil, isReliable: false, topLanguages: []))
    }

    // Mirrors c_api_test.cpp's mixed case: English leads by share, CLD2 picks Spanish.
    func testDetectMixedReportsTopLanguages() throws {
        let kit = try TranslateKit()
        let result = try kit.detectLanguage(
            "Esta es una frase sencilla escrita en el idioma español para realizar algunas pruebas. "
                + "This is a simple sentence written in the English language for testing purposes.")
        XCTAssertEqual(result.language, "es")
        XCTAssertTrue(result.isReliable)
        XCTAssertEqual(result.topLanguages.map(\.language), ["en", "es"])
        XCTAssertTrue(result.topLanguages.allSatisfy { $0.percent > 40 }, "\(result.topLanguages)")
    }

    // MARK: - Errors (engine-independent)

    func testLoadModelEmptyPathThrowsInvalidArgument() throws {
        let kit = try TranslateKit()
        let spec = ModelSpec(
            sourceLang: "es", targetLang: "en",
            modelPath: "", vocabPaths: [], shortlistPath: ""
        )
        XCTAssertThrowsError(try kit.loadModel(spec)) { error in
            guard case TranslateKitError.invalidArgument = error else {
                return XCTFail("expected .invalidArgument, got \(error)")
            }
        }
    }

    // MARK: - Decompression (engine-independent)

    func testDecompressZstd() throws {
        let dir = FileManager.default.temporaryDirectory
            .appendingPathComponent("tk-zstd-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: dir) }

        // "hello model" from the zstd CLI (-19 --check).
        let zst: [UInt8] = [
            0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x68, 0x59, 0x00, 0x00, 0x68, 0x65, 0x6c,
            0x6c, 0x6f, 0x20, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x95, 0x9e, 0x11, 0x21,
        ]
        let zstURL = dir.appendingPathComponent("model.bin.zst")
        let outURL = dir.appendingPathComponent("model.bin")
        try Data(zst).write(to: zstURL)

        try TranslateKit.decompressZstd(at: zstURL.path, to: outURL.path)
        XCTAssertEqual(try String(contentsOf: outURL, encoding: .utf8), "hello model")

        let badURL = dir.appendingPathComponent("bad.zst")
        try Data(zst.prefix(zst.count - 4)).write(to: badURL)
        XCTAssertThrowsError(try TranslateKit.decompressZstd(at: badURL.path, to: outURL.path)) { error in
            guard case TranslateKitError.io = error else {
                return XCTFail("expected .io, got \(error)")
            }
        }
        XCTAssertEqual(try String(contentsOf: outURL, encoding: .utf8), "hello model")
    }

    // MARK: - Translation (real tiny en->de model)

    func testTranslatePlainText() throws {
        let dir = try requireModelDirectory()
        let kit = try TranslateKit()
        let model = try kit.loadModel(enDeSpec(dir))
        defer { model.close() }

        let result = try model.translate("Hello World!", isHtml: false)
        XCTAssertTrue(result.text.contains("Hallo"), "plain output: \(result.text)")
    }

    func testTranslateHtmlRoundTrips() throws {
        let dir = try requireModelDirectory()
        let kit = try TranslateKit()
        let model = try kit.loadModel(enDeSpec(dir))
        defer { model.close() }

        // Mirrors c_api_test.cpp HTML_CASES: tag/attribute substrings are
        // structural invariants (projected via alignment), German tokens are the
        // stable greedy (beam-size 1) outputs. `exact` checks the whole string;
        // otherwise every `contains` substring must be present.
        struct HtmlCase {
            let input: String
            let mustContain: [String]
            let exact: String?
            init(_ input: String, contains: [String] = [], exact: String? = nil) {
                self.input = input
                self.mustContain = contains
                self.exact = exact
            }
        }
        let cases: [HtmlCase] = [
            HtmlCase("<b>Hello World!</b>", exact: "<b>Hallo Welt!</b>"),
            HtmlCase("<a href=\"https://example.com\">Hello World!</a>",
                     contains: ["<a href=\"https://example.com\">", "</a>", "Hallo Welt!"]),
            HtmlCase("Click <a href=\"https://example.com\">here</a> to continue.",
                     contains: ["<a href=\"https://example.com\">", "</a>", "hier", "Klicken"]),
            HtmlCase("<b>Hello <i>beautiful</i> World!</b>",
                     contains: ["<b>", "<i>", "</i>", "</b>", "Welt"]),
            HtmlCase("<b>Hello</b> <i>World</i>!",
                     contains: ["<b>Hallo</b>", "<i>Welt</i>"]),
            HtmlCase("Hello<br>World", contains: ["<br>", "Hallo", "Welt"]),
            HtmlCase("<p>Hello World!</p><p>Good morning!</p>",
                     contains: ["<p>Hallo Welt!</p>", "<p>Guten Morgen!</p>"]),
            HtmlCase("<span class=\"greeting\" data-id=\"42\">Hello World!</span>",
                     contains: ["<span class=\"greeting\" data-id=\"42\">", "</span>", "Hallo Welt!"]),
            HtmlCase("Hello &amp; goodbye", contains: ["&amp;", "Hallo"]),
            HtmlCase("<p>Please <a href=\"https://example.com/path?q=1&amp;x=2\">click here</a> now.</p>",
                     contains: ["<p>", "<a href=\"https://example.com/path?q=1&amp;x=2\">", "</a>", "</p>"]),
        ]
        for testCase in cases {
            let output = try model.translate(testCase.input, isHtml: true).text
            if let exact = testCase.exact {
                XCTAssertEqual(output, exact, "input: \(testCase.input)")
            } else {
                for needle in testCase.mustContain {
                    XCTAssertTrue(output.contains(needle),
                                  "input: \(testCase.input)\n  output: \(output)\n  missing: \(needle)")
                }
            }
        }
    }

    func testBatchTranslate() throws {
        let dir = try requireModelDirectory()
        let kit = try TranslateKit()
        let model = try kit.loadModel(enDeSpec(dir))
        defer { model.close() }

        let results = try model.translate(["Hello World!", "Good morning!"], isHtml: false)
        XCTAssertEqual(results.count, 2)
        XCTAssertTrue(results[0].text.contains("Hallo"), "got: \(results[0].text)")
        XCTAssertTrue(results[1].text.contains("Guten"), "got: \(results[1].text)")
    }

    // Loading a second model in the same process must succeed (engine patch
    // 0007, idempotent marian loggers). The host app loads several models per
    // process; pivoting needs two loaded at once.
    func testLoadMultipleModelsInOneProcess() throws {
        let dir = try requireModelDirectory()
        let kit = try TranslateKit()

        let first = try kit.loadModel(enDeSpec(dir))
        defer { first.close() }
        let second = try kit.loadModel(enDeSpec(dir))
        defer { second.close() }

        for model in [first, second] {
            let result = try model.translate("Hello World!", isHtml: false)
            XCTAssertTrue(result.text.contains("Hallo"), "got: \(result.text)")
        }
    }
}
