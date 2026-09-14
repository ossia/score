import Ossia 1.0 as Ossia

// Motorola S-Record encoding loopback: data is wrapped in SREC format on the wire.
// Like Intel HEX, commonly used for firmware transfer over serial links.
Ossia.Mapper
{
  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5614 },
    Framing: { type: "line" },
    Encoding: { type: "srec" },
    onOpen: function() {
      console.log("S-Record server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        var bytes = new Uint8Array(msg);
        console.log("S-Record server received", bytes.length, "bytes");
        Device.write("/received_length", bytes.length);
        conn.write(msg);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5614 },
    Framing: { type: "line" },
    Encoding: { type: "srec" },
    onOpen: function(sock) {
      console.log("S-Record client connected");
      sock.write("srec-payload-data");
    },
    onMessage: function(msg) {
      var bytes = new Uint8Array(msg);
      console.log("S-Record echo:", bytes.length, "bytes");
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
