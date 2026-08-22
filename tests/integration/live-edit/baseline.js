// Coverage baseline: the same minimal ISF scene as every live-edit
// scenario, played for the same duration with ZERO mutations. Functions
// hit by the mutation scenarios but not by this run are the ones the
// live-edit machinery lights up.
eval(Score.readFile("/home/jcelerier/ossia/wt/score-tests/tests/integration/live-edit/common.js"));

var NAME = "baseline";

function step(n) { /* no mutations */ }

var g_root = initBase();
var g_base = addSolid(g_root);
if(g_base) wireToWindow(g_base);
markReady(NAME);
