import Ossia 1.0 as Ossia

// OSC over TCP with SLIP framing (standard approach per OSC 1.1 spec).
// The server receives SLIP-decoded frames and parses them as OSC.
// The client sends OSC messages via osc() which are SLIP-encoded on the wire.
Ossia.Mapper
{
  // OSC parser for decoding binary OSC packets received over TCP
  property var oscParser: Protocols.osc({
    onOsc: function(address, args) {
      console.log("OSC received:", address, JSON.stringify(args));
      Device.write("/osc_address", address);
      if(args.length > 0)
        Device.write("/osc_value", args[0]);
    }
  })

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5603 },
    Framing: { type: "slip" },
    onOpen: function() {
      console.log("OSC/SLIP server ready");
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      // Each SLIP frame is a complete OSC packet
      conn.receive(function(frame) {
        oscParser.processMessage(frame);
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5603 },
    Framing: { type: "slip" },
    onOpen: function(sock) {
      console.log("OSC/SLIP client connected");
      Device.write("/client_status", "connected");
      // osc() builds an OSC packet then SLIP-encodes via write()
      sock.osc("/sensor/temperature", [22.5]);
      sock.osc("/sensor/humidity", [65]);
      sock.osc("/trigger", []);
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "osc_address", type: Ossia.Type.String, value: "" },
      { name: "osc_value", type: Ossia.Type.Float, value: 0.0 }
    ];
  }
}
