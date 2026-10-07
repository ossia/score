// The OSC type tags a script can ask for, against the untyped default.
//
// A JavaScript number is a double and nothing else, so an OSC message built
// from one can only carry floats. SpatGRIS's /spat/serv grammar wants an int
// source index followed by floats, whatever the coordinates happen to be, and
// no amount of looking at the values can tell the two apart: the tags come
// from the script, which is the only thing that knows the grammar at the far
// end.

import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var udpOut: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 7100 }
  })

  function createTree() {
    return [
      // "sifffff": the index is an int, the five coordinates are floats even
      // though -90, 0 and 1 are all integral.
      {
        name: "spat",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/spat/serv", ["deg", v.value, -90, 0, 1, 0.4, 0.6], "sifffff");
        }
      },
      // No tag string: the pre-existing behaviour, every number a float.
      {
        name: "untyped",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/test/untyped", [v.value]);
        }
      },
      // One argument per tag that carries a payload, plus the two that do not.
      {
        name: "all",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/test/all", [1, 2.5, "hi", true, false], "ifsTF");
        }
      },
      // A tag string shorter than the argument list: the arguments it does not
      // cover keep the untyped conversion rather than disappearing.
      {
        name: "partial",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/test/partial", [3, 4], "i");
        }
      }
    ];
  }
}
