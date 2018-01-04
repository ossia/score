import QtQuick 2.0
import Score 1.0
Item {
  // Les E/S de la boite
  IntSlider  { id: transpose; min: -127; max: 127; init: 0 }
  MidiInlet  { id: midiIn }
  MidiOutlet { id: midiOut }

  // Fonctions utilitaires
  function clamp(value, min, max)
  { return Math.min(Math.max(value, min), max); }

  function getMessageType(message)
  {
    var fb = message[0];
    if(fb >= 0xF0)
      return fb & 0xFF;
    else
      return fb & 0xF0;
  }

  function isNoteOff(message)
  { return getMessageType(message) == 0x80; }
  function isNoteOn(message)
  { return getMessageType(message) == 0x90; }
  function isPressure(message)
  { return getMessageType(message) == 0xA0; }
  function isCC(message)
  { return getMessageType(message) == 0xB0; }
  function isPC(message)
  { return getMessageType(message) == 0xC0; }
  function isAftertouch(message)
  { return getMessageType(message) == 0xD0; }
  function isPitchBend(message)
  { return getMessageType(message) == 0xE0; }

  function getChannel(data)
  {
    if ((data[0] & 0xF0) != 0xF0)
      return (data[0] & 0xF) + 1;
    return 0;
  }

  function getPitch(note_message)
  { return note_message[1]; }
  function setPitch(note_message, pitch)
  { note_message[1] = clamp(pitch, 0, 127); }

  function getVelocity(note_message)
  { return note_message[2]; }
  function setVelocity(note_message, vel)
  { note_message[2] = clamp(vel, 0, 127); }

  // On sauve les messages d'entrée pour pouvoir envoyer les bons note off.
  property var currentMessages: [];

  // Cette fonction est appelée à chaque tick.
  function onTick(oldtime, time, position, offset)
  {
    var messages = midiIn.messages();
    for(var i = 0; i < messages.length; ++i)
    {
      var message = messages[i];

      if(isNoteOn(message))
      {
        var oldPitch = getPitch(message);
        var newPitch = oldPitch + transpose.value;
        setPitch(message, newPitch);

        currentMessages.push([oldPitch, newPitch]);
      }
      else if(isNoteOff(message))
      {
        var oldPitch = getPitch(message);
        for(var j = 0; j < currentMessages.length; ++j)
        {
          var mess = currentMessages[j];
          if(mess[0] == oldPitch)
          {
            setPitch(message, mess[1]);
            currentMessages = currentMessages.filter(function (val) {
              return val[0] == oldPitch;
            });
            break;
          }
        }
      }
      midiOut.add(message);
    }
  }
}
