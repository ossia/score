#!/bin/bash
set -euo pipefail

# WebAssembly (browser) packaging script for custom ossia score applications.
#
# Unlike the native platforms, this produces a *static web bundle*: a directory
# of files (plus a .zip) that can be served by any static file host. The score
# GUI is replaced by the custom QML UI (loaded through score's --ui flag), and
# the QML tree + score file are copied into the Emscripten in-memory filesystem
# at startup through Qt's official qtLoad "preload" mechanism.
#
# Serving note: cross-origin isolation (COOP/COEP headers) is required for the
# multi-threaded wasm build (SharedArrayBuffer). The bundled coi-serviceworker
# injects those headers automatically on hosts where you cannot set them (e.g.
# GitHub Pages); on your own server, prefer sending the headers directly.

PLATFORM="$1"
shift
QML_ITEMS=("$@")

if [[ "$PLATFORM" != "wasm" ]]; then
    echo "Error: Invalid wasm platform: $PLATFORM"
    exit 1
fi

echo "Creating WebAssembly web bundle..."

# Where the base wasm build (ossia-score.js/.wasm[/.data]) comes from.
DEPLOY_SRC="$SCORE_SOURCE_DIR/cmake/Deployment/WASM"

cd "$WORK_DIR"
BUILD_DIR="$WORK_DIR/wasm-build"
mkdir -p "$BUILD_DIR"

fetch_wasm_build() {
    if [[ -n "$LOCAL_INSTALLER" ]]; then
        echo "Using local wasm build: $LOCAL_INSTALLER"
        if [[ -d "$LOCAL_INSTALLER" ]]; then
            cp -r "$LOCAL_INSTALLER"/. "$BUILD_DIR/"
        elif [[ "$LOCAL_INSTALLER" =~ \.zip$ ]]; then
            unzip -o -q "$LOCAL_INSTALLER" -d "$BUILD_DIR"
        else
            echo "Error: --local-installer for wasm must be a directory or a .zip"
            exit 1
        fi
        return
    fi

    # Try an official wasm release asset (mirrors the other platforms).
    if [[ "$RELEASE_TAG" == "continuous" ]]; then
        WASM_ZIP="ossia.score-master-wasm.zip"
    else
        VERSION="${RELEASE_TAG#v}"
        WASM_ZIP="ossia.score-${VERSION}-wasm.zip"
    fi
    WASM_URL="https://github.com/ossia/score/releases/download/${RELEASE_TAG}/${WASM_ZIP}"

    echo "Downloading: $WASM_URL"
    if curl -L -f -o "$WORK_DIR/wasm-build.zip" "$WASM_URL"; then
        unzip -o -q "$WORK_DIR/wasm-build.zip" -d "$BUILD_DIR"
        return
    fi

    # Fall back to the continuous web build published to the score-web repo.
    echo "No wasm release asset; falling back to the ossia/score-web continuous build."
    if ! git clone --depth 1 "https://github.com/ossia/score-web.git" "$WORK_DIR/score-web"; then
        echo "Error: could not obtain a wasm build (no --local-installer, no release asset, no score-web)."
        exit 1
    fi
    cp -r "$WORK_DIR/score-web"/. "$BUILD_DIR/"
}

fetch_wasm_build

# Locate the core files the build must provide.
if [[ ! -f "$BUILD_DIR/ossia-score.js" || ! -f "$BUILD_DIR/ossia-score.wasm" ]]; then
    # Some sources nest the files; try to find them.
    found_js="$(find "$BUILD_DIR" -name ossia-score.js -type f | head -n1)"
    if [[ -n "$found_js" ]]; then
        BUILD_DIR="$(dirname "$found_js")"
    else
        echo "Error: ossia-score.js / ossia-score.wasm not found in the wasm build source."
        exit 1
    fi
fi

# Assemble the servable site.
SITE_DIR="$WORK_DIR/site"
rm -rf "$SITE_DIR"
mkdir -p "$SITE_DIR/app/qml"

echo "Copying wasm runtime..."
cp "$BUILD_DIR/ossia-score.js" "$SITE_DIR/"
cp "$BUILD_DIR/ossia-score.wasm" "$SITE_DIR/"
# Qt emits a .data file only when the build preloads plugins/resources.
[[ -f "$BUILD_DIR/ossia-score.data" ]] && cp "$BUILD_DIR/ossia-score.data" "$SITE_DIR/"

# Loader + COOP/COEP service worker: prefer the ones shipped next to the build,
# otherwise use the copies from the score source tree.
for f in qtloader.js coi-serviceworker.min.js; do
    if [[ -f "$BUILD_DIR/$f" ]]; then
        cp "$BUILD_DIR/$f" "$SITE_DIR/"
    elif [[ -f "$DEPLOY_SRC/$f" ]]; then
        cp "$DEPLOY_SRC/$f" "$SITE_DIR/"
    else
        echo "Error: could not find $f (needed for the wasm loader)."
        exit 1
    fi
done

