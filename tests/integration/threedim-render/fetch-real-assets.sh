#!/usr/bin/env bash
# Out-of-repo corpus of real-world, licence-clean 3D samples for the threedim
# pipeline (the in-repo integration assets are all generated in-test; nothing
# large is ever committed — the corpus pattern of tests/corpus/fetch-fate-suite.sh).
#
#   tests/integration/threedim-render/fetch-real-assets.sh [dest]
#
# Every asset is pinned by URL + sha256 and carries its provenance + licence
# here, next to the fetch. Re-running only downloads what is missing or hash-mismatched.
set -euo pipefail
DEST="${1:-$HOME/ossia/threedim-assets}"
mkdir -p "$DEST"

fetch() { # url sha256 out
  local url="$1" sum="$2" out="$DEST/$3"
  if [ -f "$out" ] && echo "$sum  $out" | sha256sum -c --quiet - 2>/dev/null; then
    echo "ok       $3"; return
  fi
  echo "fetching $3"
  curl -fsSL "$url" -o "$out"
  echo "$sum  $out" | sha256sum -c --quiet - || { echo "SHA MISMATCH: $3"; rm -f "$out"; exit 1; }
}

# Khronos glTF-Sample-Assets "Box" — the canonical minimal glTF-Binary mesh.
# Provenance: https://github.com/KhronosGroup/glTF-Sample-Assets (Box)
# Licence: CC0 1.0 (per the model's README metadata).
fetch "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/Box/glTF-Binary/Box.glb" \
      "ed52f7192b8311d700ac0ce80644e3852cd01537e4d62241b9acba023da3d54e" \
      "Box.glb"

# Khronos glTF-Sample-Assets "BoxVertexColors" — per-vertex colour attributes.
# Provenance: https://github.com/KhronosGroup/glTF-Sample-Assets (BoxVertexColors)
# Licence: CC0 1.0 (per the model's README metadata).
fetch "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/BoxVertexColors/glTF-Binary/BoxVertexColors.glb" \
      "9c48227f33b0ba2fbcf23b98ebf60d1c8ae0c6e6c5281e0aa3cc58affee10382" \
      "BoxVertexColors.glb"

# Utah teapot, triangulated OBJ.
# Provenance: https://github.com/alecjacobson/common-3d-test-models
# Licence: repository publishes these classic scan/test models as
# public-domain/CC0-equivalent research assets; the teapot original is
# public domain (Martin Newell, 1975).
fetch "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/teapot.obj" \
      "1b5396fedd74b577e32cef41146582c2f2e1a050d5b4915193c0ac1ad4187ed4" \
      "teapot.obj"

echo "corpus at $DEST"
