#!/usr/bin/env bash
#
# Copyright 2026 Marcos Holgado
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Applies translate-kit's local patches to the vendored Bergamot engine
# submodule (third_party/translations). Run once after checking out the engine
# submodules:
#
#   git submodule update --init --recursive third_party/translations
#   scripts/apply-engine-patches.sh
#
# Idempotent: patches already applied are skipped. The patches are NOT committed
# into the submodule (we can't — it's upstream); they live in third_party/patches
# and are re-applied to the working tree by this script.
#
# Current patches:
#   0001-marian-git-revision-submodule-path.patch
#     marian's git_revision.h generation resolves the submodule .git path
#     assuming marian-fork is the direct submodule. Here it is nested two levels
#     inside the `translations` submodule, so the relative gitdir must be
#     resolved against the .git file's own directory. Without this, ninja fails:
#     "logs/HEAD ... missing and no known rule to make it".

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_DIR="$REPO_ROOT/third_party/translations"
PATCH_DIR="$REPO_ROOT/third_party/patches"

if [ ! -d "$ENGINE_DIR/inference" ]; then
    echo "error: engine submodule not initialized at $ENGINE_DIR" >&2
    echo "  run: git submodule update --init --recursive third_party/translations" >&2
    exit 1
fi

shopt -s nullglob
for patch in "$PATCH_DIR"/*.patch; do
    name="$(basename "$patch")"
    if git -C "$ENGINE_DIR" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "already applied: $name"
    elif git -C "$ENGINE_DIR" apply --check "$patch" >/dev/null 2>&1; then
        git -C "$ENGINE_DIR" apply "$patch"
        echo "applied: $name"
    else
        echo "error: cannot apply $name cleanly (engine version drift?)" >&2
        exit 1
    fi
done

echo "engine patches up to date."
