import Ossia 1.0 as Ossia

// Ascii85 encoding loopback: more space-efficient than base64
// (4 bytes become 5 characters instead of 4 bytes becoming ~5.3 characters).
// Combined with size-prefix framing so no delimiter conflicts arise.
Ossia.Mapper
{
  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5612 },
    Framing: { type: "size_prefix" },
    Encoding: { type: "ascii85" },
    onOpen: function() {
      console.log("Ascii85 server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        var text = msg.toString();
        console.log("Ascii85 server received:", text);
        Device.write("/server_received", text);
        conn.write("re:" + text);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5612 },
    Framing: { type: "size_prefix" },
    Encoding: { type: "ascii85" },
    onOpen: function(sock) {
      console.log("Ascii85 client connected");
      sock.write("compact encoding");
      sock.write("\x00\x00\x00\x00");  // all-zero block compresses to 'z'
    },
    onMessage: function(msg) {
      console.log("Ascii85 client received:", msg.toString());
      Device.write("/client_received", msg.toString());
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "server_received", type: Ossia.Type.String, value: "" },
      { name: "client_received", type: Ossia.Type.String, value: "" }
    ];
  }
}
