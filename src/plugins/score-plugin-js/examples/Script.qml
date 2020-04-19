import QtQml 2.0
import Score 1.0

QtObject {
    ValueInlet { id: in1 }
    ValueInlet { id: in2 }
    ValueOutlet { id: out }

    function onTick(time, position, offset) {
      out.val = in1.val * Math.rand() + in2.val;
    }
}
