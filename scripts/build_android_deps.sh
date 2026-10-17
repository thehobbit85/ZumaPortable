#!/usr/bin/env bash
# Build the native dependency set the SexyAppFramework needs for Android/arm64:
#   libpng + libjpeg-turbo (image decoding; the framework does not bundle them)
#   SDL2                  (windowing/input/audio)
# Installed into android-deps/<abi> so the app's CMake can find them.
#
# NOTE for future me: the CMake here is a NATIVE Windows binary, so every path
# handed to it must be a Windows-style path (C:/...). MSYS curl also fails
# writing to some paths on this box, so downloads go through Python's urllib.
#
# Usage: scripts/build_android_deps.sh [abi]   (default arm64-v8a)
set -u

NDK_W=C:/Hermes/tools/android-sdk/ndk/27.2.12479018
NDK=/c/Hermes/tools/android-sdk/ndk/27.2.12479018
CMAKE_W=C:/Hermes/tools/android-sdk/cmake/3.22.1/bin/cmake.exe
NINJA_W=C:/msys64/mingw64/bin/ninja.exe
PY=/c/Hermes/.cache/runtimes/windows-x64/venv/Scripts/python

ABI="${1:-arm64-v8a}"
API=24

BASE_W=C:/Users/admin/zuma-port/android-deps
BASE=/c/Users/admin/zuma-port/android-deps
PREFIX_W="$BASE_W/$ABI"
SRC_W="$BASE_W/src"
SRC="$BASE/src"
BUILD_W="$BASE_W/build-$ABI"
mkdir -p "$SRC" "$BASE/build-$ABI" "$BASE/$ABI"

TOOLCHAIN_W="$NDK_W/build/cmake/android.toolchain.cmake"

common_args=(
  -G Ninja
  "-DCMAKE_MAKE_PROGRAM=$NINJA_W"
  "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_W"
  "-DANDROID_ABI=$ABI"
  "-DANDROID_PLATFORM=android-$API"
  -DCMAKE_BUILD_TYPE=Release
  "-DCMAKE_INSTALL_PREFIX=$PREFIX_W"
)

download() { # url -> file (python, Windows path safe)
  local url="$1" out="$2"
  "$PY" -c "import sys,urllib.request; urllib.request.urlretrieve(sys.argv[1], sys.argv[2]); print('downloaded', sys.argv[2])" "$url" "$out"
}

fetch() { # url tarball name destdir
  local url="$1" tar="$2" dir="$3"
  if [ -d "$SRC/$dir" ]; then
    echo "--- $dir already present"
    return 0
  fi
  echo "--- fetching $dir"
  # Python here is a native Windows build, so it needs a Windows-style path.
  download "$url" "C:/Users/admin/zuma-port/android-deps/$tar" || return 1
  tar xzf "$BASE/$tar" -C "$SRC" || return 1
  local extracted
  extracted=$(ls -d "$SRC"/"${dir%%-src}"-* 2>/dev/null | head -1)
  [ -n "$extracted" ] && mv "$extracted" "$SRC/$dir"
  [ -d "$SRC/$dir" ] || { echo "--- unpack shape unexpected for $dir"; ls "$SRC" | head; return 1; }
  return 0
}

echo "############ 1/3 libpng ############"
if fetch https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.44.tar.gz libpng.tar.gz libpng-src; then
  "$CMAKE_W" -S "$SRC_W/libpng-src" -B "$BUILD_W/libpng" "${common_args[@]}" \
      -DPNG_SHARED=ON -DPNG_STATIC=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF \
      -DZLIB_INCLUDE_DIR="$NDK_W/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include" \
      2>&1 | tail -10
  "$CMAKE_W" --build "$BUILD_W/libpng" --target install -j 8 2>&1 | tail -8
  echo "libpng done"
fi

echo "############ 2/3 libjpeg-turbo ############"
if fetch https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.4.tar.gz libjpeg-turbo.tar.gz libjpeg-turbo-src; then
  "$CMAKE_W" -S "$SRC_W/libjpeg-turbo-src" -B "$BUILD_W/libjpeg" "${common_args[@]}" \
      -DENABLE_SHARED=ON -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_SIMD=0 \
      2>&1 | tail -10
  "$CMAKE_W" --build "$BUILD_W/libjpeg" --target install -j 8 2>&1 | tail -8
  echo "libjpeg done"
fi

echo "############ 3/3 SDL2 ############"
if [ -f "$BASE/$ABI/lib/libSDL2.so" ]; then
  echo "--- SDL2 already installed"
else
"$CMAKE_W" -S C:/Users/admin/zuma-port/sdl2-src -B "$BUILD_W/sdl2" "${common_args[@]}" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    2>&1 | tail -12
"$CMAKE_W" --build "$BUILD_W/sdl2" --target install -j 8 2>&1 | tail -10
echo "SDL2 done"
fi

echo "############ installed tree ############"
find "$BASE/$ABI" -name "*.so" -o -name "*.a" -o -name "*Config.cmake" | head -20
echo "DEPS DONE"
