import Ossia 1.0 as Ossia

Ossia.Mapper
{
  function createTree() {
    return [
      {
        name: "response",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "fetch",
        type: Ossia.Type.String,
        write: function(v) {
          // v.value is the URL to fetch
          Protocols.http(v.value, function(response) {
            console.log("HTTP response length:", response.length);
            Device.write("/response", response);
          }, "GET");
        }
      },
      {
        name: "poll",
        type: Ossia.Type.String,
        interval: 10000,
        read: function() {
          Protocols.http(
            "http://127.0.0.1:8080/status",
            function(response) {
              console.log("Poll response:", response);
              Device.write("/response", response);
            },
            "GET"
          );
        }
      }
    ];
  }
}
