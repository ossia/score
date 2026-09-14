import Ossia 1.0 as Ossia

// Size-prefix framing loopback: each message is prefixed with a 4-byte
// big-endian length on the wire. The server and client both use size_prefix
// framing so write()/receive() handle the framing transparently.
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5601 },
    Framing: { type: "size_prefix" },
    onOpen: function() {
      console.log("Size-prefix server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);

      conn.onClose = function() {
        var idx = clients.indexOf(conn);
        if(idx > -1) clients.splice(idx, 1);
      };

      conn.receive(function(msg) {
        var text = msg.toString();
        console.log("Size-prefix server received:", text);
        Device.write("/server_received", text);
        conn.write("ack:" + text);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5601 },
    Framing: { type: "size_prefix" },
    onOpen: function(sock) {
      console.log("Size-prefix client connected");
      Device.write("/client_status", "connected");
      sock.write("msg-one");
      sock.write("msg-two");
    },
    onMessage: function(msg) {
      console.log("Size-prefix client received:", msg.toString());
      Device.write("/client_received", msg.toString());
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "server_received", type: Ossia.Type.String, value: "" },
      { name: "client_received", type: Ossia.Type.String, value: "" }
    ];
  }
}
