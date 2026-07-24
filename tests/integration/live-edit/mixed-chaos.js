// Scenario 5 — mixed-chaos.
// Deterministic 10-step cycle interleaving everything the other
// scenarios do, against a scene with THREE processes:
//   base (solid) -> Window     : permanent, guarantees a live render
//   src  (solid)               : cable source
//   dst  (passthrough) -> Window : cable sink
// Cycle (n % 10):
//   0 createCable src->dst          (add_edge mid-play)
//   1 stop                          (transport off while cable present)
//   2 play
//   3 macro: create proc + wire     (single composite command)
//   4 undo   (proc gone)
//   5 redo   (proc back)
//   6 undo   (proc gone)            <- stack top back below the proc cmd
//   7 undo   (cable gone)           (remove_edge via undo, mid-play)
//   8 pause
//   9 resume
// Handles are never reused across undo/redo (they dangle by design).
// Step 3 pushes a fresh command, truncating the redo tail left by 6/7 —
// stack stays consistent across cycles. tick_final() reconnects the
// cable so dst shows the solid color for the final grab.
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME    = "mixed-chaos";
var g_src   = null;
var g_dst   = null;
var g_cable = null;

function step(n) {
    switch(n % 10) {
        case 0:
            if(!g_cable) {
                g_cable = Score.createCable(Score.outlet(g_src, 0), Score.inlet(g_dst, 0));
                llog(g_cable ? "cable created" : "TICK-ERROR createCable null");
            }
            break;
        case 1: Score.stop();  llog("stop");  break;
        case 2: Score.play();  llog("play");  break;
        case 3: {
            Score.startMacro();
            var p = addSolid(Score.rootInterval());
            if(p) wireToWindow(p);
            Score.endMacro();
            llog("macro proc+wire");
            break;
        }
        case 4: Score.undo(); llog("undo proc");  break;
        case 5: Score.redo(); llog("redo proc");  break;
        case 6: Score.undo(); llog("undo proc");  break;
        case 7:
            if(g_cable) { g_cable = null; Score.undo(); llog("undo cable"); }
            break;
        case 8: Score.pause();  llog("pause");  break;
        case 9: Score.resume(); llog("resume"); break;
    }
}

function tick_final() {
    if(!g_cable) {
        g_cable = Score.createCable(Score.outlet(g_src, 0), Score.inlet(g_dst, 0));
        llog("tick_final: cable restored");
    }
}

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
g_src = addSolid(g_root);
g_dst = addPassthru(g_root);
if(g_dst) wireToWindow(g_dst);
markReady(NAME);
