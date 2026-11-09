# translate-kit

On-device **neural machine translation** + **offline language detection** as a reusable native library.

`translate-kit` wraps the [Bergamot](https://github.com/mozilla/translations) translation engine
(a fork of Marian NMT with quantized int8 student models) and the bundled
[CLD2](https://github.com/CLD2Owners/cld2) Compact Language Detector, and exposes them through a thin,
platform-neutral C ABI. It ships two platform wrappers today — **Android** (JNI → Kotlin, published as a
Maven AAR) and **Apple** (macOS/iOS, a Swift package over a prebuilt XCFramework) — both layered on the
same core, which is structured so other platforms (e.g. Windows) can reuse it later.

CLD2 is self-contained — its language profiles are compiled into the library, so language detection
needs no model file and no network.

> **Privacy:** the library never touches the network. It does no model downloading and consumes only
> on-disk model paths supplied by the caller. Everything stays on-device.

## Requirements

**Android**
- **minSdk 28.** Forced by the Bergamot engine: marian's `pathie-cpp` uses `glob()`/`iconv()`, which
  Android bionic only provides at API 28+. Consumers on a lower app minSdk gate the feature off below 28.
- **NDK 28.2.13676358**, CMake 3.22+. Language detection (CLD2) alone has no such floor.

**Apple**
- **macOS 12+ / iOS 15+** deployment targets.
- **Xcode** (Swift 5.9+) to consume the Swift package. Building the XCFramework from source additionally
  needs **CMake 3.22+** and **Ninja**.

## Repository layout

```
core/        platform-agnostic C++ engine wrapper + the C ABI (include/translate_kit/translate_kit.h)
android/     Android Gradle project — the AAR wrapper (JNI shim + Kotlin facade)
apple/       Apple wrapper — Swift package (TranslateKit) over the prebuilt XCFramework
models/      test-model fixtures (fetched by scripts/, gitignored) — debug/test only, never in the release AAR
scripts/     cross-compile + model-fetch + engine-patch helpers
third_party/ git submodules: cld2 (detector), translations (Bergamot engine); patches/ (engine patches)
```

The translation engine wrapper lives in `core/` and is consumed by every platform wrapper through the
C ABI in `core/include/translate_kit/translate_kit.h`. The Android module builds it via Gradle
`externalNativeBuild` (CMake); other platforms use `scripts/build-*.sh`.

## Building from source

```bash
# 1. Engine submodules (the inference subset is enough for the build):
git submodule update --init --checkout --recursive third_party/cld2
git submodule update --init --checkout third_party/zstd
git submodule update --init --checkout third_party/translations
git -C third_party/translations submodule update --init --recursive \
    inference/3rd_party/ssplit-cpp \
    inference/marian-fork/src/3rd_party/sentencepiece \
    inference/marian-fork/src/3rd_party/intgemm \
    inference/marian-fork/src/3rd_party/ruy \
    inference/marian-fork/src/3rd_party/simd_utils

# 2. Apply translate-kit's local engine patches (see third_party/patches/):
scripts/apply-engine-patches.sh
```

Steps 1–2 are shared by all platforms; the build itself is per-platform.

**Android** (JDK 17/21) — builds the AAR:

```bash
cd android && ./gradlew :translate-kit:assembleRelease
```

**Apple** (Xcode + CMake/Ninja) — builds the per-slice merged static libs and assembles
`apple/build/TranslateKit.xcframework`:

```bash
scripts/build-apple.sh                            # all 5 slices (universal macOS + iOS device + sim)
scripts/build-apple.sh macos-arm64 iossim-arm64   # or a subset; the XCFramework is assembled from
                                                  # whatever was built (this is what CI builds)
```

## Android usage (Kotlin)

```kotlin
TranslateKit.init(context)                 // loads the native library + inits the engine. Idempotent. Blocking.

val lang = TranslateKit.detectLanguage(text)            // LanguageResult(language, isReliable, topLanguages)

val model = TranslateKit.loadModel(
    ModelSpec(
        sourceLang = "es",
        targetLang = "en",
        modelPath = "/…/model.esen.intgemm.alphas.bin",
        vocabPaths = listOf("/…/vocab.esen.spm"),        // 1 (shared) or 2 (src+tgt)
        shortlistPath = "/…/lex.50.50.esen.s2t.bin",
        configYaml = "/…/config.esen.yml",               // pre-generated off-device
    ),
)
val result = model.translate(htmlFragment, isHtml = true)  // inline tags preserved (detag-and-project)
model.close()
```

All native calls are **blocking** — call them off the main thread (e.g. a coroutine on an IO dispatcher).
`detectLanguage` returns CLD2's overall pick (`language`, `null` when undetermined), its own `isReliable`
verdict, and up to three `topLanguages` with their percentage of the text. Only offer translation when
`isReliable` is true.
`translate(input, isHtml = true)` preserves inline HTML tags via the engine's alignment-based
detag-and-project. `translate(inputs: List<String>, isHtml)` translates a whole list in one engine call, about
2× faster than one call per input. A sentence's output can differ slightly from a single call's, because
the engine's output-vocabulary shortlist is shared across the batch (in testing, usually closer to the
model's unrestricted output). The library is single-hop per loaded model; non-English↔non-English pairs are pivoted
through English by the caller.

## Apple usage (Swift)

```swift
import TranslateKit

let kit = try TranslateKit()                            // inits the engine. Blocking.

let lang = try kit.detectLanguage(text)                 // LanguageResult(language, isReliable, topLanguages)

let model = try kit.loadModel(
    ModelSpec(
        sourceLang: "es",
        targetLang: "en",
        modelPath: "/…/model.esen.intgemm.alphas.bin",
        vocabPaths: ["/…/vocab.esen.spm"],              // 1 (shared) or 2 (src+tgt)
        shortlistPath: "/…/lex.50.50.esen.s2t.bin",
        configYaml: "/…/config.esen.yml"                // pre-generated off-device; nil → engine defaults
    )
)
let result = try model.translate(htmlFragment, isHtml: true)  // inline tags preserved
model.close()
```

The Swift API mirrors the Android facade 1:1: `TranslateKit` (`init()` / `detectLanguage(_:)` /
`loadModel(_:)` / `unloadModel(_:)`, plus the static `decompressZstd(at:to:)`) and `TranslationModel` (`translate(_:isHtml:)`, a batch overload,
and `close()` — also wired to `deinit`). Language detection (CLD2) needs no model file. All calls are
**blocking** — invoke them off the main thread (e.g. a background `Task`/`DispatchQueue`); a `TranslateKit`
and the `TranslationModel`s loaded from it are **not** thread-safe, so serialize access. Deployment
targets are **macOS 12 / iOS 15**.

The library is distributed as a prebuilt XCFramework via Swift Package Manager — see
[Coordinates](#coordinates) below.

## Coordinates

**Android (Maven Central)**

```
com.duckduckgo.translatekit:translate-kit-android:<version>
```

**Apple (Swift Package Manager)**

Add the package by Git URL + version; SwiftPM downloads the prebuilt XCFramework from the matching
GitHub Release, so consumers don't clone this repo or build the C++ engine:

```swift
// In your Package.swift dependencies:
.package(url: "https://github.com/duckduckgo/translate-kit.git", from: "0.1.0"),
// …then add the "TranslateKit" product to your target's dependencies.
```

Both artifacts share one version and are published together by the `Release` workflow
(`.github/workflows/release.yml`).

## Model-file contract

Identical on both wrappers: the library consumes **decompressed** model files (Mozilla ships them
zstd-compressed; decompress each file once with `decompressZstd`, see below) plus a pre-generated Bergamot config YAML (the config path is
**nullable** — omit it to fall back to the engine's per-architecture defaults). Nothing is bundled and
nothing is downloaded — the library only opens the on-disk paths you pass it. See `SPEC-1 §10a`.

`TranslateKit.decompressZstd(zstPath, outPath)` (Kotlin) / `TranslateKit.decompressZstd(at:to:)` (Swift)
streams one `.zst` to disk. The output only appears once fully decompressed, so a crash or a truncated
download never leaves a partial model behind. It can't catch every corruption: damaged data in a file
without a zstd checksum can decode to wrong bytes. Verify each file against a known hash (e.g. a SHA-256
of the `.zst` before decompressing, or of the output after). The `.zst` is left in place. It is blocking and
thread-safe, so the model, vocab and shortlist can be decompressed in parallel.

## License

Apache-2.0 (this project). Bundled/linked third-party components retain their own licenses — see
[`NOTICE`](NOTICE) and `THIRD_PARTY_LICENSES.md`.
