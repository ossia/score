// Timeline scenario 1 — automation ramp.
//
// Scene: Window device + ramp-level.fs ISF (uniform gray = `level` control)
// wired to Window. Root interval resized to exactly RAMP_MS, and an
// Automation on the `level` inlet ramping 0 -> 1 linearly across it.
// Therefore at timeline position T the rendered frame's pixel mean MUST be
// T / RAMP_MS (alpha excluded; compare.py-independent predicate).
//
// timeline-scenario.sh seeks with OSC /transport (absolute milliseconds,
// Engine/ApplicationPlugin.cpp:385), pauses, grabs, and asserts the mean at
// several positions — testing that seek + paused execution state + shader
// uniform propagation agree with the document's timeline.
//
// `var` only — QML scopes const/let inside eval() (see live-edit/common.js).

var HERE      = "/home/jcelerier/ossia/wt/score-tests/tests/integration/timeline-scenarios";
var OUT_DIR   = "/tmp/timeline-scenarios";
var UUID_ISF    = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b"; // ISF filter process
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31"; // Window device
var RAMP_MS   = 10000;
var FLICKS_PER_MS = 705600; // TimeVal impl units (double->TimeVal converter is raw flicks)

function llog(m) { console.log("[timeline] " + m); }

Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1");
if (s) Score.remove(s);
var g_root = Score.rootInterval();
Score.setIntervalDuration(g_root, RAMP_MS * FLICKS_PER_MS);

var g_proc = Score.createProcess(g_root, UUID_ISF, HERE + "/ramp-level.fs");
if (!g_proc) llog("SCENARIO-ERROR: createProcess returned null");
else {
  Score.setAddress(Score.outlet(g_proc, 0), "Window:/");

  // `level` is the shader's only INPUT -> inlet 0 (a Message control inlet).
  // automate() creates an Automation on it whose DEFAULT curve is a linear
  // 0->1 ramp across the interval — exactly the ramp this scenario needs, so
  // the expected frame mean at position T is T/duration. (Verified: grabs at
  // 2s/5s/8s track the ramp.) We keep the default rather than depending on the
  // automation's runtime name, which is not "Automation.1".
  Score.automate(g_root, Score.inlet(g_proc, 0));
  llog("automation wired (default 0->1 ramp)");
}

Score.saveAs(OUT_DIR + "/ramp-init.score"); // readiness marker
llog("ready");
