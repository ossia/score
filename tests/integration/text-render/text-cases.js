// TEXT node render validation scene.
//
// Builds: Window device + one Gfx::Text process wired to Window:/ and
// defines setCase(name) mutators that text-render.sh triggers over OSC
// /script between grabs. All /script evaluations share the persistent
// console QJSEngine (JS::ApplicationPlugin::m_consoleEngine) so the `var`
// globals below persist across sends — same convention as
// live-edit/common.js and timeline-scenarios/scenario-ramp.js.
//
// Inlet map of Gfx::Text::Model (Gfx/Text/Process.cpp):
//   0 Text (LineEdit)     4 Position (XYSlider, domain [-5,5]^2)
//   1 Font (LineEdit)     5 Scale X (FloatSlider)
//   2 Point size (Float)  6 Scale Y (FloatSlider)
//   3 Opacity (Float)     7 Color (HSVSlider, rgba vec4)
//
// The FIRST grab ("default") happens before any setCase call: it validates
// that the DEFAULT controls ("Greetings from Oscar !", Monospace 28pt,
// white, position (0.5,0.5), scale 1) produce VISIBLE text — the
// off-screen-by-default regression check.
//
// `var` only — QML scopes const/let inside eval() (see live-edit/common.js).

var OUT_DIR     = "/tmp/text-render";
var UUID_TEXT   = "88bd9718-2a36-42ba-8eab-da5f84e3978e"; // Gfx::Text::Model
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31"; // Window device
var FLICKS_PER_MS = 705600;

function llog(m) { console.log("[text-render] " + m); }

Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1");
if (s) Score.remove(s);
var g_root = Score.rootInterval();
// Long enough that playback never ends during the sweep.
Score.setIntervalDuration(g_root, 600000 * FLICKS_PER_MS);

var g_text = Score.createProcess(g_root, UUID_TEXT, "");
if (!g_text) llog("SCENARIO-ERROR: createProcess(Text) returned null");
else Score.setAddress(Score.outlet(g_text, 0), "Window:/");

function inl(i) { return Score.inlet(g_text, i); }

// Reference settings shared by most cases; each case starts from this and
// overrides one dimension, so every case is self-contained (order-safe).
function applyBase() {
  Score.setValue(inl(0), "OSSIA score text");
  Score.setValue(inl(1), "DejaVu Sans Mono");
  Score.setValue(inl(2), 48.0);
  Score.setValue(inl(3), 1.0);
  Score.setValue(inl(4), [0.0, 0.0]);
  Score.setValue(inl(5), 1.0);
  Score.setValue(inl(6), 1.0);
  Score.setValue(inl(7), [1.0, 1.0, 1.0, 1.0]);
}

var CASES = {
  // "default" is NOT here: it is the untouched initial state.
  //
  // default-pos0 MUST run first (before applyBase touches the text): it sets
  // ONLY the position to (0,0), keeping the process-default string/font/size.
  // It isolates the known off-screen-default bug: the Position XYSlider is
  // built with the plain ControlInlet ctor (Gfx/Text/Process.cpp:50) so no
  // init value is pushed and the UBO default position {0.5,0.5}
  // (Gfx/Graph/TextNode.hpp:29) shifts the text ~180px above the screen top.
  "default-pos0": function() { Score.setValue(inl(4), [0.0, 0.0]); },
  "base":       function() { applyBase(); },
  "base-again": function() { applyBase(); }, // recovery + in-run determinism
  "size-small": function() { applyBase(); Score.setValue(inl(2), 24.0); },
  "size-large": function() { applyBase(); Score.setValue(inl(2), 96.0); },
  "font-sans":  function() { applyBase(); Score.setValue(inl(1), "Noto Sans"); },
  "color-red":  function() { applyBase(); Score.setValue(inl(7), [1.0, 0.0, 0.0, 1.0]); },
  "pos-left":   function() { applyBase(); Score.setValue(inl(4), [-0.5, 0.0]); },
  "pos-right":  function() { applyBase(); Score.setValue(inl(4), [0.5, 0.0]); },
  "pos-down":   function() { applyBase(); Score.setValue(inl(4), [0.0, -0.5]); },
  "scale-half": function() { applyBase(); Score.setValue(inl(5), 0.5); Score.setValue(inl(6), 0.5); },
  "unicode":    function() { applyBase(); Score.setValue(inl(0), "Héllö wörld ÀÉÎÕÜ çæœß"); },
  "cjk":        function() { applyBase(); Score.setValue(inl(1), "Noto Sans CJK JP");
                             Score.setValue(inl(0), "日本語のテキスト"); },
  // Codepoints no font provides (unassigned): must not crash; tofu or blank ok.
  "tofu":       function() { applyBase(); Score.setValue(inl(0), "\u0378\u0379\u0380\uFFFF"); },
  "empty":      function() { applyBase(); Score.setValue(inl(0), ""); },
  "longstr":    function() {
      applyBase();
      Score.setValue(inl(2), 28.0);
      var s = "";
      for (var i = 0; i < 40; i++)
        s += "The quick brown fox jumps over the lazy dog " + i + " — ";
      Score.setValue(inl(0), s); // ~2000 chars
  }
};

function setCase(name) {
  try {
    var f = CASES[name];
    if (!f) { llog("CASE-ERROR: unknown case " + name); return; }
    f();
    llog("case " + name + " applied");
  } catch (e) {
    llog("CASE-ERROR " + name + ": " + e);
  }
}

// Called by text-render.sh right before /exit: a just-saved (clean) document
// skips the "save changes?" QMessageBox that aborts under the offscreen QPA.
function finalizeRun() {
  try { Score.saveAs(OUT_DIR + "/text-final.score"); llog("final saved"); }
  catch (e) { llog("FINAL-ERROR: " + e); }
}

Score.saveAs(OUT_DIR + "/text-init.score"); // readiness marker
llog("ready");
