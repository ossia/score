// Headless playback scenes, played with --autoplay under --no-gui.
//
// MODE "end": an empty 3 s root interval. When playback reaches the end, the
// engine stops the document; headless it used to look up the GUI-only
// Actions::Stop and abort (BUG-LEDGER X8).
//
// MODE "resize": the root keeps the new-document durations until playback
// runs, then resizeRoot() (sent over OSC) makes it 60 s -- leaving the stored
// max at the new-document 15.75 s under the infinite flag. Execution used to
// receive that stale max and end playback there (BUG-LEDGER X7);
// checkPlaying() writes a marker if the root is still playing afterwards.
//
// headless-end.sh prepends OUT_DIR and MODE. `var` only (see live-edit/common.js).

var FLICKS_PER_MS = 705600;
var DURATION_MS = 3000;

var s = Score.find("Scenario.1");
if (s) Score.remove(s);
var g_root = Score.rootInterval();
if (MODE === "end") {
  Score.setIntervalDuration(g_root, DURATION_MS * FLICKS_PER_MS);
  Score.setIntervalMaxDuration(g_root, DURATION_MS * FLICKS_PER_MS);
}

function resizeRoot() { Score.setIntervalDuration(g_root, 60000 * FLICKS_PER_MS); }

// About 25 s into a 60 s root, the play percentage is ~0.4 while playing, and
// back to 0 once playback has stopped.
function checkPlaying() {
  if (g_root.durations.percentage > 0.3)
    Score.saveAs(OUT_DIR + "/still-playing.score");
}

function finalizeRun() { Score.saveAs(OUT_DIR + "/headless-end-final.score"); }

Score.saveAs(OUT_DIR + "/headless-end-init.score"); // readiness marker
