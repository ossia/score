// Scenario 11 — scene-storm.  (SPEC-SCENE-RENDER-TESTS.md P0-12, gap G5)
//
// REGISTRATION (for the orchestrator; this file registers nothing itself).
// tests/integration/live-edit-sweep.sh enumerates scenarios EXPLICITLY, not
// by glob — three lines must be edited there:
//   1. CFG   (the `declare -A CFG=(` block, currently lines 78-89): add
//          [scene-storm]="20 yes"
//      right after the line `[gfx-process-storm]="20 yes"` (line 88).
//      20 ticks = two full 10-step cycles; nticks MUST stay a multiple of 10
//      or the last cycle's teardown never runs and the magenta gate fails.
//   2. EXPECT (the `declare -A EXPECT=(` block, lines 96-109): add
//          [scene-storm]=magenta
//      before the closing `)` (line 109). The final tick leaves only the
//      full-screen isf-solid-color base, so the >=99%-magenta gate applies —
//      NOT min_nonblack_coverage, which is for device-fed scenarios.
//   3. ORDER (lines 110-111): append `scene-storm` to the list, e.g. after
//      `gfx-process-storm` on line 111.
//   No precondition() entry is needed: this scenario uses no camera and no
//   NDI, only the Window device.
//   Optional, per P0-12's "Drive" clause: extend the coverage() file list in
//   the sweep (GfxContext.cpp / Graph.cpp / RenderList.cpp) with the threedim
//   ScenePreprocessor node + renderer sources so the vs-baseline diff shows
//   the storm reached the scene-merge path.
//
// Every other live-edit scenario mutates the graph with plain Gfx processes
// (ISF/Images/Text/VSA/CSF — see gfx-process-storm.js). This one storms the
// THREEDIM scene family while the transport runs: Camera, Light, Asset
// Loader, Instancer and Scene Group are created, cabled into a
// Scene Preprocessor -> Render Pipeline chain wired to the window, mutated,
// undone, redone and removed underneath the running execution graph. That is
// gap G5: SceneGraphOps/SceneGraphFilterTest/MaterialOverrideTest are all CPU
// tick-discipline tests and none of them ever mutates the scene graph while
// the render thread is live.
//
// The Asset Loader deliberately points at a NONEXISTENT .glb (and is later
// retargeted at a nonexistent .fbx): per the P0-13 contract,
// AssetLoader::process() returns {} for a missing file and operator()() then
// publishes nothing — so the storm needs no asset download and additionally
// exercises that contract with the engine running. The scene still has
// content (Camera + Lights + Group), so the preprocessor's flatten/merge
// path runs regardless.
//
// 10-step cycle, strictly balanced: every create is matched by a remove or by
// an undo whose redo stack is then discarded, so each cycle ends back at the
// magenta baseline (base ISF solid -> Window) with an empty scene graph.
// Undo/redo handle discipline (see undo-redo-during-play.js): a JS handle
// dangles as soon as the object it points at is undone, so undo/redo is only
// ever applied to the throw-away macro C whose handle is nulled first; the
// macro A/B handles are never undone and stay valid until their explicit
// Score.remove() in steps 8-9.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME = "scene-storm";

// From src/plugins/score-plugin-threedim/Threedim/*.hpp (halp_meta uuid) and
// */Metadata.hpp (PROCESS_METADATA).
var UUID_CAMERA  = "4c91b5e2-8d76-4ab3-9f14-6e0d8b3a2c57";
var UUID_LIGHT   = "9f3c1a5e-4b7d-4e2a-8c5f-1d6e0b9a3c7f";
var UUID_ASSET   = "2f6a8c41-7d93-4e5b-b1c8-4e3f9a7d2c5b";
var UUID_INST    = "5e8a2c7f-9b4d-4e3a-a1c6-2d7f0b3e8c4a";
var UUID_GROUP   = "8a3b5e2d-7c4f-4b9e-9d1a-6f8e2c5d3a7b";
var UUID_PREPROC = "a8f2c6d0-1b4e-4c7a-9d3f-5e8b2c1a7f0d";
var UUID_RENDERP = "dbfc2101-40d7-4807-8804-571e88992e7e";

// Guaranteed-missing assets, with real extensions so the glTF and FBX parser
// dispatch paths are both selected before the load fails.
var MISSING_GLB = LIVE_EDIT_DIR + "/no-such-asset-p0-12.glb";
var MISSING_FBX = LIVE_EDIT_DIR + "/no-such-asset-p0-12.fbx";

var g_root  = null;
var g_base  = null;  // permanent ISF solid wired to the window (magenta)
// Macro A handles (producers) — valid for the whole cycle, removed in step 9.
var g_cam = null, g_light = null, g_asset = null, g_inst = null, g_group = null;
// Macro B handles (consumer chain) — valid for the whole cycle, removed in step 8.
var g_pre = null, g_rp = null;

function mk(uuid, data) {
    var p = Score.createProcess(g_root, uuid, data);
    if(!p) llog("TICK-ERROR createProcess null for " + uuid);
    return p;
}

// Cable src outlet 0 -> named inlet of dst. Every threedim producer here has
// exactly one outlet (its scene out), so index 0 is unambiguous; inlets are
// addressed by name because control inlets precede or interleave them.
function wire(src, dst, inName) {
    if(!src || !dst) return;
    var o = Score.outlet(src, 0);
    var i = Score.inlet(dst, inName);
    if(!o || !i) { llog("TICK-ERROR port lookup failed for inlet " + inName); return; }
    var c = Score.createCable(o, i);
    if(!c) llog("TICK-ERROR createCable null for inlet " + inName);
}

