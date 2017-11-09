import QtQuick 2.0
import Score 1.0
Item {
  ValueInlet { id: in1 }
  AudioOutlet { id: out1 }

  property int idx: 0;
  function onTick(oldtime, time, position, offset) {
    var arr = [ ];
    var n = time - oldtime;
    var freq = in1.value;
    if(n > 0) {
      for(var s = 0; s < n; s++) {
        var sample = Math.sin(2 * Math.PI * (2 * freq) * (idx++) / 44100);
        sample = sample > 0 ? 1 : -1;
        sample = freq > 0 ? sample : 0;
        arr[offset + s] = 0.3 * sample;
      }
    }
    out1.setChannel(0, arr);
    out1.setChannel(1, arr);
  }
}

