// Scenario 3 — undo-redo-during-play.
// Tick 0 creates (inside ONE startMacro/endMacro command) an ISF process
// wired to Window:/. Every following tick alternates Score.undo() /
// Score.redo() on that composite command while the transport runs: each
// undo tears the process + its window edge out of the executing render
// graph, each redo re-inserts a freshly deserialized copy. 6+ cycles.
//
// The JS handle from tick 0 dangles as soon as the first undo runs
// (undo deletes the C++ object; redo builds a NEW one) — so no handle is
// kept. Strict undo/redo alternation only ever toggles the top of the
// command stack, so the base scene underneath is never touched.
// The permanent base process keeps the final grab non-blank either way.
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME = "undo-redo-during-play";

function step(n) {
    if(n === 0) {
        Score.startMacro();
        var p = addSolid(Score.rootInterval());
        if(p) wireToWindow(p);
        Score.endMacro();
        llog(p ? "macro: proc+wire created" : "TICK-ERROR createProcess null");
    } else if(n % 2 === 1) {
        Score.undo(); llog("undo");
    } else {
        Score.redo(); llog("redo");
    }
}

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
