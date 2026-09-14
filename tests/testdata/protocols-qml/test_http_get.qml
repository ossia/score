import Ossia 1.0 as Ossia

// Basic GET request using the new Protocols.http({...}) overload.
// Demonstrates status code handling and error callback.
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "status_code", type: Ossia.Type.Int, value: 0 },
      { name: "response", type: Ossia.Type.String, value: "" },
      { name: "error", type: Ossia.Type.String, value: "" },
      {
        name: "fetch",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: v.value,
            verb: "GET",
            onResponse: function(status, body) {
              console.log("HTTP", status, "body length:", body.length);
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) {
              console.log("HTTP error:", err);
              Device.write("/error", err);
            }
          });
        }
      }
    ];
  }
}
