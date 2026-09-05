// Shared prologue for the live-edit scenarios.
//
// Each scenario script pulls this in with:
//   eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));
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

// Derived from the staged script's own directory (LIVE_EDIT_DIR, injected by
// live-edit-sweep.sh) so the scenarios need nothing outside the repository.
var TESTS_DIR   = LIVE_EDIT_DIR + "/../../gfx/corpus";
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

// ---- the window's own parameter tree -------------------------------------
// Written while the scene plays. Each of these crosses run_async onto the Qt
// thread and lands in ScreenNode, so they exercise GfxContext's clock and
// updateGraph() between the window and the graph -- the layer the C++ rigs in
// tests/gfx/ cannot reach, because they drive ScreenNode directly.
function wsize(w, h)   { Device.write("Window:/size", [w, h]); }
function wrender(w, h) { Device.write("Window:/rendersize", [w, h]); }
function wfull(b)      { Device.write("Window:/fullscreen", b); }
function wmove(x, y)   { Device.write("Window:/position", [x, y]); }

// One step of the viewport storm: resize, render-size override, full screen and
// the transport stops and starts that tear the execution graph down and rebuild
// it underneath the window. 10-step cycle; returns a label for the log.
//
// Shared by every scenario that has something streaming into the window, so
// that the camera and NDI chains are driven through exactly the same
// disruptions as the plain-ISF one and a difference between them means the
// device, not the storm.
function windowStormStep(n) {
    switch(n % 10) {
        case 0: wsize(320, 240);   return "size 320x240";
        case 1: wsize(900, 700);   return "size 900x700";
        case 2: wrender(160, 120); return "rendersize 160x120";
        case 3: wrender(0, 0);     return "rendersize cleared";
        case 4: wfull(true);       return "fullscreen on";
        case 5: Score.stop();      return "stop";
        case 6: wfull(false);      return "fullscreen off";
        case 7: Score.play();      return "play";
        // Odd extents: half-pixel viewports and mip rounding on the rebuild.
        case 8: wsize(437, 261);   return "size 437x261";
        case 9: wmove(80, 60);     return "moved";
    }
    return "?";
}

// Leave a plain, visible, default-sized window and a running transport, so the
// final grab measures the chain rather than whatever the last step happened to
// set.
function windowStormRestore() {
    wfull(false);
    wrender(0, 0);
    wsize(640, 480);
    Score.play();
}

// Readiness marker: the sweep polls for this file before pumping ticks,
// so mutations only start once the scene is built (and play has begun).
function markReady(name) {
    Score.saveAs(OUT_DIR + "/" + name + "-init.score");
    llog(name + " scene ready");
}

// Injected by the sweep every ~500ms. Scenarios define step(n).
function tick() {
    try { llog("tick " + g_step); step(g_step); }
    catch(e) { llog("TICK-ERROR step=" + g_step + ": " + e); }
    g_step++;
}

// Sent once after the tick storm, before the final grab: restore a state
// that is expected to render non-blank. Default: nothing to restore.
// Deliberately NOT `function tick_final() {}`. Each scenario eval()s this file as
// its first statement and declares its own tick_final() later in the file; the
// scenario's declaration is hoisted before any statement runs, so a function
// declaration here would be created by the eval afterwards and OVERWRITE it. That
// made every scenario's "restore a known-rendering state" step a silent no-op --
// they rendered only because the last step(n) happened to leave a rendering state.
// Assigning to a var instead runs at eval time and loses to the hoisted
// declaration, which is what we want.
var tick_final = (typeof tick_final === 'function') ? tick_final : function() { };

// Sent by the sweep after the final grab, before /stop /exit.
function finalize(name) {
    try { Score.saveAs(OUT_DIR + "/" + name + "-final.score"); llog(name + " final saved"); }
    catch(e) { llog("FINAL-ERROR: " + e); }
}
