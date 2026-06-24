# Bundled model data

Shared, on-device data packaged into every platform wrapper (e.g. the Android AAR's `assets/`).

These artifacts are **fetched, not committed** (see `scripts/fetch-models.sh`); the binaries are
`.gitignore`d. The directory is kept so the Android `assets.srcDirs` mapping resolves even before the
fetch runs.

| File | Purpose | Source / License | Phase |
|------|---------|------------------|-------|
| `nonbreaking_prefix.<lang>` | ssplit-cpp sentence-boundary rules | ssplit-cpp, MIT | C |

> **Language detection needs no bundled model.** It uses CLD2, whose language profiles are compiled
> directly into the native library (Apache-2.0).
>
> Translation **NMT** models are NOT bundled either — they are downloaded on demand by the consuming
> app (Spec 2) and passed to the library as on-disk paths.
