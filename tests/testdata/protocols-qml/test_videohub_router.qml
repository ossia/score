import Ossia 1.0 as Ossia

// Blackmagic Videohub — video matrix router for multi-display installations.
// Protocol: Blackmagic Videohub Ethernet Protocol, TCP port 9990.
//
// The Videohub protocol uses block-based messages: a header line followed by
// key-value pairs, terminated by an empty line. On connect, the hub dumps
// its full state (labels, routing table, lock status).
//
// Routing is the core operation: "VIDEO OUTPUT ROUTING:\n0 2\n\n"
// means "route input 2 to output 0" (zero-indexed).
//
// Set /connect to the Videohub's IP address to connect.
// Then use /route to switch inputs: write "output input" (e.g. "0 3").
Ossia.Mapper
{
  property var hub: null
  property bool ready: false
  property string currentBlock: ""
  property int numInputs: 0
  property int numOutputs: 0

  function connectHub(host) {
    if(hub) {
      hub.close();
      hub = null;
    }
    ready = false;
    currentBlock = "";
    Device.write("/status", "connecting");

    hub = Protocols.outboundTCP({
      Transport: { Host: host, Port: 9990 },
      Framing: { type: "line", delimiter: "\n" },
      onOpen: function(sock) {
        console.log("Videohub: connected to", host);
        // Hub sends full state dump immediately
      },
      onMessage: function(msg) {
        handleLine(msg.toString());
      },
      onClose: function() {
        console.log("Videohub: disconnected");
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
    // Strip CR if present
    if(line.length > 0 && line[line.length - 1] === '\r')
      line = line.substring(0, line.length - 1);

    // Empty line terminates current block
    if(line === "") {
      currentBlock = "";
      return;
    }

    // Block headers end with ":"
    if(line.indexOf(":") === line.length - 1) {
      currentBlock = line.substring(0, line.length - 1).trim();
      console.log("Videohub block:", currentBlock);
      return;
    }

    // Key-value data inside blocks
    switch(currentBlock) {
      case "PROTOCOL PREAMBLE": {
        if(line.indexOf("Version:") === 0)
          Device.write("/protocol_version", line.substring(9).trim());
        break;
      }

      case "VIDEOHUB DEVICE": {
        var colonIdx = line.indexOf(": ");
        if(colonIdx < 0) break;
        var key = line.substring(0, colonIdx);
        var val = line.substring(colonIdx + 2);
        if(key === "Model name") Device.write("/model", val);
        if(key === "Video inputs") {
          numInputs = parseInt(val) || 0;
          Device.write("/num_inputs", numInputs);
        }
        if(key === "Video outputs") {
          numOutputs = parseInt(val) || 0;
          Device.write("/num_outputs", numOutputs);
          ready = true;
          Device.write("/status", "ready");
        }
        break;
      }

      case "INPUT LABELS": {
        // "0 Camera 1" — index followed by label
        var spaceIdx = line.indexOf(" ");
        if(spaceIdx > 0) {
          var idx = parseInt(line.substring(0, spaceIdx));
          var label = line.substring(spaceIdx + 1);
          console.log("  Input", idx, ":", label);
        }
        break;
      }

      case "OUTPUT LABELS": {
        var spaceIdx = line.indexOf(" ");
        if(spaceIdx > 0) {
          var idx = parseInt(line.substring(0, spaceIdx));
          var label = line.substring(spaceIdx + 1);
          console.log("  Output", idx, ":", label);
        }
        break;
      }

      case "VIDEO OUTPUT ROUTING": {
        // "0 2" = output 0 is receiving input 2
        var parts = line.split(" ");
        if(parts.length >= 2) {
          var output = parseInt(parts[0]);
          var input = parseInt(parts[1]);
          console.log("  Route: input", input, "-> output", output);
          Device.write("/routing_" + output, input);
        }
        break;
      }

      case "VIDEO OUTPUT LOCKS": {
        // "0 U" = output 0 unlocked, "0 O" = owned (locked by us)
        var parts = line.split(" ");
        if(parts.length >= 2) {
          var output = parseInt(parts[0]);
          var lock = parts[1];
          if(lock !== "U")
            console.log("  Output", output, "locked:", lock);
        }
        break;
      }
    }
  }

  function routeInput(output, input) {
    if(!hub || !ready) return;
    var cmd = "VIDEO OUTPUT ROUTING:\n" + output + " " + input + "\n";
    console.log("Videohub >>", JSON.stringify(cmd));
    hub.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectHub(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },

      // --- Routing ---
      // Write "output input" to route, e.g. "0 3" routes input 3 to output 0
      {
        name: "route",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2) routeInput(parseInt(parts[0]), parseInt(parts[1]));
        }
      },
      // Convenience: set the input for outputs 0–7 directly
      {
        name: "output_0_input",
        type: Ossia.Type.Int,
        write: function(v) { routeInput(0, v.value); }
      },
      {
        name: "output_1_input",
        type: Ossia.Type.Int,
        write: function(v) { routeInput(1, v.value); }
      },
      {
        name: "output_2_input",
        type: Ossia.Type.Int,
        write: function(v) { routeInput(2, v.value); }
      },
      {
        name: "output_3_input",
        type: Ossia.Type.Int,
        write: function(v) { routeInput(3, v.value); }
      },

      // --- Device info (populated on connect) ---
      { name: "model", type: Ossia.Type.String, value: "" },
      { name: "num_inputs", type: Ossia.Type.Int, value: 0 },
      { name: "num_outputs", type: Ossia.Type.Int, value: 0 },
      { name: "protocol_version", type: Ossia.Type.String, value: "" },

      // Routing state per output (updated from hub notifications)
      { name: "routing_0", type: Ossia.Type.Int, value: 0 },
      { name: "routing_1", type: Ossia.Type.Int, value: 0 },
      { name: "routing_2", type: Ossia.Type.Int, value: 0 },
      { name: "routing_3", type: Ossia.Type.Int, value: 0 },

      // --- Raw ---
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) {
          if(hub && ready) hub.write(v.value);
        }
      }
    ];
  }
}
