import Ossia 1.0 as Ossia

// Mixed encoding demonstration: multiple connections with different encodings
// running simultaneously. Each encoding is paired with an appropriate framing.
Ossia.Mapper
{
  // --- Base64 over line framing (port 5620) ---
  property var b64Server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5620 },
    Framing: { type: "line" },
    Encoding: { type: "base64" },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        Device.write("/b64_received", msg.toString());
        conn.write("b64-ok");
      });
    }
  })

  property var b64Client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5620 },
    Framing: { type: "line" },
    Encoding: { type: "base64" },
    onOpen: function(sock) {
      sock.write("base64 message");
    },
    onMessage: function(msg) {
      Device.write("/b64_echo", msg.toString());
    }
  })

  // --- Hex over SLIP framing (port 5621) ---
  property var hexServer: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5621 },
    Framing: { type: "slip" },
    Encoding: { type: "hex" },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        Device.write("/hex_received", msg.toString());
        conn.write("hex-ok");
      });
    }
  })

  property var hexClient: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5621 },
    Framing: { type: "slip" },
    Encoding: { type: "hex" },
    onOpen: function(sock) {
      sock.write("hex message");
    },
    onMessage: function(msg) {
      Device.write("/hex_echo", msg.toString());
    }
  })

  // --- No encoding, size-prefix framing as control (port 5622) ---
  property var rawServer: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5622 },
    Framing: { type: "size_prefix" },
    onConnection: function(conn) {
      conn.receive(function(msg) {
        Device.write("/raw_received", msg.toString());
        conn.write("raw-ok");
      });
    }
  })

  property var rawClient: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5622 },
    Framing: { type: "size_prefix" },
    onOpen: function(sock) {
      sock.write("raw message");
    },
    onMessage: function(msg) {
      Device.write("/raw_echo", msg.toString());
    }
  })

  function createTree() {
    return [
      { name: "b64_received", type: Ossia.Type.String, value: "" },
      { name: "b64_echo", type: Ossia.Type.String, value: "" },
      { name: "hex_received", type: Ossia.Type.String, value: "" },
      { name: "hex_echo", type: Ossia.Type.String, value: "" },
      { name: "raw_received", type: Ossia.Type.String, value: "" },
      { name: "raw_echo", type: Ossia.Type.String, value: "" }
    ];
  }
}
