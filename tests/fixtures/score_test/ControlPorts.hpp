#pragma once

// The local OSC / WebSocket ports of a score process a test drives over OSC,
// the C++ side of tests/integration/common/control-ports.sh. Every score
// process opens that device, on 6666 / 9999 unless told otherwise, and the one
// holding the port is the one that gets the messages: each run gets ports of
// its own.

#include <QByteArray>
#include <QHostAddress>
#include <QProcessEnvironment>
#include <QString>
#include <QTcpServer>
#include <QUdpSocket>

namespace score::test::app
{

struct control_ports
{
  quint16 osc{};
  quint16 ws{};

  //! Ports nothing is bound to right now; both 0 when none could be had.
  static control_ports pick()
  {
    QUdpSocket udp;
    QTcpServer tcp;
    if(!udp.bind(QHostAddress::AnyIPv4, 0) || !tcp.listen(QHostAddress::AnyIPv4, 0))
      return {};
    return {udp.localPort(), tcp.serverPort()};
  }

  explicit operator bool() const noexcept { return osc != 0 && ws != 0; }

  //! Makes the process started with \p env listen on these ports.
  void apply(QProcessEnvironment& env) const
  {
    env.insert(QStringLiteral("SCORE_LOCAL_OSC_PORT"), QString::number(osc));
    env.insert(QStringLiteral("SCORE_LOCAL_WS_PORT"), QString::number(ws));
  }

  //! The datagram `oscsend 127.0.0.1 <osc> <address> s <arg>` sends.
  void send(const QByteArray& address, const QByteArray& arg) const
  {
    auto pad = [](QByteArray b) {
      b.append('\0');
      while(b.size() % 4)
        b.append('\0');
      return b;
    };
    QUdpSocket{}.writeDatagram(
        pad(address) + pad(",s") + pad(arg), QHostAddress::LocalHost, osc);
  }
};

}
