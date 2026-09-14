import Ossia 1.0 as Ossia

// Backward compatibility test: onBytes still delivers raw byte chunks
// even when Framing is configured. This verifies that existing scripts
// using onBytes continue to work.
Ossia.Mapper
{
  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5604 },
    // No framing on server - raw bytes
    onOpen: function() {
      console.log("Raw server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      // receive() with no framing gives raw byte chunks
      conn.receive(function(bytes) {
        console.log("Raw server received:", bytes.length, "bytes");
        Device.write("/server_received", bytes.toString());
      });
    }
  })

  // Client uses SLIP framing for write, but onBytes for raw receive
  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5604 },
    Framing: { type: "slip" },
    onOpen: function(sock) {
      console.log("Client connected");
      Device.write("/client_status", "connected");
      // write() will SLIP-encode
      sock.write("framed-data");
    },
    // onBytes receives raw bytes (SLIP-encoded on the wire, NOT decoded)
    onBytes: function(bytes) {
      console.log("Client raw bytes:", bytes.length);
      Device.write("/client_raw_bytes", bytes.toString());
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "server_received", type: Ossia.Type.String, value: "" },
      { name: "client_raw_bytes", type: Ossia.Type.String, value: "" }
    ];
  }
}
