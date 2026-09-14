import Ossia 1.0 as Ossia

// SLIP framing loopback: server and client both use SLIP framing.
// The client sends messages that are SLIP-encoded on the wire;
// the server's onMessage callback receives complete decoded frames.
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5600 },
    Framing: { type: "slip" },
    onOpen: function() {
      console.log("SLIP server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);
      console.log("SLIP server: client connected");

      conn.onClose = function() {
        var idx = clients.indexOf(conn);
        if(idx > -1) clients.splice(idx, 1);
      };

      // receive() delivers complete SLIP frames
      conn.receive(function(msg) {
        var text = msg.toString();
        console.log("SLIP server received frame:", text);
        Device.write("/server_received", text);
        // Echo back with framing applied automatically
        conn.write("echo:" + text);
      });
    },
    onClose: function() {
      Device.write("/status", "closed");
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5600 },
    Framing: { type: "slip" },
    onOpen: function(sock) {
      console.log("SLIP client connected");
      Device.write("/client_status", "connected");
      // write() automatically SLIP-encodes
      sock.write("hello-slip");
    },
    onMessage: function(msg) {
      // Complete SLIP frame from server echo
      console.log("SLIP client received:", msg.toString());
      Device.write("/client_received", msg.toString());
    },
    onClose: function() {
      Device.write("/client_status", "disconnected");
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "server_received", type: Ossia.Type.String, value: "" },
      { name: "client_received", type: Ossia.Type.String, value: "" },
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) { client.write(v.value); }
      }
    ];
  }
}
