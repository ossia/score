import Ossia 1.0 as Ossia

// ETC Eos Family — professional lighting console (Eos, Ion, Element, Gio).
// Protocol: OSC over TCP on port 3032 (OSC 1.0, size-prefix framing).
// Alternative: port 3037 (OSC 1.1, SLIP framing) available in Eos v3.1.0+.
//
// Before connecting, enable OSC on the console:
//   Setup > System > Show Control > OSC > enable OSC RX and TX
//
// OSC command paths start with /eos/, feedback paths with /eos/out/.
// Full reference in the Eos Family Show Control User Guide.
//
// Set /connect to the console's IP address.
Ossia.Mapper
{
  property var eos: null
  property bool ready: false

  property var oscParser: Protocols.osc({
    onOsc: function(address, args) {
      handleOsc(address, args);
    }
  })

  function connectEos(host) {
    if(eos) {
      eos.close();
      eos = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    eos = Protocols.outboundTCP({
      Transport: { Host: host, Port: 3032 },
      Framing: { type: "size_prefix" },
      onOpen: function(sock) {
        console.log("Eos: connected to", host);
        ready = true;
        Device.write("/status", "ready");
      },
      onMessage: function(msg) {
        oscParser.processMessage(msg);
      },
      onClose: function() {
        console.log("Eos: disconnected");
        Device.write("/status", "disconnected");
        ready = false;
      },
      onError: function() {
        Device.write("/status", "error");
        ready = false;
      }
    });
  }

  function handleOsc(address, args) {
    console.log("Eos OSC <<", address, JSON.stringify(args));

    if(address.indexOf("/eos/out/active/cue/") === 0) {
      var parts = address.split("/");
      // /eos/out/active/cue/{list}/{cue}/{label}
      if(parts.length >= 7) {
        Device.write("/active_cue_list", parts[5]);
        Device.write("/active_cue", parts[6]);
      }
      return;
    }
    if(address.indexOf("/eos/out/pending/cue/") === 0) {
      var parts = address.split("/");
      if(parts.length >= 7)
        Device.write("/pending_cue", parts[6]);
      return;
    }
    if(address === "/eos/out/show/name") {
      if(args.length > 0)
        Device.write("/show_name", args[0].toString());
      return;
    }
  }

  function sendOsc(address, args) {
    if(!eos || !ready) return;
    console.log("Eos OSC >>", address, JSON.stringify(args));
    eos.osc(address, args || []);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectEos(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },

      // --- Cue control ---
      {
        name: "go",
        type: Ossia.Type.Int,
        write: function(v) { sendOsc("/eos/key/go_0", []); }
      },
      {
        name: "stop_back",
        type: Ossia.Type.Int,
        write: function(v) { sendOsc("/eos/key/stop", []); }
      },
      // Fire a specific cue on cue list 1
      {
        name: "goto_cue",
        type: Ossia.Type.Float,
        write: function(v) { sendOsc("/eos/cue/1/" + v.value + "/fire", []); }
      },
      // Fire cue on any list: write "list cue" e.g. "2 5.5"
      {
        name: "goto_cue_list",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            sendOsc("/eos/cue/" + parts[0] + "/" + parts[1] + "/fire", []);
        }
      },

      // --- Channel intensity (0–100) ---
      {
        name: "channel",
        type: Ossia.Type.Int,
        value: 1
      },
      {
        name: "channel_level",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) {
          var ch = Device.read("/channel") || 1;
          sendOsc("/eos/chan/" + ch, [v.value]);
        }
      },

      // --- Submasters (0.0–1.0) ---
      {
        name: "sub_1",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) { sendOsc("/eos/sub/1", [v.value]); }
      },
      {
        name: "sub_2",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) { sendOsc("/eos/sub/2", [v.value]); }
      },

      // --- Presets ---
      {
        name: "recall_preset",
        type: Ossia.Type.Int,
        write: function(v) { sendOsc("/eos/preset/" + v.value + "/fire", []); }
      },

      // --- Macros ---
      {
        name: "fire_macro",
        type: Ossia.Type.Int,
        write: function(v) { sendOsc("/eos/macro/fire", [v.value]); }
      },

      // --- Blackout ---
      {
        name: "blackout",
        type: Ossia.Type.Int,
        write: function(v) { sendOsc("/eos/key/blackout", []); }
      },

      // --- Command line (text commands terminated with #) ---
      // e.g. "Chan 1 Thru 10 At 80" or "Go_to_Cue 5"
      {
        name: "command",
        type: Ossia.Type.String,
        write: function(v) { sendOsc("/eos/newcmd", [v.value + "#"]); }
      },

      // --- Key press (send any key name) ---
      // e.g. "go_0", "stop", "blackout", "blind", "live"
      {
        name: "key_press",
        type: Ossia.Type.String,
        write: function(v) { sendOsc("/eos/key/" + v.value, []); }
      },

      // --- Feedback ---
      { name: "show_name", type: Ossia.Type.String, value: "" },
      { name: "active_cue_list", type: Ossia.Type.String, value: "" },
      { name: "active_cue", type: Ossia.Type.String, value: "" },
      { name: "pending_cue", type: Ossia.Type.String, value: "" }
    ];
  }
}
