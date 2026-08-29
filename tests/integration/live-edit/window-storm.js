// Scenario 6 — window-storm.
// Resize, render-size override and full-screen on/off driven through the
// Window device's own parameter tree WHILE the scene plays, with a graph
// mutation wedged in so the swap-chain rebuild and the render-list rebuild
// overlap.
//
// The tests in tests/gfx/Window*Torture.cpp drive ScreenNode directly. That
// leaves out everything between it and the document: GfxContext's clock and
// updateGraph(), the device's async run_async hop onto the Qt thread, and the
// execution engine still pushing frames at the node whose swap chain is being
// recreated underneath it. This is the only rig that covers that layer, and it
// is the layer the reported hide/show and resize crashes were never reproduced
// in.
//
// 12-step cycle (n % 12):
//   0  resize small          1  resize large
//   2  render-size override  3  clear render-size
//   4  full screen on        5  add a proc while full screen
//   6  full screen off       7  remove that proc
//   8  resize odd            9  move the window
//   10 render-size + full screen together
//   11 back to a plain window
// tick_final() restores a plain 640x480 window so the final grab is comparable.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME  = "window-storm";
var g_tmp = null;

function wsize(w, h)   { Device.write("Window:/size", [w, h]); }
function wrender(w, h) { Device.write("Window:/rendersize", [w, h]); }
function wfull(b)      { Device.write("Window:/fullscreen", b); }
function wmove(x, y)   { Device.write("Window:/position", [x, y]); }

function step(n) {
    switch(n % 12) {
        case 0:  wsize(320, 240);   llog("size 320x240");     break;
        case 1:  wsize(900, 700);   llog("size 900x700");     break;
        case 2:  wrender(160, 120); llog("rendersize 160x120"); break;
        case 3:  wrender(0, 0);     llog("rendersize cleared"); break;
        case 4:  wfull(true);       llog("fullscreen on");    break;
        case 5:
            g_tmp = addSolid(Score.rootInterval());
            if(g_tmp) wireToWindow(g_tmp);
            llog(g_tmp ? "proc added while full screen" : "TICK-ERROR createProcess null");
            break;
        case 6:  wfull(false);      llog("fullscreen off");   break;
        case 7:
            if(g_tmp) { Score.remove(g_tmp); g_tmp = null; llog("proc removed"); }
            break;
        // Odd extents: half-pixel viewports and mip rounding on the rebuild.
        case 8:  wsize(437, 261);   llog("size 437x261");     break;
        case 9:  wmove(80, 60);     llog("moved");            break;
        case 10: wrender(96, 96); wfull(true); llog("rendersize + fullscreen"); break;
        case 11: wfull(false); wrender(0, 0); llog("plain window"); break;
    }
}

function tick_final() {
    wfull(false);
    wrender(0, 0);
    wsize(640, 480);
    llog("tick_final: plain 640x480");
}

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
