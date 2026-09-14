import Ossia 1.0 as Ossia

// Extron SIS (Simple Instruction Set) — the de facto standard protocol for
// AV matrix switchers, scalers, and distribution amplifiers found in virtually
// every permanent AV installation (museums, theaters, conference rooms).
// Protocol: TCP (port varies by model: 23 for CrossPoint/DXP, 32100 for MMX),
// CRLF-delimited.
//
// Extron SIS routing commands: {input}*{output}{suffix}
//   ! = video + audio    % = video only    $ = audio only
//   0*{output}! = disconnect output
//   {input}*0! = route input to all outputs
//
// Set /connect to the switcher's IP address to connect.
Ossia.Mapper
{
  property var sw: null
  property bool ready: false

  function connectSwitcher(host) {
    if(sw) {
      sw.close();
      sw = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    sw = Protocols.outboundTCP({
      Transport: { Host: host, Port: 23 },
      Framing: { type: "line", delimiter: "\r\n" },
      onOpen: function(sock) {
        console.log("Extron: connected to", host);
        // Extron sends a copyright banner on connect, then optionally
        // a "Password:" prompt if authentication is enabled.
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
        console.log("Extron: disconnected");
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
    console.log("Extron <<", JSON.stringify(line));

    // Password prompt
    if(line.indexOf("Password") >= 0) {
      Device.write("/status", "password_required");
      // If password is configured, send it:
      // sw.write("mypassword");
      return;
    }

    // Banner/copyright — first line(s) after connect
    if(line.indexOf("(c)") >= 0 || line.indexOf("Extron") >= 0) {
      Device.write("/device_info", line.trim());
      ready = true;
      Device.write("/status", "ready");
      return;
    }

    // Error codes: E01 through E28
    if(line.length >= 3 && line[0] === 'E' && !isNaN(parseInt(line.substring(1, 3)))) {
      var errors = {
        "E01": "invalid input number",
        "E10": "invalid command",
        "E12": "invalid output number",
        "E13": "invalid parameter",
        "E14": "not valid for this configuration",
        "E17": "invalid command for signal type",
        "E22": "busy",
        "E24": "privilege violation",
        "E25": "device not present",
        "E28": "file not found"
      };
      var code = line.substring(0, 3);
      console.log("Extron error:", code, errors[code] || "unknown");
      Device.write("/last_error", code + " " + (errors[code] || ""));
      return;
    }

    // Routing confirmations: "In1 All Out1" or "In1 Vid Out1" etc.
    if(line.indexOf("In") === 0 && line.indexOf("Out") > 0) {
      Device.write("/last_route", line.trim());
      return;
    }

    // Audio mute: "Amt1" (muted) or "Amt0" (unmuted)
    if(line.indexOf("Amt") === 0) {
      Device.write("/audio_muted", line[3] === '1' ? 1 : 0);
      return;
    }

    // Video mute: "Vmt1" (muted) or "Vmt0" (unmuted)
    if(line.indexOf("Vmt") === 0) {
      Device.write("/video_muted", line[3] === '1' ? 1 : 0);
      return;
    }

    // Frontpanel lock: "Exe1" (locked) or "Exe0" (unlocked)
    if(line.indexOf("Exe") === 0) {
      Device.write("/frontpanel_locked", line[3] === '1' ? 1 : 0);
      return;
    }

    Device.write("/last_response", line.trim());
  }

  function send(cmd) {
    if(!sw || !ready) return;
    console.log("Extron >>", cmd);
    sw.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectSwitcher(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "last_error", type: Ossia.Type.String, value: "" },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "device_info", type: Ossia.Type.String, value: "" },

      // --- Video + Audio routing ---
      // Route input N to output N: write "input output"
      {
        name: "route",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            send(parts[0] + "*" + parts[1] + "!");
        }
      },
      // Route video only: write "input output"
      {
        name: "route_video",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            send(parts[0] + "*" + parts[1] + "%");
        }
      },
      // Route audio only: write "input output"
      {
        name: "route_audio",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            send(parts[0] + "*" + parts[1] + "$");
        }
      },
      { name: "last_route", type: Ossia.Type.String, value: "" },

      // --- Convenience: direct output assignment ---
      // Write the input number to route to output 1 (video+audio)
      {
        name: "output_1",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + "*1!"); }
      },
      {
        name: "output_2",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + "*2!"); }
      },
      {
        name: "output_3",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + "*3!"); }
      },
      {
        name: "output_4",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + "*4!"); }
      },

      // --- Video mute per output: 1=mute, 0=unmute ---
      {
        name: "video_mute",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send(v.value + "*B"); }
      },
      { name: "video_muted", type: Ossia.Type.Int, value: 0 },
      // --- Audio mute: 1=mute, 0=unmute ---
      {
        name: "audio_mute",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send(v.value + "*Z"); }
      },
      { name: "audio_muted", type: Ossia.Type.Int, value: 0 },

      // --- Frontpanel lock ---
      {
        name: "frontpanel_lock",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send((v.value ? "1" : "0") + "X"); }
      },
      { name: "frontpanel_locked", type: Ossia.Type.Int, value: 0 },

      // --- Presets ---
      // Recall global preset by number
      {
        name: "recall_preset",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + "."); }
      },
      // Save current routing as preset
      {
        name: "save_preset",
        type: Ossia.Type.Int,
        write: function(v) { send(v.value + ","); }
      },

      // --- Raw SIS command ---
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
