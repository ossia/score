import Ossia 1.0 as Ossia

Ossia.Mapper
{
  // OSC parser to decode raw bytes into structured OSC messages
  property var oscParser: Protocols.osc({
    onOsc: function(address, args) {
      console.log("OSC:", address, JSON.stringify(args));
      Device.write("/last_address", address);
      if(args.length > 0)
        Device.write("/last_value", args[0]);
    }
  })

  // Receive raw UDP and feed into OSC parser
  property var udpIn: Protocols.inboundUDP({
    Transport: { Bind: "0.0.0.0", Port: 7000 },
    onMessage: function(bytes) {
      oscParser.processMessage(bytes);
    }
  })

  // Send OSC over UDP
  property var udpOut: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 7001 }
  })

  function createTree() {
    return [
      {
        name: "last_address",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "last_value",
        type: Ossia.Type.Float,
        value: 0
      },
      {
        name: "send_float",
        type: Ossia.Type.Float,
        write: function(v) {
          udpOut.osc("/test/float", [v.value]);
        }
      },
      {
        name: "send_string",
        type: Ossia.Type.String,
        write: function(v) {
          udpOut.osc("/test/string", [v.value]);
        }
      }
    ];
  }
}
