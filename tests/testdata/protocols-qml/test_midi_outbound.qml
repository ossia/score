import Ossia 1.0 as Ossia
// Component.onCompleted requires the QtQml attached Component type.
import QtQml

Ossia.Mapper
{
  property var midiDevices: Protocols.outboundMIDIDevices()

  property var midiOut: midiDevices.length > 0
    ? Protocols.outboundMIDI({
        Transport: midiDevices[0],
        onOpen: function(socket) {
          console.log("MIDI output opened:", JSON.stringify(midiDevices[0]));
          Device.write("/status", "open");
        },
        onClose: function() {
          console.log("MIDI output closed");
          Device.write("/status", "closed");
        },
        onError: function(err) {
          console.log("MIDI output error:", err);
          Device.write("/status", "error");
        }
      })
    : null

  Component.onCompleted: {
    console.log("Available MIDI outputs:", midiDevices.length);
  }

  function createTree() {
    return [
      {
        name: "status",
        type: Ossia.Type.String,
        value: midiDevices.length > 0 ? "opening" : "no_devices"
      },
      {
        name: "note_on",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [channel, note, velocity]
          if(midiOut && v.value.length >= 3)
            midiOut.sendNoteOn(v.value[0].value, v.value[1].value, v.value[2].value);
        }
      },
      {
        name: "note_off",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [channel, note, velocity]
          if(midiOut && v.value.length >= 3)
            midiOut.sendNoteOff(v.value[0].value, v.value[1].value, v.value[2].value);
        }
      },
      {
        name: "cc",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [channel, controller, value]
          if(midiOut && v.value.length >= 3)
            midiOut.sendControlChange(v.value[0].value, v.value[1].value, v.value[2].value);
        }
      },
      {
        name: "program",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects [channel, program]
          if(midiOut && v.value.length >= 2)
            midiOut.sendProgramChange(v.value[0].value, v.value[1].value);
        }
      },
      {
        name: "raw",
        type: Ossia.Type.List,
        write: function(v) {
          // Expects raw MIDI bytes, e.g. [0x90, 60, 100]
          if(midiOut) {
            var bytes = [];
            for(var i = 0; i < v.value.length; i++)
              bytes.push(v.value[i].value);
            midiOut.sendMessage(bytes);
          }
        }
      }
    ];
  }
}
