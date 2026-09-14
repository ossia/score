import Ossia 1.0 as Ossia

// Blackmagic HyperDeck — professional video recorder/player found in nearly
// every broadcast truck, live event, and media installation.
// Protocol: Blackmagic HyperDeck Ethernet Protocol over TCP port 9993.
// Messages are newline-delimited; multi-parameter commands use colon header
// followed by key: value lines, terminated by a blank line.
//
// The HyperDeck sends asynchronous notifications for transport and slot changes
// when subscribed via "notify". Responses are multi-line: a status line
// followed by key: value pairs, terminated by an empty line.
//
// Set /connect to the HyperDeck's IP address to connect.
Ossia.Mapper
{
  property var deck: null
  property bool ready: false
  property bool inBlock: false
  property string blockHeader: ""

  function connectDeck(host) {
    if(deck) {
      deck.close();
      deck = null;
    }
    ready = false;
    inBlock = false;
    Device.write("/status", "connecting");

    deck = Protocols.outboundTCP({
      Transport: { Host: host, Port: 9993 },
      Framing: { type: "line", delimiter: "\n" },
      onOpen: function(sock) {
        console.log("HyperDeck: TCP connected to", host);
        // HyperDeck sends a 500 greeting block on connect
      },
      onMessage: function(msg) {
        handleLine(msg.toString());
      },
      onClose: function() {
        console.log("HyperDeck: disconnected");
        Device.write("/status", "disconnected");
        ready = false;
      },
      onError: function() {
        Device.write("/status", "error");
        ready = false;
      }
    });
  }

  function handleLine(line) {
    // Strip trailing CR if present (HyperDeck sends \r\n, decoder strips \n)
    if(line.length > 0 && line[line.length - 1] === '\r')
      line = line.substring(0, line.length - 1);

    console.log("HD <<", JSON.stringify(line));

    // Empty line terminates a response/notification block
    if(line === "") {
      inBlock = false;
      blockHeader = "";
      return;
    }

    // Response header: "NNN description:"
    if(!inBlock && line.length >= 3 && !isNaN(parseInt(line.substring(0, 3)))) {
      var code = parseInt(line.substring(0, 3));
      blockHeader = line;
      inBlock = true;

      // 500 = connection info (greeting)
      if(code === 500) {
        Device.write("/status", "connected");
        return;
      }

      // 200 = ok (simple acknowledgement, no data follows)
      if(code === 200) {
        inBlock = false;
        return;
      }

      // 1xx = errors
      if(code >= 100 && code < 200) {
        console.log("HyperDeck error:", line);
        Device.write("/last_error", line);
        inBlock = false;
        return;
      }

      return;
    }

    // Key: value pair inside a block
    var colonIdx = line.indexOf(": ");
    if(colonIdx < 0) return;

    var key = line.substring(0, colonIdx);
    var val = line.substring(colonIdx + 2);

    // Greeting block (500) — extract model and protocol version
    if(blockHeader.indexOf("500") === 0) {
      if(key === "model") Device.write("/model", val);
      if(key === "protocol version") {
        Device.write("/protocol_version", val);
        // Greeting complete — subscribe to async notifications and query state
        ready = true;
        Device.write("/status", "ready");
        send("notify:\ntransport: true\nslot: true\nconfiguration: true");
        send("transport info");
        send("slot info:\nslot id: 1");
        send("device info");
      }
      return;
    }

    // Transport info (208 or 508 async notification)
    if(blockHeader.indexOf("208") >= 0 || blockHeader.indexOf("508") >= 0) {
      switch(key) {
        case "status":
          Device.write("/transport_state", val);
          break;
        case "speed":
          Device.write("/speed", parseInt(val) || 0);
          break;
        case "clip id":
          Device.write("/current_clip", parseInt(val) || 0);
          break;
        case "timecode":
          Device.write("/timecode", val);
          break;
        case "display timecode":
          Device.write("/display_timecode", val);
          break;
      }
      return;
    }

    // Slot info (202 or 502 async)
    if(blockHeader.indexOf("202") >= 0 || blockHeader.indexOf("502") >= 0) {
      switch(key) {
        case "status":
          Device.write("/slot_status", val);
          break;
        case "recording time":
          Device.write("/recording_time_available", val);
          break;
      }
      return;
    }

    // Device info (204)
    if(blockHeader.indexOf("204") >= 0) {
      if(key === "model") Device.write("/model", val);
      if(key === "unique id") Device.write("/unique_id", val);
      return;
    }
  }

  // send() appends \n so the framing encoder adds the blank-line terminator
  function send(cmd) {
    if(!deck || !ready) return;
    console.log("HD >>", cmd);
    deck.write(cmd + "\n");
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectDeck(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "last_error", type: Ossia.Type.String, value: "" },

      // --- Transport control ---
      {
        name: "play",
        type: Ossia.Type.Int,
        write: function(v) { send("play"); }
      },
      {
        name: "play_loop",
        type: Ossia.Type.Int,
        write: function(v) { send("play:\nloop: true"); }
      },
      {
        name: "play_single",
        type: Ossia.Type.Int,
        write: function(v) { send("play:\nsingle clip: true"); }
      },
      {
        name: "stop",
        type: Ossia.Type.Int,
        write: function(v) { send("stop"); }
      },
      {
        name: "record",
        type: Ossia.Type.Int,
        write: function(v) { send("record"); }
      },
      // Write a clip name to record with that name
      {
        name: "record_named",
        type: Ossia.Type.String,
        write: function(v) { send("record:\nname: " + v.value); }
      },

      // --- Navigation ---
      // Write a clip ID (1-based) to jump to it
      {
        name: "goto_clip",
        type: Ossia.Type.Int,
        write: function(v) { send("goto:\nclip id: " + v.value); }
      },
      // Write a timecode string "HH:MM:SS:FF"
      {
        name: "goto_timecode",
        type: Ossia.Type.String,
        write: function(v) { send("goto:\ntimecode: " + v.value); }
      },
      // Jog relative: write a timecode offset like "+00:00:01:00" or "-00:00:05:00"
      {
        name: "jog",
        type: Ossia.Type.String,
        write: function(v) { send("jog:\ntimecode: " + v.value); }
      },
      // Shuttle speed: 0=pause, 100=1x, 200=2x, -100=reverse
      {
        name: "shuttle_speed",
        type: Ossia.Type.Int,
        value: 0,
        write: function(v) { send("shuttle:\nspeed: " + v.value); }
      },

      // --- Configuration ---
      // Video input: "SDI", "HDMI", "component"
      {
        name: "video_input",
        type: Ossia.Type.String,
        write: function(v) { send("configuration:\nvideo input: " + v.value); }
      },
      // File format: "QuickTimeProRes", "QuickTimeDNxHD", etc.
      {
        name: "file_format",
        type: Ossia.Type.String,
        write: function(v) { send("configuration:\nfile format: " + v.value); }
      },

      // --- Read-only status (updated from transport notifications) ---
      { name: "transport_state", type: Ossia.Type.String, value: "" },
      { name: "speed", type: Ossia.Type.Int, value: 0 },
      { name: "current_clip", type: Ossia.Type.Int, value: 0 },
      { name: "timecode", type: Ossia.Type.String, value: "00:00:00:00" },
      { name: "display_timecode", type: Ossia.Type.String, value: "00:00:00:00" },
      { name: "slot_status", type: Ossia.Type.String, value: "" },
      { name: "recording_time_available", type: Ossia.Type.String, value: "" },
      { name: "model", type: Ossia.Type.String, value: "" },
      { name: "unique_id", type: Ossia.Type.String, value: "" },
      { name: "protocol_version", type: Ossia.Type.String, value: "" },

      // --- Manual ---
      {
        name: "refresh",
        type: Ossia.Type.Int,
        write: function(v) {
          send("transport info");
          send("slot info:\nslot id: 1");
        }
      },
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
