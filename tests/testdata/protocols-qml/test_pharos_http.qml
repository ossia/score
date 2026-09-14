import Ossia 1.0 as Ossia

// Pharos Architectural Lighting Controller — permanent LED installations
// in museums, buildings, and public art.
// Protocol: HTTP REST API on port 80, JSON request/response, Bearer token auth.
// Requires Pharos Designer API v6+ (Designer 2.9+).
//
// Flow: authenticate to get a Bearer token, then POST JSON to control
// timelines, scenes, and group overrides.
//
// Set /connect to the controller's IP to authenticate and connect.
Ossia.Mapper
{
  property string pharosHost: ""
  property string authToken: ""
  property bool ready: false

  function connectPharos(host) {
    pharosHost = host;
    authToken = "";
    ready = false;
    Device.write("/status", "authenticating");

    var user = Device.read("/login_user") || "admin";
    var pass = Device.read("/login_password") || "";

    Protocols.http({
      url: "http://" + host + "/authenticate",
      verb: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "username=" + encodeURIComponent(user) + "&password=" + encodeURIComponent(pass),
      onResponse: function(status, body) {
        if(status === 200) {
          try {
            var resp = JSON.parse(body);
            if(resp.token) {
              authToken = resp.token;
              ready = true;
              Device.write("/status", "ready");
              console.log("Pharos: authenticated");
              return;
            }
          } catch(e) {}
        }
        console.log("Pharos: auth failed, status", status);
        Device.write("/status", "auth_failed");
      },
      onError: function(err) {
        console.log("Pharos: connection error:", err);
        Device.write("/status", "error");
      }
    });
  }

  function apiPost(endpoint, payload, callback) {
    if(!ready || !authToken) return;
    Protocols.http({
      url: "http://" + pharosHost + endpoint,
      verb: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": "Bearer " + authToken
      },
      body: JSON.stringify(payload),
      onResponse: function(status, body) {
        console.log("Pharos", endpoint, "->", status);
        // Check for refreshed token in response
        if(body) {
          try {
            var resp = JSON.parse(body);
            if(resp.token) authToken = resp.token;
          } catch(e) {}
        }
        if(callback) callback(status, body);
      },
      onError: function(err) {
        console.log("Pharos API error:", err);
      }
    });
  }

  function apiGet(endpoint, callback) {
    if(!ready || !authToken) return;
    Protocols.http({
      url: "http://" + pharosHost + endpoint,
      verb: "GET",
      headers: { "Authorization": "Bearer " + authToken },
      onResponse: function(status, body) {
        // Refresh token if provided
        if(body) {
          try {
            var resp = JSON.parse(body);
            if(resp.token) authToken = resp.token;
            if(callback) callback(status, resp);
          } catch(e) {
            if(callback) callback(status, body);
          }
        }
      },
      onError: function(err) {
        console.log("Pharos API error:", err);
      }
    });
  }

  function createTree() {
    return [
      // --- Connection ---
      {
        name: "connect",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { connectPharos(v.value); }
      },
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "login_user", type: Ossia.Type.String, value: "admin" },
      { name: "login_password", type: Ossia.Type.String, value: "" },

      // --- Timeline control ---
      {
        name: "start_timeline",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/timeline", { action: "start", num: v.value });
        }
      },
      {
        name: "release_timeline",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/timeline", { action: "release", num: v.value });
        }
      },
      {
        name: "toggle_timeline",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/timeline", { action: "toggle", num: v.value });
        }
      },
      {
        name: "pause_timeline",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/timeline", { action: "pause", num: v.value });
        }
      },
      {
        name: "resume_timeline",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/timeline", { action: "resume", num: v.value });
        }
      },
      // Set timeline rate: write "timeline rate" e.g. "1 0.5"
      {
        name: "set_timeline_rate",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 2)
            apiPost("/api/timeline", {
              action: "set_rate", num: parseInt(parts[0]), rate: parts[1]
            });
        }
      },

      // --- Scene control ---
      {
        name: "start_scene",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/scene", { action: "start", num: v.value });
        }
      },
      {
        name: "release_scene",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/scene", { action: "release", num: v.value });
        }
      },

      // --- Group master intensity (0.0–1.0) ---
      {
        name: "group_1_level",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) {
          apiPost("/api/group", {
            action: "master_intensity", num: 1, level: v.value
          });
        }
      },
      {
        name: "group_2_level",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) {
          apiPost("/api/group", {
            action: "master_intensity", num: 2, level: v.value
          });
        }
      },
      {
        name: "group_3_level",
        type: Ossia.Type.Float,
        value: 0,
        write: function(v) {
          apiPost("/api/group", {
            action: "master_intensity", num: 3, level: v.value
          });
        }
      },

      // --- Triggers ---
      {
        name: "fire_trigger",
        type: Ossia.Type.Int,
        write: function(v) {
          apiPost("/api/trigger", { action: "fire", num: v.value });
        }
      },

      // --- Query status ---
      {
        name: "refresh_timelines",
        type: Ossia.Type.Int,
        write: function(v) {
          apiGet("/api/timeline", function(status, resp) {
            if(resp && resp.timelines) {
              for(var i = 0; i < resp.timelines.length; i++) {
                var tl = resp.timelines[i];
                console.log("Timeline", tl.num, ":", tl.name, "-", tl.state);
              }
            }
          });
        }
      },
      {
        name: "refresh_groups",
        type: Ossia.Type.Int,
        write: function(v) {
          apiGet("/api/group", function(status, resp) {
            if(resp && resp.groups) {
              for(var i = 0; i < resp.groups.length; i++) {
                var g = resp.groups[i];
                console.log("Group", g.num, ":", g.name, "level:", g.level);
              }
            }
          });
        }
      }
    ];
  }
}
