// Scenario 10 — gfx-process-storm.
//
// Every OTHER live-edit scenario mutates the graph with one process type (a
// plain ISF filter). This one storms the rest of the Gfx process family while
// the scene plays: Images, Text, VSA, CSF and the geometry filter get created,
// wired, undone, redone and removed underneath a running execution graph, and
// the ISF filter that feeds the window has its SHADER swapped mid-playback via
// a preset.
//
// Why the preset and not a command: EditJsContext exposes loadPreset(), which
// calls ProcessModel::loadPreset() directly. For Gfx::Filter that is
// setProgram() -> ISFHelpers::setupISFModelPorts(), i.e. the whole port surface
// of a *live, executing* node is destroyed and rebuilt between two frames —
// the same thing ChangeShader's redo() does, minus the undo record. The
// undoable half of that path is covered in-process by
// tests/gfx/GfxShaderCommands.cpp; what only this rig can reach is doing it
// while the execution engine is pushing frames at the node.
//
// The C++ process tests never start the transport. So this is the only place
// where the Gfx process models meet GfxContext::recompute_graph and the
// exec-engine's live-edit path.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME = "gfx-process-storm";

var UUID_IMAGES = "e96c5c0b-7e09-49fb-a851-ff6f4811bb00";
var UUID_TEXT   = "88bd9718-2a36-42ba-8eab-da5f84e3978e";
var UUID_VSA    = "ea13ed06-d21c-4c84-8d0f-83ce0027b81c";
var UUID_CSF    = "a5bbffe0-93d2-4e70-995c-cf46c2c43520";
var UUID_GEOM   = "27d3cc85-a4b0-4924-8fde-71c337b40f59";

var VSA_SHADER  = TESTS_DIR + "/vsa-triangle.vs";
var CSF_SHADER  = TESTS_DIR + "/csf-gradient-y.cs";

var g_root    = null;
var g_base    = null;   // the ISF filter wired to the window
var g_tmp     = null;   // the process created this cycle
var g_solidP  = null;   // preset of the solid ISF
var g_passP   = null;   // preset of the passthrough ISF

function makeAndWire(uuid, data) {
    var p = Score.createProcess(g_root, uuid, data);
    if(!p) { llog("TICK-ERROR createProcess null for " + uuid); return null; }
    // Texture-producing processes get routed to the window; the geometry
    // filter has no texture outlet, so wiring it is skipped on purpose.
    if(uuid !== UUID_GEOM) {
        try { wireToWindow(p); }
        catch(e) { llog("TICK-ERROR wire: " + e); }
    }
    return p;
}

function dropTmp() {
    if(g_tmp) { Score.remove(g_tmp); g_tmp = null; llog("removed"); }
}

// 10-step cycle. Each cycle: create one process family, undo/redo the creation
// while it plays, swap the base filter's shader, then remove it again.
function step(n) {
    switch(n % 10) {
        case 0:
            g_tmp = makeAndWire(UUID_IMAGES, "");
            llog("images added");
            break;
        case 1:
            // Undo the creation WHILE the graph is running, then put it back.
            Score.undo();
            llog("undo (process removed under the running graph)");
            break;
        case 2:
            Score.redo();
            llog("redo");
            break;
        case 3:
            dropTmp();
            g_tmp = makeAndWire(UUID_TEXT, "");
            llog("text added");
            break;
        case 4:
            // Live shader swap on the node the window is reading from.
            if(g_passP) { Score.loadPreset(g_base, g_passP); llog("base -> passthrough"); }
            break;
        case 5:
            dropTmp();
            g_tmp = makeAndWire(UUID_VSA, VSA_SHADER);
            llog("vsa added");
            break;
        case 6:
            if(g_solidP) { Score.loadPreset(g_base, g_solidP); llog("base -> solid"); }
            break;
        case 7:
            dropTmp();
            g_tmp = makeAndWire(UUID_CSF, CSF_SHADER);
            llog("csf added");
            break;
        case 8:
            dropTmp();
            g_tmp = makeAndWire(UUID_GEOM, "");
            llog("geometry filter added (no texture outlet, left unwired)");
            break;
        case 9:
            dropTmp();
            // One undo/redo pair with nothing pending, to make sure the empty
            // stack case is hit while playing too.
            Score.undo(); Score.redo();
            llog("undo/redo of the removal");
            break;
    }
}

// Leave the base filter on the solid shader with nothing else in the graph, so
// the final grab measures this chain and not whichever process the last tick
// happened to add.
function tick_final() {
    dropTmp();
    if(g_solidP) Score.loadPreset(g_base, g_solidP);
    Score.play();
    llog("tick_final: solid only");
}

g_root = initBase();
g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);

// Presets are captured from throw-away processes so the storm can swap the
// live one between two known shaders without touching the filesystem.
{
    var solid = addSolid(g_root);
    var pass = addPassthru(g_root);
    if(solid) { g_solidP = Score.savePreset(solid); Score.remove(solid); }
    if(pass) { g_passP = Score.savePreset(pass); Score.remove(pass); }
    llog("presets captured: solid=" + (g_solidP ? "yes" : "NO")
         + " passthrough=" + (g_passP ? "yes" : "NO"));
}

markReady(NAME);
