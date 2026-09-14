import Ossia 1.0 as Ossia

// Size-prefix framing with binary data.
// Tests that arbitrary binary payloads are correctly framed and delivered intact.
// QByteArray is exposed as ArrayBuffer in QML — use .byteLength for size,
// new Uint8Array(msg) for element access.
Ossia.Mapper
{
  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5606 },
    Framing: { type: "size_prefix" },
    onOpen: function() {
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        var bytes = new Uint8Array(msg);
        Device.write("/received_length", bytes.length);
        console.log("Binary message received, length:", bytes.length);
        // Echo it back
        conn.write(msg);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5606 },
    Framing: { type: "size_prefix" },
    onOpen: function(sock) {
      Device.write("/client_status", "connected");

      // Send multiple messages rapidly - size-prefix ensures
      // they are correctly delimited even without natural boundaries
      sock.write("short");
      sock.write("a slightly longer message with more content");
      sock.write("x");  // single byte message
    },
    onMessage: function(msg) {
      var bytes = new Uint8Array(msg);
      console.log("Echo received, length:", bytes.length);
      Device.write("/echo_length", bytes.length);
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "received_length", type: Ossia.Type.Int, value: 0 },
      { name: "echo_length", type: Ossia.Type.Int, value: 0 }
    ];
  }
}
