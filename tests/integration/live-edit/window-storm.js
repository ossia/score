// Scenario 6 — window-storm.
// Resize, render-size override, full screen and transport stop/start driven
// while the scene plays, with a graph mutation wedged in so the swap-chain
// rebuild and the render-list rebuild overlap.
//
// The tests in tests/gfx/Window*Torture.cpp drive ScreenNode directly. That
// leaves out everything between it and the document: GfxContext's clock and
// updateGraph(), the device's async run_async hop onto the Qt thread, and the
// execution engine still pushing frames at the node whose swap chain is being
// recreated underneath it. This is the only rig that covers that layer, and it
// is the layer the reported hide/show and resize crashes were never reproduced
// in.
//
// The source here is a plain ISF, so this is the control for camera-storm and
// ndi-storm: all three run the same windowStormStep() sequence, and a failure
// that appears in one of those but not here belongs to the device.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME  = "window-storm";
var g_tmp = null;

function step(n) {
    llog(windowStormStep(n));
    // Every other cycle, add and remove a process while the viewport is being
    // disrupted, so a render-list rebuild lands in the middle of a swap-chain
    // rebuild rather than after it.
    if(n % 10 === 4) {
        g_tmp = addSolid(Score.rootInterval());
        if(g_tmp) wireToWindow(g_tmp);
        llog(g_tmp ? "proc added while full screen" : "TICK-ERROR createProcess null");
    } else if(n % 10 === 9 && g_tmp) {
        Score.remove(g_tmp); g_tmp = null;
        llog("proc removed");
    }
}

function tick_final() { windowStormRestore(); llog("tick_final: plain 640x480"); }

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
