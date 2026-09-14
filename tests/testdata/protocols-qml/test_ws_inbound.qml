import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var wsServer: Protocols.inboundWS({
    Transport: { Bind: "0.0.0.0", Port: 8080 },
    onOpen: function(socket) {
      console.log("WebSocket server listening on port 8080");
      Device.write("/status", "listening");
    },
    onClose: function() {
      console.log("WebSocket server closed");
      Device.write("/status", "closed");
    },
    onError: function() {
      console.log("WebSocket server error");
      Device.write("/status", "error");
    },
    onConnection: function(socket) {
      console.log("WebSocket client connected");
      Device.write("/status", "client_connected");
    }
  })

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: "idle"
      }
    ];
  }
}
