# Bundled model data

Shared, on-device data packaged into every platform wrapper (e.g. the Android AAR's `assets/`).

These artifacts are **fetched, not committed** (see `scripts/fetch-models.sh`); the binaries are
`.gitignore`d. The directory is kept so the Android `assets.srcDirs` mapping resolves even before the
fetch runs.

| File | Purpose | Source / License | Phase |
|------|---------|------------------|-------|
| `lid.176.ftz` | fastText compressed language-ID model (~917 KB) | fastText, MIT | B |
| `nonbreaking_prefix.<lang>` | ssplit-cpp sentence-boundary rules | ssplit-cpp, MIT | C |

> Translation **NMT** models are NOT bundled — they are downloaded on demand by the consuming app
> (Spec 2) and passed to the library as on-disk paths.
