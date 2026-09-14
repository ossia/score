import Ossia 1.0 as Ossia
// Component.onCompleted requires the QtQml attached Component type.
import QtQml

Ossia.Mapper
{
  property var umpDevices: Protocols.inboundUMPDevices()

  property var umpIn: umpDevices.length > 0
    ? Protocols.inboundUMP({
        Transport: umpDevices[0],
        onOpen: function(socket) {
          console.log("UMP input opened:", JSON.stringify(umpDevices[0]));
          Device.write("/status", "open");
        },
        onClose: function() {
          console.log("UMP input closed");
          Device.write("/status", "closed");
        },
        onError: function(err) {
          console.log("UMP input error:", err);
          Device.write("/status", "error");
        },
        onMessage: function(msg) {
          console.log("UMP message - words:", JSON.stringify(msg.words),
                       "timestamp:", msg.timestamp);
          Device.write("/last_word0", msg.words[0]);
          Device.write("/last_word1", msg.words[1]);
        }
      })
    : null

  Component.onCompleted: {
    console.log("Available UMP inputs:", umpDevices.length);
  }

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: umpDevices.length > 0 ? "opening" : "no_devices"
      },
      {
        name: "last_word0",
        type: Ossia.Type.Int,
        value: 0
      },
      {
        name: "last_word1",
        type: Ossia.Type.Int,
        value: 0
      }
    ];
  }
}
