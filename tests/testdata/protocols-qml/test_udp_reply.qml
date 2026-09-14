import Ossia 1.0 as Ossia

// UDP reply test: a "server" listens on a well-known port.
// A "client" sends a message to that port. The server receives the message
// along with the sender's address/port, and replies back using sender.reply().
// The client receives the reply on its ephemeral port.
Ossia.Mapper
{
  // UDP "server" listening on port 7000
  property var server: Protocols.inboundUDP({
    Transport: { Bind: "0.0.0.0", Port: 7000 },
    onMessage: function(msg, sender) {
      // QV4 needs a real Array, not a typed array, as apply()'s argument list.
      var text = Array.from(new Uint8Array(msg));
      var str = String.fromCharCode.apply(null, text);
      console.log("Server received from", sender.host + ":" + sender.port, ":", str);
      Device.write("/server_received", str);
      Device.write("/sender_host", sender.host);
      Device.write("/sender_port", sender.port);

      // Reply back to the sender using the same socket
      sender.reply("pong:" + str);
    }
  })

  // UDP "client" sending to the server
  property var client: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 7000 },
    onOpen: function(sock) {
      console.log("UDP client ready, sending ping");
      Device.write("/client_status", "open");
      sock.write("ping");
    }
  })

  function createTree() {
    return [
      { name: "server_received", type: Ossia.Type.String, value: "" },
      { name: "sender_host", type: Ossia.Type.String, value: "" },
      { name: "sender_port", type: Ossia.Type.Int, value: 0 },
      { name: "client_status", type: Ossia.Type.String, value: "idle" },
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) {
          client.write(v.value);
        }
      }
    ];
  }
}
