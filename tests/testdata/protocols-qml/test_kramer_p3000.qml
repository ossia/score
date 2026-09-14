import Ossia 1.0 as Ossia

// Kramer Protocol 3000 — AV matrix switchers, scalers, and signal processors.
// Kramer is one of the most common AV brands alongside Extron, found in
// theaters, conference rooms, museums, and broadcast facilities.
//
// Protocol 3000 uses "#" as command prefix, CRLF delimited, TCP port 5000.
// Commands: #CMD param1,param2,...\r\n
// Responses: ~NN@CMD param1,param2,...\r\n  (NN = machine number)
// Error: ~NN@CMD ERR description\r\n
//
// Routing: #ROUTE layer,input,output  (layer 0=video, 1=audio)
//
// Set /connect to the Kramer device IP to connect.
Ossia.Mapper
{
  property var kramer: null
  property bool ready: false

  function connectKramer(host) {
    if(kramer) {
      kramer.close();
      kramer = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    kramer = Protocols.outboundTCP({
      Transport: { Host: host, Port: 5000 },
      Framing: { type: "line", delimiter: "\r\n" },
      onOpen: function(sock) {
        console.log("Kramer: connected to", host);
        ready = true;
        Device.write("/status", "ready");
        // Query model and capabilities
        send("#MODEL?");
        send("#VERSION?");
        send("#INFO-IO?");
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
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
    console.log("Kramer <<", line);

    // Response format: ~NN@CMD param,... or ~NN@CMD ERR ...
    if(line.length < 4 || line[0] !== '~') {
      Device.write("/last_response", line);
      return;
    }

    // Find the '@' separator
    var atIdx = line.indexOf("@");
    if(atIdx < 0) return;

    var payload = line.substring(atIdx + 1);

    // Check for error
    if(payload.indexOf(" ERR") >= 0) {
      Device.write("/last_error", payload);
      console.log("Kramer error:", payload);
      return;
    }

    // Parse command and parameters
    var spaceIdx = payload.indexOf(" ");
    var cmd, params;
    if(spaceIdx > 0) {
      cmd = payload.substring(0, spaceIdx);
      params = payload.substring(spaceIdx + 1);
    } else {
      cmd = payload;
      params = "";
    }

    switch(cmd) {
      case "MODEL":
        Device.write("/model", params);
        break;
      case "VERSION":
        Device.write("/firmware_version", params);
        break;
      case "INFO-IO": {
        // "IN inputs,OUT outputs"
        var m = params.match(/IN (\d+),OUT (\d+)/);
        if(m) {
          Device.write("/num_inputs", parseInt(m[1]));
          Device.write("/num_outputs", parseInt(m[2]));
        }
        break;
      }
      case "ROUTE": {
        // Routing confirmation: "layer,input,output"
        Device.write("/last_route", params);
        break;
      }
      case "VMUTE": {
        // "output,state" — 0 = unmuted, 1 = muted
        var parts = params.split(",");
        if(parts.length >= 2)
          Device.write("/mute_output_" + parts[0], parseInt(parts[1]) || 0);
        break;
      }
      case "VOL": {
        // "output,level"
        var parts = params.split(",");
        if(parts.length >= 2)
          Device.write("/volume", parseInt(parts[1]) || 0);
        break;
      }
      default:
        Device.write("/last_response", payload);
    }
  }

  function send(cmd) {
    if(!kramer || !ready) return;
    console.log("Kramer >>", cmd);
    kramer.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectKramer(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "last_error", type: Ossia.Type.String, value: "" },
      { name: "model", type: Ossia.Type.String, value: "" },
      { name: "firmware_version", type: Ossia.Type.String, value: "" },
      { name: "num_inputs", type: Ossia.Type.Int, value: 0 },
      { name: "num_outputs", type: Ossia.Type.Int, value: 0 },

      // --- AV Routing ---
      // Route input to output (video+audio): write "input output"
      // Both video and audio layers are routed
      {
        name: "route",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2) {
            send("#ROUTE 0," + parts[0] + "," + parts[1]);
            send("#ROUTE 1," + parts[0] + "," + parts[1]);
          }
        }
      },
      // Route video only (layer 0): write "input output"
      {
        name: "route_video",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            send("#ROUTE 0," + parts[0] + "," + parts[1]);
        }
      },
      // Route audio only (layer 1): write "input output"
      {
        name: "route_audio",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            send("#ROUTE 1," + parts[0] + "," + parts[1]);
        }
      },
      { name: "last_route", type: Ossia.Type.String, value: "" },

      // --- Convenience: direct output assignment ---
      // Write input number to route video to output N
      {
        name: "output_1",
        type: Ossia.Type.Int,
        write: function(v) {
          send("#ROUTE 0," + v.value + ",1");
          send("#ROUTE 1," + v.value + ",1");
        }
      },
      {
        name: "output_2",
        type: Ossia.Type.Int,
        write: function(v) {
          send("#ROUTE 0," + v.value + ",2");
          send("#ROUTE 1," + v.value + ",2");
        }
      },
      {
        name: "output_3",
        type: Ossia.Type.Int,
        write: function(v) {
          send("#ROUTE 0," + v.value + ",3");
          send("#ROUTE 1," + v.value + ",3");
        }
      },
      {
        name: "output_4",
        type: Ossia.Type.Int,
        write: function(v) {
          send("#ROUTE 0," + v.value + ",4");
          send("#ROUTE 1," + v.value + ",4");
        }
      },

      // --- Video mute per output ---
      // Write 1 to mute, 0 to unmute
      {
        name: "mute_output_1",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("#VMUTE 1," + (v.value ? "1" : "0")); }
      },
      {
        name: "mute_output_2",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("#VMUTE 2," + (v.value ? "1" : "0")); }
      },

      // --- Audio volume (device-dependent range) ---
      {
        name: "volume",
        type: Ossia.Type.Int,
        value: 50,
        write: function(v) { send("#VOL 1," + v.value); }
      },
      // Audio mute: 1 = mute, 0 = unmute
      {
        name: "audio_mute",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("#MUTE 1," + (v.value ? "1" : "0")); }
      },

      // --- Front panel lock ---
      {
        name: "frontpanel_lock",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("#LOCK-FP " + (v.value ? "1" : "0")); }
      },

      // --- Presets ---
      {
        name: "recall_preset",
        type: Ossia.Type.Int,
        write: function(v) { send("#PRST-RCL " + v.value); }
      },
      {
        name: "save_preset",
        type: Ossia.Type.Int,
        write: function(v) { send("#PRST-STO " + v.value); }
      },

      // --- Query current state ---
      {
        name: "refresh",
        type: Ossia.Type.Int,
        write: function(v) {
          send("#MODEL?");
          send("#INFO-IO?");
          send("#ROUTE? 0,1");
          send("#ROUTE? 0,2");
          send("#ROUTE? 0,3");
          send("#ROUTE? 0,4");
        }
      },

      // --- Raw Protocol 3000 command ---
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
