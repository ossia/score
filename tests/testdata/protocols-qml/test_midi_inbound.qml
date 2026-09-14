import Ossia 1.0 as Ossia
// Component.onCompleted requires the QtQml attached Component type.
import QtQml

Ossia.Mapper
{
  property var midiDevices: Protocols.inboundMIDIDevices()

  property var midiIn: midiDevices.length > 0
    ? Protocols.inboundMIDI({
        Transport: midiDevices[0],
        onOpen: function(socket) {
          console.log("MIDI input opened:", JSON.stringify(midiDevices[0]));
          Device.write("/status", "open");
        },
        onClose: function() {
          console.log("MIDI input closed");
          Device.write("/status", "closed");
        },
        onError: function(err) {
          console.log("MIDI input error:", err);
          Device.write("/status", "error");
        },
        onMessage: function(msg) {
          console.log("MIDI message - bytes:", JSON.stringify(msg.bytes),
                       "timestamp:", msg.timestamp);
          Device.write("/last_status", msg.bytes[0]);
          if(msg.bytes.length > 1)
            Device.write("/last_data1", msg.bytes[1]);
          if(msg.bytes.length > 2)
            Device.write("/last_data2", msg.bytes[2]);
        }
      })
    : null

  Component.onCompleted: {
    console.log("Available MIDI inputs:", midiDevices.length);
    for(var i = 0; i < midiDevices.length; i++) {
      console.log("  [" + i + "]", JSON.stringify(midiDevices[i]));
    }
  }

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: midiDevices.length > 0 ? "opening" : "no_devices"
      },
      {
        name: "last_status",
        type: Ossia.Type.Int,
        value: 0
      },
      {
        name: "last_data1",
        type: Ossia.Type.Int,
        value: 0
      },
      {
        name: "last_data2",
        type: Ossia.Type.Int,
        value: 0
      }
    ];
  }
}