# Copy the custom QML files/directories into site/app/qml (same rules as Linux).
echo "Adding custom QML files..."
for item in "${QML_ITEMS[@]}"; do
    if [[ -f "$item" ]]; then
        echo "  Copying file: $(basename "$item")"
        cp "$item" "$SITE_DIR/app/qml/"
    elif [[ -d "$item" ]]; then
        echo "  Copying directory contents: $(basename "$item")"
        cp -r "$item"/* "$SITE_DIR/app/qml/" 2>/dev/null || true
        cp -r "$item"/.[!.]* "$SITE_DIR/app/qml/" 2>/dev/null || true
    fi
done

# Copy the score file (and any sibling assets in its directory).
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" "$SITE_DIR/app/"
    SCORE_DIR="$(dirname "$SCORE_FILE")"
    cp -r "$SCORE_DIR"/* "$SITE_DIR/app/" 2>/dev/null || true
fi

# Compile a .qrc into resources.rcc if provided (loaded by score at startup).
if [[ -n "$APP_QRC" ]]; then
    RCC_BIN="${RCC:-rcc}"
    if command -v "$RCC_BIN" >/dev/null 2>&1; then
        echo "Compiling resources..."
        "$RCC_BIN" "$APP_QRC" -o "$SITE_DIR/resources.rcc"
    else
        echo "Warning: rcc not found in PATH; skipping --app-qrc resource compilation."
    fi
fi

# Build the preload manifest: every file under app/ (and resources.rcc) is
# fetched at startup and written to the Emscripten FS at the same absolute path.
echo "Generating preload manifest..."
PRELOAD_JSON="$SITE_DIR/preload.json"
{
    echo "["
    first=1
    # app/ tree -> /app/<relpath>
    while IFS= read -r -d '' f; do
        rel="${f#"$SITE_DIR"/}"                # e.g. app/qml/Main.qml
        dest="/$rel"                           # e.g. /app/qml/Main.qml
        [[ $first -eq 1 ]] && first=0 || echo ","
        printf '  { "source": "%s", "destination": "%s" }' "$rel" "$dest"
    done < <(find "$SITE_DIR/app" -type f -print0 | sort -z)
    # resources.rcc -> /resources.rcc
    if [[ -f "$SITE_DIR/resources.rcc" ]]; then
        [[ $first -eq 1 ]] && first=0 || echo ","
        printf '  { "source": "resources.rcc", "destination": "/resources.rcc" }'
    fi
    echo ""
    echo "]"
} > "$PRELOAD_JSON"

# Compose the argv passed to score's main().
# The UI flag itself is chosen at load time: ?debug in the URL opens the score
# editor window alongside the custom UI.
ARGS_JS="'/app/qml/${MAIN_QML}'"
if [[ -n "$SCORE_BASENAME" ]]; then
    ARGS_JS="${ARGS_JS}, '/app/${SCORE_BASENAME}'"
fi
if [[ -n "$AUTOPLAY" ]]; then
    ARGS_JS="${ARGS_JS}, '--autoplay'"
fi

# Generate the custom index.html (based on cmake/Deployment/WASM/index.html but
# passing arguments + preload so score loads the custom UI and score file).
echo "Generating index.html..."
cat > "$SITE_DIR/index.html" << HTML_EOF
<!doctype html>
<html lang="en-us">
  <head>
    <meta charset="utf-8">
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <meta name="viewport" content="width=device-width, height=device-height, user-scalable=0"/>
    <title>${APP_NAME}</title>
    <style>
      html, body { padding: 0; margin: 0; overflow: hidden; height: 100% }
      #screen { width: 100%; height: 100%; }
    </style>
  </head>
  <body onload="init()">
    <script src="coi-serviceworker.min.js"></script>
    <figure style="overflow:visible;" id="qtspinner">
      <center style="margin-top:1.5em; line-height:150%">
        <strong>${APP_NAME}</strong>
        <div id="qtstatus"></div>
        <noscript>JavaScript is disabled. Please enable JavaScript to use this application.</noscript>
      </center>
    </figure>
    <div id="screen"></div>

    <script type="text/javascript">
        async function init() {
          const spinner = document.querySelector('#qtspinner');
          const screen = document.querySelector('#screen');
          const status = document.querySelector('#qtstatus');

          const showUi = (ui) => {
            [spinner, screen].forEach(element => element.style.display = 'none');
            ui.style.display = 'block';
          };

          const debugParam = new URLSearchParams(window.location.search).get('debug');
          const uiFlag = (debugParam !== null && !['0', 'false', 'no', 'off'].includes(debugParam.toLowerCase()))
            ? '--ui-debug' : '--ui';

          try {
            showUi(spinner);
            status.innerHTML = 'Loading...';

            await qtLoad({
              arguments: [uiFlag, ${ARGS_JS}],
              qt: {
                preload: ['preload.json'],
                environment: { QML2_IMPORT_PATH: '/app/qml' },
                onLoaded: () => showUi(screen),
                onExit: exitData => {
                  status.innerHTML = 'Application exit';
                  status.innerHTML += exitData.code !== undefined ? \` with code \${exitData.code}\` : '';
                  status.innerHTML += exitData.text !== undefined ? \` (\${exitData.text})\` : '';
                  showUi(spinner);
                },
                entryFunction: window.score_entry,
                containerElements: [screen],
              }
            });
          } catch (e) {
            status.innerHTML = e.message;
            showUi(spinner);
            console.error(e);
          }
        }
    </script>
    <script src="ossia-score.js"></script>
    <script type="text/javascript" src="qtloader.js"></script>
  </body>
</html>
HTML_EOF

# Publish the site and a zip of it.
FINAL_DIR="$OUTPUT_DIR/${APP_NAME_SAFE}-wasm"
rm -rf "$FINAL_DIR"
mkdir -p "$FINAL_DIR"
cp -r "$SITE_DIR"/. "$FINAL_DIR/"

OUTPUT_ZIP="$OUTPUT_DIR/${APP_NAME_SAFE}-wasm.zip"
rm -f "$OUTPUT_ZIP"
( cd "$SITE_DIR" && zip -r -q "$OUTPUT_ZIP" . )

echo "✓ Created web bundle: $FINAL_DIR"
echo "✓ Created archive:    $OUTPUT_ZIP"
echo
echo "To run locally (cross-origin isolation is provided by coi-serviceworker):"
echo "    cd \"$FINAL_DIR\" && python3 -m http.server 8080"
echo "    # then open http://localhost:8080"
echo "✓ WebAssembly bundle created successfully"
