import Ossia 1.0 as Ossia

// grandMA2 — professional lighting console telnet interface.
// The grandMA2 telnet session accepts command-line syntax as well as LUA
// commands for hardware simulation. Widely used in concert, theater, and
// architectural lighting.
//
// Protocol: Telnet on TCP port 30000, CRLF-delimited.
// Login required: username + password (default: "administrator" / "admin").
//
// Note: grandMA3 does NOT use telnet — it uses OSC over UDP instead.
// This example is for grandMA2 (and grandMA2 onPC) only.
//
// Set /connect to the console's IP to connect.
Ossia.Mapper
{
  property var gma: null
  property bool ready: false
  property bool loggedIn: false

  function connectConsole(host) {
    if(gma) {
      gma.close();
      gma = null;
    }
    ready = false;
    loggedIn = false;
    Device.write("/status", "connecting");

    gma = Protocols.outboundTCP({
      Transport: { Host: host, Port: 30000 },
      Framing: { type: "line", delimiter: "\r\n" },
      onOpen: function(sock) {
        console.log("grandMA2: connected to", host);
        ready = true;
        Device.write("/status", "connected");
      },
      onMessage: function(msg) {
        handleResponse(msg.toString());
      },
      onClose: function() {
        console.log("grandMA2: disconnected");
        Device.write("/status", "disconnected");
        ready = false;
        loggedIn = false;
      },
      onError: function() {
        Device.write("/status", "error");
        ready = false;
      }
    });
  }

  function handleResponse(line) {
    // Strip ANSI color/escape codes the console may send
    line = line.replace(/\x1b\[[0-9;]*m/g, "");

    console.log("gMA <<", line);

    // Login prompt
    if(line.indexOf("Please login") >= 0 || line.indexOf("Login") >= 0 && !loggedIn) {
      var user = Device.read("/login_user") || "administrator";
      var pass = Device.read("/login_password") || "admin";
      send("Login " + user + " " + pass);
      return;
    }

    // Successful login
    if(line.indexOf("Logged in") >= 0 && line.indexOf("guest") < 0) {
      loggedIn = true;
      Device.write("/status", "ready");
      console.log("grandMA2: logged in successfully");
      return;
    }

    // Login failure
    if(line.indexOf("no login") >= 0 || line.indexOf("Login failed") >= 0) {
      Device.write("/status", "login_failed");
      console.log("grandMA2: login failed");
      return;
    }

    Device.write("/last_response", line);
  }

  function send(cmd) {
    if(!gma || !ready) return;
    console.log("gMA >>", cmd);
    gma.write(cmd);
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectConsole(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "login_user", type: Ossia.Type.String, value: "administrator" },
      { name: "login_password", type: Ossia.Type.String, value: "admin" },
      { name: "last_response", type: Ossia.Type.String, value: "" },

      // --- Cue triggering ---
      // Fire the next cue in executor 1 (main playback)
      {
        name: "go",
        type: Ossia.Type.Int,
        write: function(v) { send("Go+ Executor 1"); }
      },
      // Go back to previous cue
      {
        name: "goback",
        type: Ossia.Type.Int,
        write: function(v) { send("Go- Executor 1"); }
      },
      // Jump to a specific cue number on executor 1
      {
        name: "goto_cue",
        type: Ossia.Type.Float,
        write: function(v) { send("Goto Cue " + v.value + " Executor 1"); }
      },
      // Fire on a specific executor (write executor number)
      {
        name: "go_executor",
        type: Ossia.Type.Int,
        write: function(v) { send("Go+ Executor " + v.value); }
      },

      // --- Fader levels ---
      // Set master fader of executor 1 (0–100)
      {
        name: "fader_executor_1",
        type: Ossia.Type.Float,
        value: 100.0,
        write: function(v) { send("Executor 1 At " + v.value); }
      },
      {
        name: "fader_executor_2",
        type: Ossia.Type.Float,
        value: 0.0,
        write: function(v) { send("Executor 2 At " + v.value); }
      },
      {
        name: "fader_executor_3",
        type: Ossia.Type.Float,
        value: 0.0,
        write: function(v) { send("Executor 3 At " + v.value); }
      },

      // --- Direct fixture / channel control ---
      // Write any console command, e.g. "Channel 1 Thru 10 At 80"
      {
        name: "channel_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      },

      // --- Blackout ---
      {
        name: "blackout_on",
        type: Ossia.Type.Int,
        write: function(v) { send("BlackOut On"); }
      },
      {
        name: "blackout_off",
        type: Ossia.Type.Int,
        write: function(v) { send("BlackOut Off"); }
      },

      // --- Macro execution ---
      // Trigger a stored macro by number
      {
        name: "run_macro",
        type: Ossia.Type.Int,
        write: function(v) { send("Go+ Macro " + v.value); }
      },

      // --- LUA hardware simulation (advanced) ---
      // Simulate a hardkey press by button ID
      // Common IDs: 10=Go-, 11=Go+, 71=Macro, 82=Channel
      {
        name: "lua_hardkey",
        type: Ossia.Type.Int,
        write: function(v) {
          send("LUA 'gma.canbus.hardkey(" + v.value + ", true, false)'");
          send("LUA 'gma.canbus.hardkey(" + v.value + ", false, false)'");
        }
      },

      // --- Timecode ---
      {
        name: "timecode_go",
        type: Ossia.Type.Int,
        write: function(v) { send("Go+ Timecode " + v.value); }
      },
      {
        name: "timecode_stop",
        type: Ossia.Type.Int,
        write: function(v) { send("Off Timecode " + v.value); }
      },

      // --- Raw console command ---
      {
        name: "raw_command",
        type: Ossia.Type.String,
        write: function(v) { send(v.value); }
      }
    ];
  }
}
