import Ossia 1.0 as Ossia

// Bearer token authentication flow: POST credentials to a login endpoint,
// receive a token, then use it for subsequent authenticated requests.
// Common pattern for REST APIs (Pharos, Hue Bridge v2, custom backends).
Ossia.Mapper
{
  property string baseUrl: ""
  property string authToken: ""
  property bool authenticated: false

  function login(host, user, pass) {
    baseUrl = "http://" + host;
    authToken = "";
    authenticated = false;
    Device.write("/status", "authenticating");

    Protocols.http({
      url: baseUrl + "/api/login",
      verb: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username: user, password: pass }),
      onResponse: function(status, body) {
        if(status === 200) {
          try {
            var resp = JSON.parse(body);
            if(resp.token) {
              authToken = resp.token;
              authenticated = true;
              Device.write("/status", "authenticated");
              Device.write("/token", authToken);
              console.log("Login successful");
              return;
            }
          } catch(e) {}
        }
        console.log("Login failed, HTTP", status);
        Device.write("/status", "login_failed (" + status + ")");
      },
      onError: function(err) {
        console.log("Login error:", err);
        Device.write("/status", "error");
      }
    });
  }

  function authGet(path, callback) {
    if(!authenticated) return;
    Protocols.http({
      url: baseUrl + path,
      verb: "GET",
      headers: { "Authorization": "Bearer " + authToken },
      onResponse: callback,
      onError: function(err) {
        console.log("Request error:", err);
        Device.write("/last_error", err);
      }
    });
  }

  function authPost(path, payload, callback) {
    if(!authenticated) return;
    Protocols.http({
      url: baseUrl + path,
      verb: "POST",
      headers: {
        "Authorization": "Bearer " + authToken,
        "Content-Type": "application/json"
      },
      body: JSON.stringify(payload),
      onResponse: callback,
      onError: function(err) {
        console.log("Request error:", err);
        Device.write("/last_error", err);
      }
    });
  }

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "disconnected" },
      { name: "token", type: Ossia.Type.String, value: "" },
      { name: "last_error", type: Ossia.Type.String, value: "" },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "last_status", type: Ossia.Type.Int, value: 0 },
      // Write "host user password" to log in
      {
        name: "login",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 3)
            login(parts[0], parts[1], parts[2]);
        }
      },
      // GET a resource path
      {
        name: "get",
        type: Ossia.Type.String,
        write: function(v) {
          authGet(v.value, function(status, body) {
            console.log("GET", v.value, "->", status);
            Device.write("/last_status", status);
            Device.write("/last_response", body);
          });
        }
      },
      // POST JSON to a path: write "path json" e.g. "/api/action {\"cmd\":\"go\"}"
      {
        name: "post",
        type: Ossia.Type.String,
        write: function(v) {
          var spaceIdx = v.value.indexOf(" ");
          if(spaceIdx > 0) {
            var path = v.value.substring(0, spaceIdx);
            var jsonStr = v.value.substring(spaceIdx + 1);
            try {
              var payload = JSON.parse(jsonStr);
              authPost(path, payload, function(status, body) {
                console.log("POST", path, "->", status);
                Device.write("/last_status", status);
                Device.write("/last_response", body);
              });
            } catch(e) {
              console.log("Invalid JSON:", jsonStr);
            }
          }
        }
      }
    ];
  }
}
