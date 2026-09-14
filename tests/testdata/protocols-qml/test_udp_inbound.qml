import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property int messageCount: 0

  property var udpIn: Protocols.inboundUDP({
    Transport: { Bind: "0.0.0.0", Port: 9001 },
    onOpen: function(socket) {
      console.log("UDP inbound listening on port 9001");
    },
    onClose: function() { console.log("UDP inbound closed"); },
    onError: function() { console.log("UDP inbound error"); },
    onMessage: function(bytes) {
      messageCount++;
      console.log("UDP received (" + messageCount + "):", bytes);
      Device.write("/last_message", bytes.toString());
    }
  })

  function createTree() {
    return [
      {
        name: "last_message",
        type: Ossia.Type.String,
        value: ""
      }
    ];
  }
}
