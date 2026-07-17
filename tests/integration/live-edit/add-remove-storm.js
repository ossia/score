// Scenario 1 — add-remove-storm.
// While the scene plays: create an ISF process wired straight to Window:/,
// then remove it on the next tick; 8 cycles (16 ticks). Exercises
// GfxContext add_edge/remove_edge + recompute_graph and
// Graph::recreateOutputRenderList on every cycle, mid-render.
// A permanent solid-color process keeps Window rendering, so the final
// grab must be non-blank.
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME  = "add-remove-storm";
var g_tmp = null;

function step(n) {
    var root = Score.rootInterval();
    if(n % 2 === 0) {
        g_tmp = addSolid(root);
        if(g_tmp) { wireToWindow(g_tmp); llog("created transient proc"); }
        else llog("TICK-ERROR createProcess returned null");
    } else if(g_tmp) {
        Score.remove(g_tmp);   // deletes the C++ object; drop the JS handle
        g_tmp = null;
        llog("removed transient proc");
    }
}

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
