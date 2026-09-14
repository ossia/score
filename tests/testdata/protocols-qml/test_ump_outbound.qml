import Ossia 1.0 as Ossia
// Component.onCompleted requires the QtQml attached Component type.
import QtQml

Ossia.Mapper
{
  property var umpDevices: Protocols.outboundUMPDevices()

  property var umpOut: umpDevices.length > 0
    ? Protocols.outboundUMP({
        Transport: umpDevices[0],
        onOpen: function(socket) {
          console.log("UMP output opened:", JSON.stringify(umpDevices[0]));
          Device.write("/status", "open");
        },
        onClose: function() {
          console.log("UMP output closed");
          Device.write("/status", "closed");
        },
        onError: function(err) {
          console.log("UMP output error:", err);
          Device.write("/status", "error");
        }
      })
    : null

  Component.onCompleted: {
    console.log("Available UMP outputs:", umpDevices.length);
  }

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: umpDevices.length > 0 ? "opening" : "no_devices"
      },
      {
        name: "note_on",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [group, channel, note, velocity, attrType, attr]
          if(umpOut && v.value.length >= 6)
            umpOut.sendNoteOn(
              v.value[0].value, v.value[1].value, v.value[2].value,
              v.value[3].value, v.value[4].value, v.value[5].value);
        }
      },
      {
        name: "note_off",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [group, channel, note, velocity, attrType, attr]
          if(umpOut && v.value.length >= 6)
            umpOut.sendNoteOff(
              v.value[0].value, v.value[1].value, v.value[2].value,
              v.value[3].value, v.value[4].value, v.value[5].value);
        }
      },
      {
        name: "cc",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [group, channel, controller, value]
          if(umpOut && v.value.length >= 4)
            umpOut.sendControlChange(
              v.value[0].value, v.value[1].value,
              v.value[2].value, v.value[3].value);
        }
      },
      {
        name: "raw",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects 4 UMP words
          if(umpOut && v.value.length >= 4) {
            var words = [];
            for(var i = 0; i < 4; i++)
              words.push(v.value[i].value);
            umpOut.sendMessage(words);
          }
        }
      }
    ];
  }
}
