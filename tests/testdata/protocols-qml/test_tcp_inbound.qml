import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var clients: []

  property var tcpServer: Protocols.inboundTCP({
    Transport: { Bind: "0.0.0.0", Port: 5000 },
    onOpen: function() {
      console.log("TCP server listening on port 5000");
      Device.write("/status", "listening");
    },
    onConnection: function(socket) {
      clients.push(socket);
      console.log("TCP client connected, total:", clients.length);
      Device.write("/client_count", clients.length);

      socket.onClose = function() {
        var idx = clients.indexOf(socket);
        if(idx > -1)
          clients.splice(idx, 1);
        console.log("TCP client disconnected, remaining:", clients.length);
        Device.write("/client_count", clients.length);
      };

      socket.receive(function(bytes) {
        console.log("TCP received:", bytes);
        Device.write("/last_message", bytes.toString());
      });
    },
    onClose: function() {
      console.log("TCP server closed");
      Device.write("/status", "closed");
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
        name: "client_count",
        type: Ossia.Type.Int,
        value: 0
      },
      {
        name: "last_message",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "broadcast",
        type: Ossia.Type.String,
        write: function(v) {
          for(var i = 0; i < clients.length; i++) {
            clients[i].write(v.value);
          }
        }
      }
    ];
  }
}
