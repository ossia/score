// gfx-soak — resource/lifetime soak scenario.
//
// Baseline scene: Window device + ONE persistent solid-color ISF wired to
// Window (so a live render exists for the whole session and the final grab
// must be non-blank). Then soak-leak.sh pumps `cycle()` over OSC /script
// (persistent console QJSEngine — globals survive across sends, same
// convention as tests/integration/live-edit/): each cycle creates K
// passthrough-ISF processes, chains them with cables (inlet 0 = image input,
// proven by live-edit/cable-storm.js), wires the tail to the Window, then
// removes every process (cable teardown cascades from process removal).
// Each cycle therefore exercises: process+renderer creation, add_edge /
// remove_edge, recompute_graph, renderer destruction — the teardown-UAF /
// leak bug family.
//
// All top-level identifiers use `var` — QML's engine scopes const/let inside
// eval() so later /script sends could not see them.

var TESTS_DIR = "/home/jcelerier/Documents/ossia/score/packages/csf-examples/csf-testers";
var OUT_DIR   = "/tmp/gfx-soak";
var UUID_ISF    = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b"; // ISF filter process
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31"; // Window device
var SOLID     = TESTS_DIR + "/isf-solid-color.fs";
var PASSTHRU  = TESTS_DIR + "/isf-image-passthrough.fs";

var g_root   = null;
var g_cycles = 0;
var g_errors = 0;
var K = 4; // gfx processes churned per cycle

function llog(m) { console.log("[gfx-soak] " + m); }

function cycle() {
  try {
    var procs = [];
    for (var i = 0; i < K; i++) {
      var p = Score.createProcess(g_root, UUID_ISF, PASSTHRU);
      if (!p) throw "createProcess returned null (i=" + i + ")";
      procs.push(p);
    }
    for (var j = 1; j < K; j++) {
      if (!Score.createCable(Score.outlet(procs[j - 1], 0), Score.inlet(procs[j], 0)))
        throw "createCable returned null (j=" + j + ")";
    }
    Score.setAddress(Score.outlet(procs[K - 1], 0), "Window:/");
    for (var k = K - 1; k >= 0; k--) Score.remove(procs[k]);
    g_cycles++;
    llog("cycle " + g_cycles + " done");
  } catch (e) {
    g_errors++;
    llog("CYCLE-ERROR " + g_cycles + ": " + e);
  }
}

// Sent once after the storm: dump the final document (the runner asserts the
// gfx-process population returned to baseline by counting UUID_ISF entries).
function teardown() {
  try {
    Score.saveAs(OUT_DIR + "/final.score");
    llog("teardown cycles=" + g_cycles + " errors=" + g_errors);
  } catch (e) { llog("TEARDOWN-ERROR: " + e); }
}

// ---- build baseline ----
Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1");
if (s) Score.remove(s);
g_root = Score.rootInterval();
var g_solid = Score.createProcess(g_root, UUID_ISF, SOLID);
if (g_solid) Score.setAddress(Score.outlet(g_solid, 0), "Window:/");
else llog("INIT-ERROR: baseline createProcess returned null");
Score.saveAs(OUT_DIR + "/init.score"); // readiness marker polled by the runner
llog("ready");
