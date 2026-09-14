import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var udpOut: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 9001 },
    onOpen: function(socket) {
      console.log("UDP outbound open");
      socket.write("hello from udp");
    },
    onClose: function() { console.log("UDP outbound closed"); },
    onError: function() { console.log("UDP outbound error"); }
  })

  function createTree() {
    return [
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) {
          udpOut.write(v.value);
        }
      },
      {
        name: "osc_send",
        type: Ossia.Type.Float,
        write: function(v) {
          udpOut.osc("/test/value", [v.value]);
        }
      }
    ];
  }
}
