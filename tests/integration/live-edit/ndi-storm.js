// Scenario 8 — ndi-storm.
// One score instance sends an NDI stream and receives its own stream back, and
// the received frames feed the window while it is driven through the same
// viewport storm as window-storm.
//
// Both NDI nodes are covered at once, and no external sender is needed: NDI
// discovery works over localhost, and the receiver connects by the full source
// name, which is "<MACHINE> (<path>)" -- NDI_MACHINE is injected by the sweep
// because the machine half is uppercased by the SDK and is not something the
// script can know.
//
// What this reaches that window-storm cannot: the NDI output node holds its own
// QRhi and its own readback path, so a viewport rebuild on the receiving side
// happens while a second, independent render target is still being presented
// into on the sending side. Stop/start tears down both at once.
//
// EXPECTED RED. No device pixels reach the window through this wiring: the grab
// is 76% black with the remaining 24% a uniform muddy purple, (159,90,127) at
// 1280x720 and (54,32,127) at 437x261. 24% is half the window's width by half
// its height, and the colour is deterministic per size.
//
// It is NOT an NDI defect, and the evidence for that is camera-storm: it renders
// the pixel-identical frame, same md5, from a completely different source. Two
// unrelated devices cannot coincide, so what is on screen is the passthrough
// ISF's disconnected-input fallback, and the shared cause is upstream of both
// devices -- most likely that setAddress() on an inlet does not establish a
// texture connection from a device the way a cable does.
//
// Ruled out along the way: score's NDI output node (an external sender built
// straight against the SDK gives the byte-identical picture), and NDI's own
// colour handling (a raw SDK-to-SDK round trip on this host returns
// (252,0,241), i.e. correct).
//
// The viewport storm itself PASSES underneath this: the chain survives resize,
// render-size, full screen and stop/start without crashing. The coverage oracle
// is what is red, and it must stay red until device pixels actually arrive --
// a non-blank check alone accepts the fallback, which is how this looked green
// at first.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME = "ndi-storm";
var UUID_NDI_IN  = "ae78b7c6-6400-483e-b45b-fd6ff87ec700";
var UUID_NDI_OUT = "07651c13-83de-48b8-a450-abe2891051e8";
var NDI_PATH = "score-ndi-storm";
var g_tmp = null;

function initNdi() {
    Score.createDevice("NdiOut", UUID_NDI_OUT, {
        "Path": NDI_PATH, "Width": 320, "Height": 240, "Rate": 30, "Format": "RGBA"
    });
    Score.createDevice("NdiIn", UUID_NDI_IN, {
        "Path": NDI_MACHINE + " (" + NDI_PATH + ")"
    });
}

function step(n) {
    llog(windowStormStep(n));
    // Churn a second consumer of the received stream while the viewport is
    // being rebuilt, so the receiver's texture is bound and unbound underneath
    // a live swap-chain recreation.
    if(n % 10 === 2) {
        g_tmp = addPassthru(Score.rootInterval());
        if(g_tmp) {
            Score.setAddress(Score.inlet(g_tmp, 0), "NdiIn:/");
            wireToWindow(g_tmp);
        }
        llog(g_tmp ? "second NDI consumer added" : "TICK-ERROR createProcess null");
    } else if(n % 10 === 7 && g_tmp) {
        Score.remove(g_tmp); g_tmp = null;
        llog("second NDI consumer removed");
    }
}

function tick_final() { windowStormRestore(); llog("tick_final: plain 640x480"); }

var g_root = initBase();
initNdi();

// Sender side: a solid colour into the NDI output device.
var g_send = addSolid(g_root);
if(g_send) Score.setAddress(Score.outlet(g_send, 0), "NdiOut:/");

// Receiver side: the NDI input through a passthrough into the window. A solid
// colour here would render whether or not a single NDI frame ever arrived.
var g_base = addPassthru(g_root);
if(g_base) {
    Score.setAddress(Score.inlet(g_base, 0), "NdiIn:/");
    wireToWindow(g_base);
}
markReady(NAME);
