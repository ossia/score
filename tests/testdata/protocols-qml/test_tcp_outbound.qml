import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var tcpOut: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5000 },
    onOpen: function(socket) {
      console.log("TCP connected to server");
      socket.write("hello from tcp client");
      Device.write("/status", "connected");
    },
    onClose: function() {
      console.log("TCP disconnected");
      Device.write("/status", "disconnected");
    },
    onFail: function() {
      console.log("TCP connection failed");
      Device.write("/status", "failed");
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
        name: "send",
        type: Ossia.Type.String,
        write: function(v) {
          tcpOut.write(v.value);
        }
      }
    ];
  }
}
