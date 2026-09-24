// Multi-outlet texture validation scene.
//
// Builds: Images (test photo) -> Image Processor (Depth Anything 3 metric) and a
// "Grid 2x2" ISF (loaded from Grid 2x2.scp) wired to Window:/ :
//
//   top left     : the source image          (Images outlet)
//   top right    : Image Processor "Image"   (outlet 0, nothing writes it here)
//   bottom left  : Image Processor "Mask"    (outlet 1, the sky mask, output 1)
//   bottom right : Image Processor "Depth"   (outlet 2, the metric depth, output 0)
//
// Two things are under test:
//  - score: every texture outlet of an avnd CPU node must draw its own texture.
//    It used to draw the first outlet's, so the three right/bottom tiles were
//    identical.
//  - Image Processor: the model output that Output Index does not select must
//    still reach its own outlet (depth -> Depth while the sky mask -> Mask).
//
// multi-outlet.sh prepends OUT_DIR, IMAGE, MODEL, GRID_PRESET and WIRING.
// `var` only -- QML scopes const/let inside eval() (see live-edit/common.js).

var UUID_IMAGES = "e96c5c0b-7e09-49fb-a851-ff6f4811bb00"; // Gfx::Images
var UUID_ISF    = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b"; // ISF filter process
var UUID_IMGPROC = "f4a5b6c7-d8e9-0123-4567-89abcdef0123"; // OnnxModels::ImageProcessor
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31"; // Window device
var FLICKS_PER_MS = 705600;

// Depth gain for the Grid. The metric depth is in meters (about 0.7 .. 6.5
// here), but it reaches the ISF input through an 8-bit render target that
// clamps it to [0, 1] first (BUG-LEDGER X10), so a gain < 1 would only
// compress what is left: keep the full [0, 1].
var DEPTH_GAIN = 1.0;

function llog(m) { console.log("[multi-outlet] " + m); }
function fail(m) { llog("SCENARIO-ERROR: " + m); }

Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1");
if (s) Score.remove(s);
var g_root = Score.rootInterval();
Score.setIntervalDuration(g_root, 600000 * FLICKS_PER_MS);
// resizeInterval leaves the root's max at the new-document default (15.75 s),
// and playback ends there even though the max is flagged infinite; under
// --no-gui that end then aborts (no Stop action to trigger). See BUG-LEDGER X7.
Score.setIntervalMaxDuration(g_root, 600000 * FLICKS_PER_MS);

// Source image.
var g_img = Score.createProcess(g_root, UUID_IMAGES, "");
if (!g_img) fail("createProcess(Images) returned null");
Score.setValue(Score.inlet(g_img, 5), [IMAGE]);
// Scale: Stretch (Original / Black bars / Fill / Stretch), so the processor's
// 512x512 input is the whole photo; analyze.py mirrors this framing.
Score.setValue(Score.inlet(g_img, 7), 3);

// Image Processor. Inlets: 0 In, 1 Aux, 2 Model, 3 Resolution, 4 Normalization,
// 5 Channel Order, 6 Resize, 7 Output Index, 8 Task, 9 Pixel Mapping, 10/11 Params.
// Outlets: 0 Image, 1 Mask, 2 Depth, 3 Data.
var g_ip = Score.createProcess(g_root, UUID_IMGPROC, "");
if (!g_ip) fail("createProcess(Image Processor) returned null");
Score.loadPreset(g_ip, JSON.stringify({
  "Key": {"Uuid": UUID_IMGPROC, "Effect": ""},
  "Name": "Depth Anything v3 metric large (sky mask)",
  "Preset": [[2, {"String": MODEL}], [3, {"Vec2f": [504.0, 280.0]}],
             [4, {"String": "ImageNet"}], [5, {"String": "RGB"}], [6, {"String": "Crop"}],
             [7, {"Int": 1}], [8, {"String": "Mask"}], [9, {"String": "DirectClamp"}],
             [10, {"Float": 0.0}], [11, {"Float": 0.0}]]
}));

// Grid, from the "Grid 2x2" preset. The ISF is created empty (default
// program) and the preset replaces its program: in "init" right away, in
// "live" once playback runs (wireOutlets) -- a preset loaded while playing
// used to leave the old shader running (BUG-LEDGER X9).
var g_grid = Score.createProcess(g_root, UUID_ISF, "");
if (!g_grid) fail("createProcess(ISF) returned null");
Score.setAddress(Score.outlet(g_grid, 0), "Window:/");
function loadGrid() {
  Score.loadPreset(g_grid, JSON.stringify(GRID_PRESET));
  Score.setValue(Score.inlet(g_grid, 8), DEPTH_GAIN); // gainBottomRight
}
if (WIRING === "init")
  loadGrid();

function cable(a, b, what) {
  if (!a || !b) { fail("missing port for " + what); return; }
  if (!Score.createCable(a, b)) fail("createCable failed: " + what);
}
cable(Score.outlet(g_img, 0), Score.inlet(g_ip, 0), "images -> ip.In");

// The processor -> grid cables. WIRING "init": created with the document, so
// the render passes come from the renderer's init. WIRING "live": created by
// multi-outlet.sh over OSC once playback runs, so each pass is added to the
// running graph (addOutputPass) -- the path where every outlet showed the first.
function wireOutlets() {
  if (WIRING === "live")
    loadGrid();
  cable(Score.outlet(g_img, 0), Score.inlet(g_grid, 0), "images -> grid.topLeft");
  cable(Score.outlet(g_ip, 0), Score.inlet(g_grid, 1), "ip.Image -> grid.topRight");
  cable(Score.outlet(g_ip, 1), Score.inlet(g_grid, 2), "ip.Mask -> grid.bottomLeft");
  cable(Score.outlet(g_ip, 2), Score.inlet(g_grid, 3), "ip.Depth -> grid.bottomRight");
  llog("outlets wired (" + WIRING + ")");
}
if (WIRING === "init")
  wireOutlets();

// Called by multi-outlet.sh right before /exit: a just-saved (clean) document
// skips the "save changes?" QMessageBox that aborts under the offscreen QPA.
function finalizeRun() {
  try { Score.saveAs(OUT_DIR + "/multi-outlet-final.score"); llog("final saved"); }
  catch (e) { llog("FINAL-ERROR: " + e); }
}

Score.saveAs(OUT_DIR + "/multi-outlet-init.score"); // readiness marker
llog("ready");
