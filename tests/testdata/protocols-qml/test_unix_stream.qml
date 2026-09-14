import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var clients: []

  property var streamServer: Protocols.inboundUnixStream({
    Transport: { Path: "/tmp/ossia_test_stream" },
    onOpen: function(socket) {
      console.log("Unix stream server listening");
      Device.write("/server_status", "listening");
    },
    onConnection: function(socket) {
      clients.push(socket);
      console.log("Unix stream client connected, total:", clients.length);
      Device.write("/client_count", clients.length);
    },
    onClose: function() {
      console.log("Unix stream server closed");
      Device.write("/server_status", "closed");
    },
    onError: function() {
      console.log("Unix stream server error");
    }
  })

  property var streamClient: Protocols.outboundUnixStream({
    Transport: { Path: "/tmp/ossia_test_stream" },
    onOpen: function(socket) {
      console.log("Unix stream client connected");
      Device.write("/client_status", "connected");
      socket.write("hello from unix stream");
    },
    onClose: function() {
      console.log("Unix stream client disconnected");
      Device.write("/client_status", "disconnected");
    },
    onFail: function() {
      console.log("Unix stream client connection failed");
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
        name: "client_count",
        type: Ossia.Type.Int,
        value: 0
      },
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) {
          streamClient.write(v.value);
        }
      }
    ];
  }
}
