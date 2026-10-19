#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build an installable Zuma Deluxe Portable APK for Android.
#
# This is the "one command" path: it fetches SDL2, builds the native
# dependencies for the target ABI with your NDK, builds libmain.so, packages
# everything and hands you an APK under dist/.
#
# Usage:
#   scripts/build-android-apk.sh [abi] [--release]
#
#   abi        arm64-v8a (default), armeabi-v7a, x86_64, x86
#   --release  build a release APK (signed with your debug key unless
#              PVZP_KEYSTORE_* env vars are set)
#
# Requirements:
#   * JDK 17 or newer on PATH ($JAVA_HOME recommended)
#   * Android SDK with: platform android-36, build-tools 35.x,
#     CMake 3.22.1, NDK 27.2.12479018
#   * Android SDK location in $ANDROID_HOME (or $ANDROID_SDK_ROOT, or a
#     standard install path)
#   * curl + tar (git-bash on Windows provides both)
#
# Everything is cached: re-running only rebuilds what changed.
# ---------------------------------------------------------------------------
set -euo pipefail

ABI="arm64-v8a"
BUILD_TYPE="debug"
for arg in "$@"; do
    case "$arg" in
        --release) BUILD_TYPE="release" ;;
        arm64-v8a|armeabi-v7a|x86_64|x86) ABI="$arg" ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
API_LEVEL=24
SDL2_VERSION="2.32.10"
LIBPNG_VERSION="1.6.44"
LIBJPEG_VERSION="3.0.4"
NDK_PIN="27.2.12479018"

say()  { printf '\n== %s\n' "$*"; }
fail() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }

# Native tools (cmake, ninja, gradle) are Windows binaries when running under
# git-bash, so every path handed to them has to be a Windows path.
wp() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi
}

# --- toolchain discovery ---------------------------------------------------
say "Locating Android SDK / NDK / JDK"

SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK" ]; then
    for cand in \
        "$HOME/Android/Sdk" \
        "$HOME/Library/Android/sdk" \
        "$HOME/AppData/Local/Android/Sdk" \
        "/c/Users/$USER/AppData/Local/Android/Sdk" \
        /usr/lib/android-sdk
    do
        [ -d "$cand" ] && SDK="$cand" && break
    done
fi
[ -n "$SDK" ] && [ -d "$SDK" ] || fail "Android SDK not found. Set ANDROID_HOME to your SDK directory."
SDK_W="$(wp "$SDK")"

NDK="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK" ]; then
    if [ -d "$SDK/ndk/$NDK_PIN" ]; then
        NDK="$SDK/ndk/$NDK_PIN"
    elif [ -d "$SDK/ndk" ]; then
        NDK="$(ls -d "$SDK"/ndk/*/ 2>/dev/null | sort -V | tail -1)"
        NDK="${NDK%/}"
    fi
fi
[ -n "$NDK" ] && [ -d "$NDK" ] || fail "Android NDK not found. Install NDK $NDK_PIN via Android Studio -> SDK Manager, or set ANDROID_NDK_HOME."
NDK_W="$(wp "$NDK")"

command -v java >/dev/null 2>&1 || fail "java not found. Install JDK 17+ and set JAVA_HOME."
JAVA_MAJOR="$(java -version 2>&1 | sed -nE 's/.*version "([0-9]+).*/\1/p' | head -1)"
[ -n "$JAVA_MAJOR" ] && [ "$JAVA_MAJOR" -ge 17 ] 2>/dev/null \
    || fail "JDK 17+ required (found: $(java -version 2>&1 | head -1))."

CMAKE=""
if [ -x "$SDK/cmake/3.22.1/bin/cmake" ] || [ -f "$SDK/cmake/3.22.1/bin/cmake.exe" ]; then
    CMAKE="$SDK/cmake/3.22.1/bin/cmake"
