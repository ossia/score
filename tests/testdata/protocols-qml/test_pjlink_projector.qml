import Ossia 1.0 as Ossia

// PJLink projector control — the standard protocol for professional projectors.
// Supported by Panasonic, Epson, NEC, Sony, Barco, Christie, BenQ, Optoma, etc.
// Spec: Class 1, TCP port 4352, CR-delimited text commands.
//
// Set /connect to the projector's IP address to connect, then use the tree
// nodes to control power, input, mute, and read back status.
//
// PJLink input codes (most common):
//   31 = HDMI 1    32 = HDMI 2
//   11 = RGB/VGA 1 12 = RGB/VGA 2
//   51 = Network   21 = Video/Composite
Ossia.Mapper
{
  property var projector: null
  property bool ready: false

  function connectProjector(host) {
    if(projector) {
      projector.close();
      projector = null;
    }
    ready = false;
    Device.write("/status", "connecting");

    projector = Protocols.outboundTCP({
      Transport: { Host: host, Port: 4352 },
      Framing: { type: "line", delimiter: "\r" },
      onOpen: function(sock) {
        console.log("PJLink: TCP connected to", host);
        // Projector will send its PJLINK handshake line
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
        console.log("PJLink: disconnected");
        Device.write("/status", "disconnected");
        ready = false;
      },
      onError: function() {
        console.log("PJLink: connection error");
        Device.write("/status", "error");
        ready = false;
      }
    });
  }

  function handleResponse(line) {
    console.log("PJLink <<", JSON.stringify(line));

    // --- Handshake ---
    // "PJLINK 0" = no authentication required
    // "PJLINK 1 xxxxxxxx" = authentication required (random seed)
    if(line.indexOf("PJLINK") === 0) {
      var parts = line.split(" ");
      if(parts[1] === "0") {
        console.log("PJLink: ready (no authentication)");
        ready = true;
        Device.write("/status", "ready");
        queryAll();
      } else if(parts[1] === "ERRA") {
        console.log("PJLink: authentication failed (wrong password)");
        Device.write("/status", "auth_failed");
      } else {
        console.log("PJLink: authentication required — set password in source");
        Device.write("/status", "auth_required");
      }
      return;
    }

    // --- Command responses: "%1XXXX=value" ---
    if(line.length < 8 || line[0] !== '%')
      return;

    var cmd = line.substring(2, 6);
    var val = line.substring(7);

    // Error responses common to all commands
    if(val === "ERR1") { console.log("PJLink: undefined command", cmd); return; }
    if(val === "ERR2") { console.log("PJLink: invalid parameter for", cmd); return; }
    if(val === "ERR3") { console.log("PJLink: unavailable right now", cmd); return; }
    if(val === "ERR4") { console.log("PJLink: projector failure on", cmd); return; }

    switch(cmd) {
      case "POWR": {
        var states = { "0": "standby", "1": "on", "2": "cooling", "3": "warming" };
        Device.write("/power_state", states[val] || val);
        break;
      }
      case "INPT":
        Device.write("/input_state", val);
        break;
      case "AVMT": {
        var mutes = { "10": "unmuted", "11": "video_mute",
                      "20": "unmuted", "21": "audio_mute",
                      "30": "unmuted", "31": "av_mute" };
        Device.write("/mute_state", mutes[val] || val);
        break;
      }
      case "LAMP": {
        // "12345 1" = 12345 cumulative hours, lamp currently on
        var parts = val.split(" ");
        Device.write("/lamp_hours", parseInt(parts[0]) || 0);
        break;
      }
      case "ERST": {
        // 6-digit error status: fan, lamp, temperature, cover, filter, other
        // 0=ok, 1=warning, 2=error
        Device.write("/error_status", val);
        if(val !== "000000")
          console.log("PJLink: error flags:", val);
        break;
      }
      case "NAME":
        Device.write("/projector_name", val);
        break;
      case "INF1":
        Device.write("/manufacturer", val);
        break;
      case "INF2":
        Device.write("/product", val);
        break;
    }
  }

  function send(cmd) {
    if(!projector || !ready) return;
    console.log("PJLink >>", cmd);
    projector.write(cmd);
  }

  function queryAll() {
    send("%1POWR ?");
    send("%1INPT ?");
    send("%1AVMT ?");
    send("%1LAMP ?");
    send("%1ERST ?");
    send("%1NAME ?");
    send("%1INF1 ?");
    send("%1INF2 ?");
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectProjector(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },

      // --- Power ---
      {
        name: "power",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("%1POWR " + (v.value ? "1" : "0")); }
      },
      { name: "power_state", type: Ossia.Type.String, value: "" },

      // --- Input selection ---
      // Write a PJLink input code: 31=HDMI1, 32=HDMI2, 11=VGA1, 51=Network
      {
        name: "input",
        type: Ossia.Type.Int,
        value: 31,
        write: function(v) { send("%1INPT " + v.value); }
      },
      { name: "input_state", type: Ossia.Type.String, value: "" },

      // --- A/V mute ---
      // 1 = mute video+audio, 0 = unmute
      {
        name: "mute",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("%1AVMT 3" + (v.value ? "1" : "0")); }
      },
      { name: "mute_state", type: Ossia.Type.String, value: "" },

      // --- Read-only status ---
      { name: "lamp_hours", type: Ossia.Type.Int, value: 0 },
      { name: "error_status", type: Ossia.Type.String, value: "" },
      { name: "projector_name", type: Ossia.Type.String, value: "" },
      { name: "manufacturer", type: Ossia.Type.String, value: "" },
      { name: "product", type: Ossia.Type.String, value: "" },

      // --- Manual query / raw command ---
      {
        name: "refresh",
        type: Ossia.Type.Int,
        write: function(v) { queryAll(); }
      },
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
