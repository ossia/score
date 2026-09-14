import Ossia 1.0 as Ossia

// CasparCG AMCP — open-source video/graphics server widely used in broadcast,
// live events, and media art installations (https://casparcg.com).
// Protocol: AMCP over TCP port 5250, CRLF-delimited text commands.
//
// CasparCG addresses content by channel and layer: "1-1" = channel 1 layer 1.
// Layers composite on top of each other (1 = bottom, higher = on top).
//
// Set /connect to the CasparCG server IP to connect.
// Then use /play, /stop, /mixer_* nodes to drive video playback.
Ossia.Mapper
{
  property var casparcg: null
  property bool ready: false

  function connectServer(host) {
    if(casparcg) {
      casparcg.close();
      casparcg = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    casparcg = Protocols.outboundTCP({
      Transport: { Host: host, Port: 5250 },
      Framing: { type: "line", delimiter: "\r\n" },
      onOpen: function(sock) {
        console.log("CasparCG: connected to", host);
        ready = true;
        Device.write("/status", "ready");
        send("VERSION");
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
        console.log("CasparCG: disconnected");
        Device.write("/status", "disconnected");
        ready = false;
      },
      onError: function() {
        Device.write("/status", "error");
        ready = false;
      }
    });
  }

  function handleResponse(line) {
    console.log("AMCP <<", line);

    // Response codes:
    //   1xx = informational  2xx = success  4xx = client error  5xx = server error
    if(line.length >= 3) {
      var code = parseInt(line.substring(0, 3));
      if(code >= 200 && code < 300)
        Device.write("/last_response", line);
      else if(code >= 400)
        Device.write("/last_error", line);
      else
        Device.write("/last_response", line);
    }
  }

  function send(cmd) {
    if(!casparcg || !ready) return;
    console.log("AMCP >>", cmd);
    casparcg.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectServer(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "last_error", type: Ossia.Type.String, value: "" },

      // --- Playback (channel 1 layer 1) ---
      // Write a filename to play it, e.g. "AMB" or "video.mp4"
      {
        name: "play",
        type: Ossia.Type.String,
        write: function(v) { send('PLAY 1-1 "' + v.value + '"'); }
      },
      // Same but looped
      {
        name: "play_loop",
        type: Ossia.Type.String,
        write: function(v) { send('PLAY 1-1 "' + v.value + '" LOOP'); }
      },
      // Preload in background — auto-plays when current clip finishes
      {
        name: "loadbg",
        type: Ossia.Type.String,
        write: function(v) { send('LOADBG 1-1 "' + v.value + '" AUTO'); }
      },
      {
        name: "stop",
        type: Ossia.Type.Int,
        write: function(v) { send("STOP 1-1"); }
      },
      {
        name: "pause",
        type: Ossia.Type.Int,
        write: function(v) { send("PAUSE 1-1"); }
      },
      {
        name: "resume",
        type: Ossia.Type.Int,
        write: function(v) { send("RESUME 1-1"); }
      },
      {
        name: "clear",
        type: Ossia.Type.Int,
        write: function(v) { send("CLEAR 1"); }
      },

      // --- Second layer (channel 1 layer 2, composites on top) ---
      {
        name: "overlay_play",
        type: Ossia.Type.String,
        write: function(v) { send('PLAY 1-2 "' + v.value + '"'); }
      },
      {
        name: "overlay_stop",
        type: Ossia.Type.Int,
        write: function(v) { send("STOP 1-2"); }
      },

      // --- Mixer: opacity (0.0–1.0), with optional transition ---
      // Write a float to set opacity instantly
      {
        name: "mixer_opacity",
        type: Ossia.Type.Float,
        value: 1.0,
        write: function(v) { send("MIXER 1-1 OPACITY " + v.value); }
      },
      // Write a float to animate opacity over 25 frames with easing
      {
        name: "mixer_opacity_fade",
        type: Ossia.Type.Float,
        value: 1.0,
        write: function(v) { send("MIXER 1-1 OPACITY " + v.value + " 25 easeinsine"); }
      },
      // Volume (0.0–1.0)
      {
        name: "mixer_volume",
        type: Ossia.Type.Float,
        value: 1.0,
        write: function(v) { send("MIXER 1-1 VOLUME " + v.value); }
      },
      // Brightness (0.0–1.0)
      {
        name: "mixer_brightness",
        type: Ossia.Type.Float,
        value: 1.0,
        write: function(v) { send("MIXER 1-1 BRIGHTNESS " + v.value); }
      },
      // Saturation (0.0–1.0)
      {
        name: "mixer_saturation",
        type: Ossia.Type.Float,
        value: 1.0,
        write: function(v) { send("MIXER 1-1 SATURATION " + v.value); }
      },
      // Fill: position and scale — "x y width height [frames] [easing]"
      // Origin is top-left, full screen = "0 0 1 1"
      {
        name: "mixer_fill",
        type: Ossia.Type.String,
        write: function(v) { send("MIXER 1-1 FILL " + v.value); }
      },

      // --- Raw AMCP command ---
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