else
    for d in $(ls -d "$SDK"/cmake/*/bin 2>/dev/null | sort -V -r); do
        if [ -x "$d/cmake" ] || [ -f "$d/cmake.exe" ]; then CMAKE="$d/cmake"; break; fi
    done
fi
[ -n "$CMAKE" ] || CMAKE="$(command -v cmake || true)"
[ -n "$CMAKE" ] || fail "CMake not found. Install it from Android Studio -> SDK Manager (SDK Tools -> CMake 3.22.1) or your package manager."

NINJA=""
if [ -x "$(dirname "$CMAKE")/ninja" ] || [ -f "$(dirname "$CMAKE")/ninja.exe" ]; then
    NINJA="$(dirname "$CMAKE")/ninja"
else
    NINJA="$(command -v ninja || true)"
fi
[ -n "$NINJA" ] || fail "Ninja not found. It ships with the Android SDK CMake package; install CMake 3.22.1 from SDK Manager."

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST_TAG="windows-x86_64" ;;
    Darwin)               HOST_TAG="darwin-x86_64" ;;
    *)                    HOST_TAG="linux-x86_64" ;;
esac
STRIP="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/llvm-strip"
[ -x "$STRIP" ] || fail "llvm-strip not found under $NDK/toolchains/llvm/prebuilt/$HOST_TAG."

echo "  SDK    : $SDK"
echo "  NDK    : $NDK"
echo "  CMake  : $CMAKE"
echo "  Ninja  : $NINJA"
echo "  ABI    : $ABI (API $API_LEVEL, $BUILD_TYPE)"

# --- SDL2 sources (needed for SDLActivity.java and the .so) ----------------
say "Fetching SDL2 $SDL2_VERSION sources"
CACHE="$ROOT/.cache"
mkdir -p "$CACHE"
if [ ! -d "$ROOT/sdl2-src" ]; then
    TARBALL="$CACHE/SDL2-$SDL2_VERSION.tar.gz"
    [ -f "$TARBALL" ] || curl -fL --retry 3 -o "$(wp "$TARBALL")" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz"
    tar xzf "$TARBALL" -C "$CACHE"
    mv "$CACHE/SDL2-$SDL2_VERSION" "$ROOT/sdl2-src"
fi

# --- native dependencies ---------------------------------------------------
DEPS="${ZUMA_ANDROID_DEPS:-$ROOT/android-deps/$ABI}"
DEPS_W="$(wp "$DEPS")"
DEPS_SRC="$ROOT/android-deps/src"
DEPS_BUILD="$ROOT/android-deps/build-$ABI"
mkdir -p "$DEPS" "$DEPS_SRC" "$DEPS_BUILD"
export ZUMA_ANDROID_DEPS="$DEPS_W"

TOOLCHAIN_W="$(wp "$NDK/build/cmake/android.toolchain.cmake")"
NINJA_W="$(wp "$NINJA")"
CMAKE_ARGS=(
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=$NINJA_W"
    "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_W"
    "-DANDROID_ABI=$ABI"
    "-DANDROID_PLATFORM=android-$API_LEVEL"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_INSTALL_PREFIX=$DEPS_W"
)

fetch_src() { # url tarball-dir-name dest-name
    local url="$1" tar_name="$2" dest="$3"
    [ -d "$DEPS_SRC/$dest" ] && return 0
    local tarball="$CACHE/$tar_name"
    [ -f "$tarball" ] || curl -fL --retry 3 -o "$(wp "$tarball")" "$url"
    mkdir -p "$DEPS_SRC/_unpack"
    tar xzf "$tarball" -C "$DEPS_SRC/_unpack"
    mv "$DEPS_SRC/_unpack/"* "$DEPS_SRC/$dest"
    rmdir "$DEPS_SRC/_unpack"
}

say "Building libpng $LIBPNG_VERSION"
fetch_src "https://github.com/pnggroup/libpng/archive/refs/tags/v$LIBPNG_VERSION.tar.gz" "libpng.tar.gz" "libpng-src"
if [ ! -f "$DEPS/lib/libpng.so" ]; then
    "$CMAKE" -S "$(wp "$DEPS_SRC/libpng-src")" -B "$(wp "$DEPS_BUILD/libpng")" "${CMAKE_ARGS[@]}" \
        -DPNG_SHARED=ON -DPNG_STATIC=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF \
        "-DZLIB_INCLUDE_DIR=$NDK_W/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/include" >/dev/null
    "$CMAKE" --build "$(wp "$DEPS_BUILD/libpng")" --target install -j 8 >/dev/null
fi
echo "  ok"

say "Building libjpeg-turbo $LIBJPEG_VERSION"
fetch_src "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$LIBJPEG_VERSION.tar.gz" "libjpeg-turbo.tar.gz" "libjpeg-turbo-src"
if [ ! -f "$DEPS/lib/libjpeg.so" ]; then
    "$CMAKE" -S "$(wp "$DEPS_SRC/libjpeg-turbo-src")" -B "$(wp "$DEPS_BUILD/libjpeg")" "${CMAKE_ARGS[@]}" \
        -DENABLE_SHARED=ON -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_SIMD=0 >/dev/null
    "$CMAKE" --build "$(wp "$DEPS_BUILD/libjpeg")" --target install -j 8 >/dev/null
fi
echo "  ok"

say "Building SDL2 $SDL2_VERSION"
if [ ! -f "$DEPS/lib/libSDL2.so" ]; then
    "$CMAKE" -S "$(wp "$ROOT/sdl2-src")" -B "$(wp "$DEPS_BUILD/sdl2")" "${CMAKE_ARGS[@]}" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF >/dev/null
    "$CMAKE" --build "$(wp "$DEPS_BUILD/sdl2")" --target install -j 8 >/dev/null
fi
echo "  ok"

# --- game native library ---------------------------------------------------
say "Building libmain.so"
BUILD_NATIVE="$ROOT/build-android-$ABI"
"$CMAKE" -S "$(wp "$ROOT")" -B "$(wp "$BUILD_NATIVE")" "${CMAKE_ARGS[@]}" >/dev/null
"$CMAKE" --build "$(wp "$BUILD_NATIVE")" -j 8

# --- package the .so files ------------------------------------------------
say "Collecting native libraries"
JNILIBS="$ROOT/android/app/src/main/jniLibs/$ABI"
mkdir -p "$JNILIBS"
cp -f "$BUILD_NATIVE/libmain.so" "$JNILIBS/"
cp -f "$DEPS"/lib/libSDL2*.so "$JNILIBS/" 2>/dev/null || true
cp -f "$DEPS"/lib/libpng*.so "$JNILIBS/" 2>/dev/null || true
cp -f "$DEPS"/lib/libjpeg*.so "$JNILIBS/" 2>/dev/null || true
for so in "$JNILIBS"/*.so; do
    [ -f "$so" ] && "$STRIP" --strip-unneeded "$(wp "$so")"
done
ls -1 "$JNILIBS"

# --- gradle ----------------------------------------------------------------
say "Packaging the APK with Gradle"
cd "$ROOT/android"
printf 'sdk.dir=%s\n' "$SDK_W" > local.properties
GRADLE_TASK="assembleDebug"
[ "$BUILD_TYPE" = "release" ] && GRADLE_TASK="assembleRelease"
# The gradlew shell script builds a classpath with POSIX paths, which the native
# Windows JVM cannot resolve, so on Windows invoke the wrapper jar with a native
# path directly. Elsewhere the standard script is fine.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        java -classpath "$(wp "$ROOT/android/gradle/wrapper/gradle-wrapper.jar")" \
            org.gradle.wrapper.GradleWrapperMain "$GRADLE_TASK" -PusePrebuiltLibs=true --no-daemon ;;
    *)
        ./gradlew "$GRADLE_TASK" -PusePrebuiltLibs=true --no-daemon ;;
esac

mkdir -p "$ROOT/dist"
APK_SRC=$(find "$ROOT/android/app/build/outputs/apk" -name '*.apk' -newermt '-10 minutes' | head -1)
[ -n "$APK_SRC" ] || APK_SRC=$(find "$ROOT/android/app/build/outputs/apk" -name '*.apk' | sort | tail -1)
APK_OUT="$ROOT/dist/zuma-portable-android-$ABI.apk"
cp -f "$APK_SRC" "$APK_OUT"

say "Done"
echo "  APK: $APK_OUT"
echo "  Size: $(du -h "$APK_OUT" | cut -f1)"
echo
echo "Next: copy the APK to your phone, install it, then point the app at your"
echo "Zuma Deluxe folder (properties/, images/, sounds/, levels/, music/, fonts/)."
