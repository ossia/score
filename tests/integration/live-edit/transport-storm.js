// Scenario 4 — transport-storm.
// 6 stop/play cycles with a graph mutation wedged between each transport
// flip: stop -> (add or remove an ISF proc wired to Window) -> play.
// Exercises execution-engine setup/teardown racing the gfx pipeline
// rebuild. 18 ticks = 6 full cycles; the cycle ends on play() so the
// final grab happens on a running transport. PASS = clean exit (render
// verdict is informational: the base proc should still show).
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME  = "transport-storm";
var g_tmp = null;

function step(n) {
    switch(n % 3) {
        case 0:
            Score.stop(); llog("stop");
            break;
        case 1:
            if(!g_tmp) {
                g_tmp = addSolid(Score.rootInterval());
                if(g_tmp) { wireToWindow(g_tmp); llog("created transient proc (stopped)"); }
            } else {
                Score.remove(g_tmp); g_tmp = null;
                llog("removed transient proc (stopped)");
            }
            break;
        case 2:
            Score.play(); llog("play");
            break;
    }
}

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
