// Shared prologue for the live-edit scenarios.
//
// Each scenario script pulls this in with:
//   eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));
// builds a small initial scene, then defines step(n). The scene plays via
// --autoplay while live-edit-sweep.sh injects `tick()` every ~500ms over
// OSC (/script on udp/6666). All /script evaluations run in the SAME
// persistent console QJSEngine as the initial --script
// (JS::ApplicationPlugin::m_consoleEngine), so `var` globals persist
// across sends — that is what makes stateful mutation sequences possible.
//
// NOTE: all top-level identifiers use `var` (not const/let) — QML's JS
// engine scopes const/let inside eval() so the outer script could not see
// them (same convention as tests-scene/common.js).

var TESTS_DIR   = "/home/jcelerier/Documents/ossia/score/packages/csf-examples/csf-testers";
var OUT_DIR     = "/tmp/live-edit";
var UUID_ISF    = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b"; // ISF filter process
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31"; // Window device
var SOLID       = TESTS_DIR + "/isf-solid-color.fs";
var PASSTHRU    = TESTS_DIR + "/isf-image-passthrough.fs";

var g_step = 0;

function llog(m) { console.log("[live-edit] " + m); }

// Window device + empty root interval (default Scenario removed).
function initBase() {
    Score.createDevice("Window", UUID_WINDOW, {});
    var s = Score.find("Scenario.1");
    if(s) Score.remove(s);
    return Score.rootInterval();
}

function addSolid(root)    { return Score.createProcess(root, UUID_ISF, SOLID); }
function addPassthru(root) { return Score.createProcess(root, UUID_ISF, PASSTHRU); }
function wireToWindow(p)   { Score.setAddress(Score.outlet(p, 0), "Window:/"); }

// Readiness marker: the sweep polls for this file before pumping ticks,
// so mutations only start once the scene is built (and play has begun).
function markReady(name) {
    Score.saveAs(OUT_DIR + "/" + name + "-init.score");
    llog(name + " scene ready");
}

// Injected by the sweep every ~500ms. Exceptions inside a mutation must
// land in the app log (a silent swallow would fake coverage), hence the
// wrapper. Scenarios define step(n).
function tick() {
    try { llog("tick " + g_step); step(g_step); }
    catch(e) { llog("TICK-ERROR step=" + g_step + ": " + e); }
    g_step++;
}

// Sent once after the tick storm, before the final grab: restore a state
// that is expected to render non-blank. Default: nothing to restore.
function tick_final() { }

// Sent by the sweep after the final grab, before /stop /exit.
function finalize(name) {
    try { Score.saveAs(OUT_DIR + "/" + name + "-final.score"); llog(name + " final saved"); }
    catch(e) { llog("FINAL-ERROR: " + e); }
}
