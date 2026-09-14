import Ossia 1.0 as Ossia

// Full loopback test: TCP server + client on the same mapper.
// The client connects to the server, sends a message, and the server echoes it back.
Ossia.Mapper
{
  property var clients: []

  // TCP server on port 5555
  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5555 },
    onOpen: function() {
      console.log("Loopback server ready");
      Device.write("/server_status", "listening");
    },
    onConnection: function(socket) {
      clients.push(socket);
      console.log("Server: client connected");

      socket.onClose = function() {
        var idx = clients.indexOf(socket);
        if(idx > -1) clients.splice(idx, 1);
      };

      // Echo received data back
      socket.receive(function(bytes) {
        console.log("Server received:", bytes);
        Device.write("/server_received", bytes.toString());
        socket.write("echo:" + bytes);
      });
    },
    onClose: function() {
      Device.write("/server_status", "closed");
    }
  })

  // TCP client connecting to the server above
  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5555 },
    onOpen: function(socket) {
      console.log("Client: connected to server");
      Device.write("/client_status", "connected");
      socket.write("ping");
    },
    onClose: function() {
      Device.write("/client_status", "disconnected");
    },
    onFail: function() {
      Device.write("/client_status", "failed");
    }
  })

  function createTree() {
    return [
      {
        name: "server_status",
        type: Ossia.Type.String,
        value: "idle"
      },
      {
        name: "client_status",
        type: Ossia.Type.String,
        value: "idle"
      },
      {
        name: "server_received",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "client_send",
        type: Ossia.Type.String,
        write: function(v) {
          client.write(v.value);
        }
      }
    ];
  }
}
