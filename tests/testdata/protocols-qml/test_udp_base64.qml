import Ossia 1.0 as Ossia

// UDP with base64 encoding: each datagram carries base64-encoded data.
// Encoding on UDP is useful when the transport only accepts printable ASCII
// (e.g. certain serial-to-WiFi bridges, text-mode radio links).
Ossia.Mapper
{
  property int messageCount: 0

  property var receiver: Protocols.inboundUDP({
    Transport: { Bind: "0.0.0.0", Port: 7010 },
    Encoding: { type: "base64" },
    onMessage: function(msg, sender) {
      messageCount++;
      var text = msg.toString();
      console.log("Received (" + messageCount + ") from",
                  sender.host + ":" + sender.port, ":", text);
      Device.write("/last_message", text);
      sender.reply("ack:" + messageCount);
    }
  })

  property var sender: Protocols.outboundUDP({
    Transport: { Host: "127.0.0.1", Port: 7010 },
    Encoding: { type: "base64" },
    onOpen: function(sock) {
      console.log("UDP base64 sender ready");
      sock.write("hello base64 over udp");
    }
  })

  function createTree() {
    return [
      { name: "last_message", type: Ossia.Type.String, value: "" },
      {
        name: "send",
        type: Ossia.Type.String,
        write: function(v) { sender.write(v.value); }
      }
    ];
  }
}
