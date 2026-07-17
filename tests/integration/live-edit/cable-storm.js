// Scenario 2 — cable-storm.
// Scene: src (solid color) and dst (image passthrough); dst outlet 0 ->
// Window:/. While playing, the cable src.out0 -> dst.in0 is created and
// removed every other tick — each toggle drives GfxContext::add_edge /
// remove_edge + recompute_graph while the render thread is live.
//
// KNOWN API GAP (documented in the cluster-J report):
// EditJsContext::remove() (EditContext.scenario.cpp:473) only handles
// Process::ProcessModel and scenario elements — a Process::Cable matches
// neither branch, so Score.remove(cable) is a silent no-op that submits
// no command. We still call it (regression probe: if it ever starts
// working, the following undo would then undo the WRONG command and this
// scenario would go blank — flagging the semantic change), then actually
// remove the cable by undoing the CreateCable command. createCable
// returning null submits no command either, so the undo is guarded.
//
// tick_final() leaves the cable CONNECTED so the final grab shows the
// solid color through the passthrough (non-blank).
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME    = "cable-storm";
var g_src   = null;
var g_dst   = null;
var g_cable = null;

function makeCable() {
    g_cable = Score.createCable(Score.outlet(g_src, 0), Score.inlet(g_dst, 0));
    llog(g_cable ? "cable created" : "TICK-ERROR createCable returned null");
}

function dropCable() {
    if(!g_cable) return;
    Score.remove(g_cable); // no-op today, see header comment
    g_cable = null;
    Score.undo();          // undoes CreateCable -> cable actually removed
    llog("cable removed (via undo)");
}

function step(n) {
    if(n % 2 === 0) { if(!g_cable) makeCable(); }
    else dropCable();
}

function tick_final() { if(!g_cable) makeCable(); }

var g_root = initBase();
g_src = addSolid(g_root);
g_dst = addPassthru(g_root);
if(g_dst) wireToWindow(g_dst);
markReady(NAME);
