import Ossia 1.0 as Ossia

// Dataton WATCHOUT — spatial media / projection mapping server widely used
// for museum installations, immersive experiences, and architectural projection.
// Protocol: WATCHOUT Production TCP control, port 3040, CRLF-delimited text.
// Display cluster computers use port 3039 instead.
//
// WATCHOUT uses a simple command-response model. Commands are single lines;
// responses start with a status keyword: "Ready", "Busy", "Error", "Reply".
// Time values are in milliseconds. String parameters must be double-quoted.
//
// Set /connect to the WATCHOUT production computer IP.
Ossia.Mapper
{
  property var wo: null
  property bool ready: false

  function connectWatchout(host) {
    if(wo) {
      wo.close();
      wo = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    wo = Protocols.outboundTCP({
      Transport: { Host: host, Port: 3040 },
      Framing: { type: "line", delimiter: "\r\n" },
      onOpen: function(sock) {
        console.log("WATCHOUT: connected to", host);
        ready = true;
        Device.write("/status", "ready");
        send("ping");
        send("getStatus 1");
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
        console.log("WATCHOUT: disconnected");
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
    console.log("WO <<", line);

    if(line.indexOf("Ready") === 0) {
      Device.write("/wo_state", "ready");
      // Parse position if present: "Ready 12345 true" = time 12345ms, playing
      var parts = line.split(" ");
      if(parts.length >= 2)
        Device.write("/position_ms", parseInt(parts[1]) || 0);
      if(parts.length >= 3)
        Device.write("/playing", parts[2] === "true" ? 1 : 0);
    }
    else if(line.indexOf("Busy") === 0) {
      Device.write("/wo_state", "busy");
      var parts = line.split(" ");
      if(parts.length >= 2)
        Device.write("/position_ms", parseInt(parts[1]) || 0);
    }
    else if(line.indexOf("Error") === 0) {
      Device.write("/wo_state", "error");
      Device.write("/last_error", line);
      console.log("WATCHOUT error:", line);
    }
    else if(line.indexOf("Reply") === 0) {
      Device.write("/last_reply", line);
    }
    else if(line.indexOf("Status") === 0) {
      Device.write("/last_status_update", line);
    }

    Device.write("/last_response", line);
  }

  function send(cmd) {
    if(!wo || !ready) return;
    console.log("WO >>", cmd);
    wo.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectWatchout(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "last_error", type: Ossia.Type.String, value: "" },
      { name: "last_reply", type: Ossia.Type.String, value: "" },
      { name: "last_status_update", type: Ossia.Type.String, value: "" },

      // --- Timeline transport ---
      {
        name: "run",
        type: Ossia.Type.Int,
        write: function(v) { send("run"); }
      },
      // Run a specific auxiliary timeline
      {
        name: "run_timeline",
        type: Ossia.Type.String,
        write: function(v) { send('run "' + v.value + '"'); }
      },
      {
        name: "halt",
        type: Ossia.Type.Int,
        write: function(v) { send("halt"); }
      },
      // Halt a specific auxiliary timeline
      {
        name: "halt_timeline",
        type: Ossia.Type.String,
        write: function(v) { send('halt "' + v.value + '"'); }
      },
      // Kill an auxiliary timeline (stop + reset)
      {
        name: "kill_timeline",
        type: Ossia.Type.String,
        write: function(v) { send('kill "' + v.value + '"'); }
      },
      // Jump to time in milliseconds
      {
        name: "goto_time",
        type: Ossia.Type.Int,
        write: function(v) { send("gotoTime " + v.value); }
      },
      // Jump to a named control cue (does not start playback)
      {
        name: "goto_cue",
        type: Ossia.Type.String,
        write: function(v) { send('gotoControlCue "' + v.value + '" false'); }
      },
      // Jump to cue and run
      {
        name: "goto_cue_and_run",
        type: Ossia.Type.String,
        write: function(v) { send('gotoControlCue "' + v.value + '" true'); }
      },

      // --- Online / standby ---
      {
        name: "online",
        type: Ossia.Type.Int,
        write: function(v) { send("online " + (v.value ? "true" : "false")); }
      },
      // standBy with fade time in milliseconds
      {
        name: "standby",
        type: Ossia.Type.Int,
        write: function(v) { send("standBy " + (v.value ? "true" : "false") + " 1000"); }
      },
      // Configurable fade time for standBy
      {
        name: "standby_fade_ms",
        type: Ossia.Type.Int,
        value: 1000
      },

      // --- Show management ---
      // Load a show by path (production) or name (display cluster)
      {
        name: "load_show",
        type: Ossia.Type.String,
        write: function(v) { send('load "' + v.value + '"'); }
      },
      {
        name: "reset",
        type: Ossia.Type.Int,
        write: function(v) { send("reset"); }
      },

      // --- Dynamic inputs (WATCHOUT "setInput" feature) ---
      // Set a named input value. Write "inputName value" e.g. "myOpacity 0.5"
      // Supports relative values: "myParam +0.1" or "myParam -0.1"
      {
        name: "set_input",
        type: Ossia.Type.String,
        write: function(v) {
          var spaceIdx = v.value.indexOf(" ");
          if(spaceIdx > 0) {
            var name = v.value.substring(0, spaceIdx);
            var val = v.value.substring(spaceIdx + 1);
            send('setInput "' + name + '" ' + val);
          }
        }
      },
      // Convenience: set a named input to a float with optional fade
      {
        name: "input_name",
        type: Ossia.Type.String,
        value: "myInput"
      },
      {
        name: "input_value",
        type: Ossia.Type.Float,
        value: 0.0,
        write: function(v) {
          var name = Device.read("/input_name");
          send('setInput "' + name + '" ' + v.value);
        }
      },

      // --- Layer conditions (30-bit bitmask) ---
      {
        name: "layer_conditions",
        type: Ossia.Type.Int,
        write: function(v) { send("enableLayerCond " + v.value); }
      },

      // --- Status feedback ---
      { name: "wo_state", type: Ossia.Type.String, value: "" },
      { name: "position_ms", type: Ossia.Type.Int, value: 0 },
      { name: "playing", type: Ossia.Type.Int, value: 0 },

      // --- Utility ---
      {
        name: "ping",
        type: Ossia.Type.Int,
        write: function(v) { send("ping"); }
      },
      {
        name: "get_status",
        type: Ossia.Type.Int,
        write: function(v) { send("getStatus 1"); }
      },
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
