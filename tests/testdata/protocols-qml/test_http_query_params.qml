import Ossia 1.0 as Ossia

// HTTP requests with query parameters, custom Accept headers, and
// various HTTP methods. Tests that URL query strings pass through correctly.
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "status_code", type: Ossia.Type.Int, value: 0 },
      { name: "response", type: Ossia.Type.String, value: "" },

      // GET with query parameters in URL
      {
        name: "search",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: "http://127.0.0.1:8080/api/search?q=" + encodeURIComponent(v.value) + "&limit=10",
            verb: "GET",
            headers: { "Accept": "application/json" },
            onResponse: function(status, body) {
              console.log("Search ->", status);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              console.log("Search error:", err);
            }
          });
        }
      },

      // HEAD request — check resource existence without downloading body
      {
        name: "head_check",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: v.value,
            verb: "HEAD",
            onResponse: function(status, body) {
              console.log("HEAD ->", status, "(body should be empty:", body.length, ")");
              Device.write("/status_code", status);
            },
            onError: function(err) {
              console.log("HEAD error:", err);
            }
          });
        }
      },

      // PATCH — partial update
      {
        name: "patch",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: "http://127.0.0.1:8080/api/resource/1",
            verb: "PATCH",
            headers: { "Content-Type": "application/json" },
            body: v.value,
            onResponse: function(status, body) {
              console.log("PATCH ->", status, body);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              console.log("PATCH error:", err);
            }
          });
        }
      },

      // POST form-urlencoded data (e.g. for OAuth token endpoints)
      {
        name: "post_form",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: "http://127.0.0.1:8080/api/token",
            verb: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "grant_type=client_credentials&client_id=myapp&client_secret=" + v.value,
            onResponse: function(status, body) {
              console.log("Token ->", status, body);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              console.log("Token error:", err);
            }
          });
        }
      }
    ];
  }
}
