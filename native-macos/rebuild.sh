#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

mkdir -p "$ROOT_DIR/build/sfinder-classes"
find "$ROOT_DIR/solution-finder/src/main/java" -name '*.java' -print0 \
  | xargs -0 javac --release 8 -encoding UTF-8 \
      -cp "$ROOT_DIR/solution-finder-1.43/sfinder.jar" \
      -d "$ROOT_DIR/build/sfinder-classes"

mkdir -p "$ROOT_DIR/native-macos/lib" "$ROOT_DIR/native-macos/bin"
jar cfe "$ROOT_DIR/native-macos/lib/sfinder-from-source.jar" Main \
  -C "$ROOT_DIR/build/sfinder-classes" .

rm -rf "$ROOT_DIR/build/commons-cli-only"
mkdir -p "$ROOT_DIR/build/commons-cli-only"
cd "$ROOT_DIR/build/commons-cli-only"
jar xf "$ROOT_DIR/solution-finder-1.43/sfinder.jar" org/apache/commons/cli
jar cf "$ROOT_DIR/native-macos/lib/sfinder-deps.jar" -C "$ROOT_DIR/build/commons-cli-only" org

cd "$ROOT_DIR"
clang -Wall -Wextra -O2 native-macos/sfinder_launcher.c -o native-macos/bin/sfinder
