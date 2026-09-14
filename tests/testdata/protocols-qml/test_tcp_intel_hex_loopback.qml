import Ossia 1.0 as Ossia

// Intel HEX encoding loopback: data is wrapped in Intel HEX records on the wire.
// This is common for firmware transfer over serial (UART bridges, bootloaders).
// Combined with line framing since Intel HEX is inherently line-oriented.
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5613 },
    Framing: { type: "line" },
    Encoding: { type: "intel_hex" },
    onOpen: function() {
      console.log("Intel HEX server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);

      conn.onClose = function() {
        var idx = clients.indexOf(conn);
        if(idx > -1) clients.splice(idx, 1);
      };

      conn.receive(function(msg) {
        var bytes = new Uint8Array(msg);
        console.log("Intel HEX server received", bytes.length, "bytes");
        Device.write("/received_length", bytes.length);
        // Echo the raw data back (will be re-encoded as Intel HEX)
        conn.write(msg);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5613 },
    Framing: { type: "line" },
    Encoding: { type: "intel_hex" },
    onOpen: function(sock) {
      console.log("Intel HEX client connected");
      sock.write("firmware-chunk-01");
    },
    onMessage: function(msg) {
      var bytes = new Uint8Array(msg);
      console.log("Intel HEX echo:", bytes.length, "bytes");
      Device.write("/echo_length", bytes.length);
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "received_length", type: Ossia.Type.Int, value: 0 },
      { name: "echo_length", type: Ossia.Type.Int, value: 0 }
    ];
  }
}
