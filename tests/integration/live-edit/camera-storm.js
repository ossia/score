// Scenario 7 — camera-storm.
// A live camera capture feeding the window, driven through the same viewport
// storm as window-storm: resize, render-size override, full screen, and
// transport stop/start.
//
// A camera is not an ISF. It owns a capture thread, a decoder and a pool of
// GPU textures whose lifetime is not the render list's, and stop/start tears
// down the execution graph while that thread is still producing.
//
// Input "default"/device "default" makes CameraDevice call findBestCameraMode()
// and pick whatever this host actually has, rather than pinning a /dev/videoN
// that exists on one machine. The sweep refuses to run this scenario when no
// camera answers, so an absent camera is a skip with a reason and never a
// silent pass.
eval(Score.readFile(LIVE_EDIT_DIR + "/common.js"));

var NAME = "camera-storm";
var UUID_CAMERA = "d615690b-f2e2-447b-b70e-a800552db69c";
var g_tmp = null;

function initCamera() {
    Score.createDevice("Camera", UUID_CAMERA, {
        "Input": "default", "Device": "default",
        "Size": [0, 0], "FPS": 0,
        "Codec": 0, "PixelFormat": -1, "ColorRange": 0, "Custom": false
    });
}

function step(n) {
    llog(windowStormStep(n));
    // Add and remove a consumer of the camera texture mid-storm: the capture
    // keeps producing into a render list that is being rebuilt around it.
    if(n % 10 === 2) {
        g_tmp = addPassthru(Score.rootInterval());
        if(g_tmp) {
            Score.setAddress(Score.inlet(g_tmp, 0), "Camera:/");
            wireToWindow(g_tmp);
        }
        llog(g_tmp ? "second camera consumer added" : "TICK-ERROR createProcess null");
    } else if(n % 10 === 7 && g_tmp) {
        Score.remove(g_tmp); g_tmp = null;
        llog("second camera consumer removed");
    }
}

function tick_final() { windowStormRestore(); llog("tick_final: plain 640x480"); }

var g_root = initBase();
initCamera();
// Passthrough rather than solid colour: the assertion is that camera pixels
// reach the window, which a solid-colour shader would satisfy without them.
var g_base = addPassthru(g_root);
if(g_base) {
    Score.setAddress(Score.inlet(g_base, 0), "Camera:/");
    wireToWindow(g_base);
}
markReady(NAME);
