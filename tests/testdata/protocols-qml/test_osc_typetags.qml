// Type tags a script can ask for, against the untyped default: a JS number is
// a double, so without tags every argument goes out as a float.

import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var udpOut: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 7100 }
  })

  function createTree() {
    return [
      // SpatGRIS /spat/serv: int index, then floats, integral values included.
      {
        name: "spat",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/spat/serv", ["deg", v.value, -90, 0, 1, 0.4, 0.6], "sifffff");
        }
      },
      // No tags: every number a float, as before.
      {
        name: "untyped",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/test/untyped", [v.value]);
        }
      },
      // T and F carry no payload.
      {
        name: "all",
        type: Ossia.Type.Int,
        write: function(v) {
          udpOut.osc("/test/all", [1, 2.5, "hi", true, false], "ifsTF");
        }
      },
      // Fewer tags than arguments: the rest fall back to the untyped path.
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
