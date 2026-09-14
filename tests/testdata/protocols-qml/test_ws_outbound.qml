import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var ws: Protocols.outboundWS({
    Transport: { Host: "127.0.0.1", Port: 8080 },
    onOpen: function(socket) {
      console.log("WebSocket connected");
      Device.write("/status", "connected");
      socket.write("hello from ws client");
    },
    onClose: function() {
      console.log("WebSocket disconnected");
      Device.write("/status", "disconnected");
    },
    onError: function() {
      console.log("WebSocket error");
      Device.write("/status", "error");
    },
    onTextMessage: function(msg) {
      console.log("WS text:", msg);
      Device.write("/last_text", msg);
    },
    onBinaryMessage: function(msg) {
      console.log("WS binary:", msg);
      Device.write("/last_binary", msg.toString());
    }
  })

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: "idle"
      },
      {
        name: "last_text",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "last_binary",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "send_text",
        type: Ossia.Type.String,
        write: function(v) {
          ws.write(v.value);
        }
      },
      {
        name: "send_binary",
        type: Ossia.Type.String,
        write: function(v) {
          ws.writeBinary(v.value);
        }
      }
    ];
  }
}
