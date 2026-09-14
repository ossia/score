import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var unixOut: Protocols.outboundUnixDatagram({
    Transport: { Path: "/tmp/ossia_test_dgram" },
    onOpen: function(socket) {
      console.log("Unix datagram outbound open");
    },
    onClose: function() { console.log("Unix datagram outbound closed"); },
    onError: function() { console.log("Unix datagram outbound error"); }
  })

  property var unixIn: Protocols.inboundUnixDatagram({
    Transport: { Path: "/tmp/ossia_test_dgram" },
    onOpen: function(socket) {
      console.log("Unix datagram inbound listening");
    },
    onClose: function() { console.log("Unix datagram inbound closed"); },
    onError: function() { console.log("Unix datagram inbound error"); },
    onMessage: function(bytes) {
      console.log("Unix datagram received:", bytes);
      Device.write("/last_message", bytes.toString());
    }
  })

  function createTree() {
    return [
      {
        name: "last_message",
        type: Ossia.Type.String,
        value: ""
      },
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) {
          unixOut.write(v.value);
        }
      }
    ];
  }
}
