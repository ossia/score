import Ossia 1.0 as Ossia

// POST JSON to an API endpoint.
// Demonstrates custom headers (Content-Type, Authorization) and request body.
Ossia.Mapper
{
  property string baseUrl: ""
  property string token: ""

  function createTree() {
    return [
      { name: "status_code", type: Ossia.Type.Int, value: 0 },
      { name: "response", type: Ossia.Type.String, value: "" },
      { name: "error", type: Ossia.Type.String, value: "" },
      {
        name: "base_url",
        type: Ossia.Type.String,
        value: "http://127.0.0.1:8080",
        write: function(v) { baseUrl = v.value; }
      },
      {
        name: "auth_token",
        type: Ossia.Type.String,
        value: "",
        write: function(v) { token = v.value; }
      },

      // POST arbitrary JSON: write '{"key":"value"}'
      {
        name: "post_json",
        type: Ossia.Type.String,
        write: function(v) {
          var headers = { "Content-Type": "application/json" };
          if(token) headers["Authorization"] = "Bearer " + token;

          Protocols.http({
            url: baseUrl + "/api/data",
            verb: "POST",
            headers: headers,
            body: v.value,
            onResponse: function(status, body) {
              console.log("POST ->", status, body);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              Device.write("/error", err);
            }
          });
        }
      },

      // PUT update: write '{"id":1,"name":"updated"}'
      {
        name: "put_json",
        type: Ossia.Type.String,
        write: function(v) {
          var headers = { "Content-Type": "application/json" };
          if(token) headers["Authorization"] = "Bearer " + token;

          Protocols.http({
            url: baseUrl + "/api/data",
            verb: "PUT",
            headers: headers,
            body: v.value,
            onResponse: function(status, body) {
              console.log("PUT ->", status, body);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              Device.write("/error", err);
            }
          });
        }
      },

      // DELETE resource by id: write "42"
      {
        name: "delete_resource",
        type: Ossia.Type.String,
        write: function(v) {
          var headers = {};
          if(token) headers["Authorization"] = "Bearer " + token;

          Protocols.http({
            url: baseUrl + "/api/data/" + v.value,
            verb: "DELETE",
            headers: headers,
            onResponse: function(status, body) {
              console.log("DELETE ->", status);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              Device.write("/error", err);
            }
          });
        }
      }
    ];
  }
}