// A control inlet tweak that must exist: a silent miss would hollow out the
// storm, so a bad name is a TICK-ERROR (which fails the sweep verdict).
function poke(proc, inName, value) {
    if(!proc) return;
    var i = Score.inlet(proc, inName);
    if(!i) { llog("TICK-ERROR no inlet named " + inName); return; }
    Score.setValue(i, value);
}

function step(n) {
    switch(n % 10) {
        case 0:
            // Macro A: the producer set, as ONE undoable command.
            // camera -> group.0, light -> group.1, asset -> instancer -> group.2
            Score.startMacro();
            g_cam   = mk(UUID_CAMERA, "");
            g_light = mk(UUID_LIGHT, "");
            g_asset = mk(UUID_ASSET, MISSING_GLB); // P0-13: publishes nothing
            g_inst  = mk(UUID_INST, "");
            g_group = mk(UUID_GROUP, "");
            wire(g_cam,   g_group, "Scene 0");
            wire(g_light, g_group, "Scene 1");
            wire(g_asset, g_inst,  "Scene In");
            wire(g_inst,  g_group, "Scene 2");
            Score.endMacro();
            llog("producers created (camera/light/asset[missing glb]/instancer/group)");
            break;
        case 1:
            // Macro B: the consumer chain, wired into the live window.
            // group -> Scene Preprocessor -> Render Pipeline -> Window:/
            Score.startMacro();
            g_pre = mk(UUID_PREPROC, "");
            g_rp  = mk(UUID_RENDERP, ""); // default raw-raster program
            wire(g_group, g_pre, "Scene In");
            if(g_pre && g_rp) {
                var o = Score.outlet(g_pre, "Geometry Out");
                var i = Score.inlet(g_rp, "Geometry In");
                if(o && i) { if(!Score.createCable(o, i)) llog("TICK-ERROR createCable geometry"); }
                else llog("TICK-ERROR preproc/pipeline port lookup");
                var t = Score.outlet(g_rp, "Texture Out");
                if(t) Score.setAddress(t, "Window:/");
                else llog("TICK-ERROR no Texture Out outlet");
            }
            Score.endMacro();
            llog("preprocessor + render pipeline wired to window");
            break;
        case 2: {
            // Macro C: a throw-away second Light merged into the group —
            // the piece the undo/redo storm is allowed to chew on.
            Score.startMacro();
            var xl = mk(UUID_LIGHT, "");
            wire(xl, g_group, "Scene 3");
            Score.endMacro();
            xl = null; // dangles at the first undo; never kept
            llog("extra light + cable added (macro C)");
            break;
        }
        case 3:
            // Tear macro C out from under the running scene graph...
            Score.undo();
            llog("undo (extra light removed live)");
            break;
        case 4:
            // ...put a freshly deserialized copy back...
            Score.redo();
            llog("redo (extra light re-inserted live)");
            break;
        case 5:
            // ...and take it out again. The next new command discards the
            // redo stack, so the extra light is gone for good — no handle
            // needed, nothing left for teardown.
            Score.undo();
            llog("undo (extra light removed again; redo stack dies at step 6)");
            break;
        case 6:
            // Port-driven rebuilds on LIVE nodes: Camera::rebuild(),
            // Instancer::rebuild(), SceneGroup::rebuild() between two frames.
            poke(g_cam,   "FOV", 90);
            poke(g_inst,  "Count", 16);
            poke(g_group, "Name", "scene-storm-group");
            llog("live control pokes (camera fov / instancer count / group name)");
            break;
        case 7:
            // Retarget the loader at a missing FBX: the other parser dispatch
            // path of the P0-13 nothing-published contract, mid-playback.
            poke(g_asset, "Asset file", MISSING_FBX);
            llog("asset loader retargeted at missing fbx");
            break;
        case 8:
            // Teardown, consumers first: the window loses its scene texture
            // while the producers are still ticking.
            if(g_rp)  { Score.remove(g_rp);  g_rp = null; }
            if(g_pre) { Score.remove(g_pre); g_pre = null; }
            llog("render pipeline + preprocessor removed");
            break;
        case 9:
            // Producers out, downstream-first; attached cables go with them.
            if(g_group) { Score.remove(g_group); g_group = null; }
            if(g_inst)  { Score.remove(g_inst);  g_inst = null; }
            if(g_asset) { Score.remove(g_asset); g_asset = null; }
            if(g_light) { Score.remove(g_light); g_light = null; }
            if(g_cam)   { Score.remove(g_cam);   g_cam = null; }
            // The camera removal, reversed and reapplied while playing: undo
            // resurrects the Camera under the running graph, redo removes it
            // again. Balanced, so the cycle still ends at the bare baseline.
            Score.undo(); Score.redo();
            llog("producers removed (+ undo/redo of the camera removal)");
            break;
    }
}

// The cycle is self-cleaning when nticks is a multiple of 10; the guarded
// removes only matter if the sweep is run with a nonstandard NTICKS that
// strands a cycle mid-flight. Order: consumers, then producers.
function tick_final() {
    if(g_rp)    { Score.remove(g_rp);    g_rp = null; }
    if(g_pre)   { Score.remove(g_pre);   g_pre = null; }
    if(g_group) { Score.remove(g_group); g_group = null; }
    if(g_inst)  { Score.remove(g_inst);  g_inst = null; }
    if(g_asset) { Score.remove(g_asset); g_asset = null; }
    if(g_light) { Score.remove(g_light); g_light = null; }
    if(g_cam)   { Score.remove(g_cam);   g_cam = null; }
    Score.play();
    llog("tick_final: scene chain gone, base solid only");
}

g_root = initBase();
g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
