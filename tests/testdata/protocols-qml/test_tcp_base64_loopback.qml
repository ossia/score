import Ossia 1.0 as Ossia

// Base64 encoding loopback: binary data is base64-encoded on the wire.
// Encoding is orthogonal to framing — here we combine line framing
// with base64 so that each line carries one base64-encoded message.
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5610 },
    Framing: { type: "line" },
    Encoding: { type: "base64" },
    onOpen: function() {
      console.log("Base64 server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);

      conn.onClose = function() {
        var idx = clients.indexOf(conn);
        if(idx > -1) clients.splice(idx, 1);
      };

      // Data arrives already base64-decoded
      conn.receive(function(msg) {
        var text = msg.toString();
        console.log("Server received (decoded):", text);
        Device.write("/server_received", text);
        // Reply is automatically base64-encoded + line-framed
        conn.write("echo:" + text);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5610 },
    Framing: { type: "line" },
    Encoding: { type: "base64" },
    onOpen: function(sock) {
      console.log("Base64 client connected");
      // write() base64-encodes then appends line delimiter
      sock.write("hello base64");
      sock.write("binary-safe: \x00\x01\x02\xFF");
    },
    onMessage: function(msg) {
      console.log("Client received (decoded):", msg.toString());
      Device.write("/client_received", msg.toString());
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
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
