import Ossia 1.0 as Ossia

// Line-delimiter framing with plain "\n" (default delimiter).
// Simulates a simple text-command protocol.
Ossia.Mapper
{
  property var clients: []
  property int msgCount: 0

  property var server: Protocols.inboundTCP({
    Transport: { Bind: "127.0.0.1", Port: 5605 },
    Framing: { type: "line" },  // default delimiter is "\n"
    onOpen: function() {
      Device.write("/status", "listening");
    },
    onConnection: function(conn) {
      clients.push(conn);

      conn.receive(function(line) {
        msgCount++;
        var text = line.toString();
        console.log("Command #" + msgCount + ":", text);
        Device.write("/last_command", text);
        Device.write("/command_count", msgCount);

        // Simple command dispatcher
        if(text === "PING") {
          conn.write("PONG");
        } else if(text.startsWith("SET ")) {
          var parts = text.split(" ");
          if(parts.length >= 3)
            Device.write("/" + parts[1], parts[2]);
          conn.write("OK");
        } else {
          conn.write("ERR:unknown");
        }
      });
    }
  })

  property var client: Protocols.outboundTCP({
    Transport: { Host: "127.0.0.1", Port: 5605 },
    Framing: { type: "line" },
    onOpen: function(sock) {
      Device.write("/client_status", "connected");
      sock.write("PING");
      sock.write("SET volume 80");
      sock.write("SET brightness 50");
    },
    onMessage: function(line) {
      console.log("Client response:", line.toString());
      Device.write("/last_response", line.toString());
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      { name: "last_command", type: Ossia.Type.String, value: "" },
      { name: "command_count", type: Ossia.Type.Int, value: 0 },
      { name: "last_response", type: Ossia.Type.String, value: "" },
      { name: "volume", type: Ossia.Type.String, value: "" },
      { name: "brightness", type: Ossia.Type.String, value: "" }
    ];
  }
}
