import Ossia 1.0 as Ossia

// Line-delimiter framing loopback: messages are delimited by "\r\n".
// Useful for text protocols (NMEA, Telnet, HTTP-style, Art-Net text, etc.)
Ossia.Mapper
{
  property var clients: []

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5602 },
    Framing: { type: "line", delimiter: "\r\n" },
    onOpen: function() {
      console.log("Line server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);

      conn.onClose = function() {
        var idx = clients.indexOf(conn);
        if(idx > -1) clients.splice(idx, 1);
      };

      // Each call delivers one complete line (delimiter stripped by decoder)
      conn.receive(function(line) {
        var text = line.toString();
        console.log("Line server received:", JSON.stringify(text));
        Device.write("/server_received", text);
        // Reply (delimiter appended automatically by encoder)
        conn.write("OK:" + text);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5602 },
    Framing: { type: "line", delimiter: "\r\n" },
    onOpen: function(sock) {
      console.log("Line client connected");
      // Each write() appends \r\n automatically
      sock.write("HELLO");
      sock.write("WORLD");
    },
    onMessage: function(line) {
      console.log("Line client received:", line.toString());
      Device.write("/client_received", line.toString());
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
