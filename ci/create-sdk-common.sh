#!/bin/bash -eux

mkdir -p "$INCLUDE/qt"

cp -rf "$OSSIA_SDK/qt6-static/include/." "$INCLUDE/qt/"

if [[ -d "$OSSIA_SDK/llvm-libs/include" ]]; then
  cp -rf "$OSSIA_SDK/llvm-libs/include/." "$INCLUDE/"
else
  cp -rf "$OSSIA_SDK/llvm/include/." "$INCLUDE/"
fi

cp -rf "$OSSIA_SDK/ffmpeg/include/." "$INCLUDE/" || true
cp -rf "$OSSIA_SDK/fftw/include/." "$INCLUDE/" || true
cp -rf "$OSSIA_SDK/portaudio/include/." "$INCLUDE/" || true

if [[ -d "$OSSIA_SDK/openssl/include" ]]; then
  cp -rf "$OSSIA_SDK/openssl/include/." "$INCLUDE/"
fi

cp -rf "$SCORE_DIR/3rdparty/avendish/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/csv2/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/DSPFilters/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xsimd/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xtensor/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xtl/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/eigen/Eigen" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/Gamma/Gamma" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/vcglib/vcg" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/vcglib/wrap" "$INCLUDE/"


BOOST=$(find "$SCORE_DIR/3rdparty/libossia/3rdparty/" -maxdepth 2 -name boost -type d | sort | tail -1)
if [[ -d "$BOOST" ]]; then
  cp -rf "$BOOST" "$INCLUDE/"
else
  BOOST=$(find /usr/local/Cellar/boost/ -mindepth 3 -maxdepth 3 -name boost -type d | sort | tail -1)
  if [[ -d "$BOOST" ]]; then
    cp -rf "$BOOST" "$INCLUDE/"
  fi
fi

# Ship compiler-rt's ORC runtime (liborc_rt*.a) into the JIT SDK so the JIT's
# ExecutorNativePlatform can load it at runtime: native TLS, real static-init /
# atexit and -- on COFF -- the __ImageBase platform symbol & exception registration.
# Args: <source clang-resource dir> <dest .../usr/lib dir>. The archive is copied
# preserving its clang/<ver>/lib/<triple>/ layout; locateOrcRuntime() finds it by
# a recursive liborc_rt*.a search under <dest>/clang. No-op where orc_rt is not
# built (e.g. Windows-arm64) -- the JIT then keeps its legacy non-platform path.
ship_orc_runtime() {
  local src_clang="$1" dst_lib="$2" found=0 f rel
  [[ -d "$src_clang" ]] || { echo "ship_orc_runtime: no $src_clang, skipping"; return 0; }
  while IFS= read -r f; do
    rel="${f#"$src_clang"/}"
    mkdir -p "$dst_lib/clang/$(dirname "$rel")"
    cp -f "$f" "$dst_lib/clang/$rel"
    echo "ship_orc_runtime: shipped clang/$rel"
    found=1
  done < <(find "$src_clang" -name 'liborc_rt*.a')
  [[ "$found" = 1 ]] || echo "ship_orc_runtime: no liborc_rt*.a under $src_clang (JIT native platform unavailable here)"
}