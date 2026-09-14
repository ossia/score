import Ossia 1.0 as Ossia

// Hex encoding loopback: data is transmitted as hex strings on the wire.
// Combined with line framing, each line is a hex-encoded message.
// This is common for serial device debugging and simple text protocols.
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5611 },
    Framing: { type: "line", delimiter: "\r\n" },
    Encoding: { type: "hex" },
    onOpen: function() {
      console.log("Hex server ready");
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
        console.log("Hex server received (decoded):", text);
        Device.write("/server_received", text);
        conn.write("OK");
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5611 },
    Framing: { type: "line", delimiter: "\r\n" },
    Encoding: { type: "hex" },
    onOpen: function(sock) {
      console.log("Hex client connected");
      // "Hello" becomes "48656C6C6F" on the wire
      sock.write("Hello");
      sock.write("World");
    },
    onMessage: function(msg) {
      console.log("Hex client received:", msg.toString());
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
