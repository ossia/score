#pragma once

// Shared WebSocket client + main-loop boilerplate for the plug-in scanner
// puppets. Header-only; must only be included from the puppet executables
// (pulls in asio / ossia websockets).
//
// Behaviour (aligned on the battle-tested lv2puppet implementation):
//  * connect to ws://127.0.0.1:<port>, send the payload once both the
//    socket and the scan are ready;
//  * wait 500ms before exiting so the (async) send actually flushes,
//    then _Exit: websocketpp/asio destructors can throw from internals
//    when the server closes the connection first, which would turn a
//    perfectly successful scan into a non-zero exit;
//  * a watchdog kills the process if the socket never becomes ready.

#include <score/tools/PuppetJson.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/network/sockets/websocket.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace score::puppet
{
struct client
{
  boost::asio::io_context ctx;
  ossia::net::websocket_simple_client socket;

  bool socket_ready{}, payload_ready{};
  std::string payload;
  std::string log_name;
  bool echo_stdout{};

  explicit client(int port)
      : socket{{.url = fmt::format("ws://127.0.0.1:{}", port)}, ctx}
  {
    socket.on_open.connect<&client::on_open>(*this);
    socket.on_fail.connect<&client::on_error>(*this);
    socket.on_close.connect<&client::on_error>(*this);

    socket.websocket_client::connect(fmt::format("ws://127.0.0.1:{}", port));
  }

  void set_payload(std::string json)
  {
    payload = std::move(json);
    if(echo_stdout && !payload.empty())
    {
      std::fwrite(payload.data(), 1, payload.size(), stdout);
      std::fwrite("\n", 1, 1, stdout);
      std::fflush(stdout);
    }
    payload_ready = true;
    on_ready();
  }

  void on_ready()
  {
    if(socket_ready && payload_ready)
    {
      try
      {
        socket.send_message(payload);
      }
      catch(...)
      {
        on_error();
        return;
      }

      // Flush delay: send_message is async; large payloads (~MB) need time
      auto delay = std::make_shared<boost::asio::steady_timer>(ctx);
      delay->expires_after(std::chrono::milliseconds(500));
      delay->async_wait(
          [this, delay](auto) { std::_Exit(payload.empty() ? 1 : 0); });
    }
  }

  void on_error()
  {
    auto line = fmt::format("[{}] socket error\n", log_name);
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
    std::_Exit(1);
  }

  void on_open()
  {
    socket_ready = true;
    on_ready();
  }
};

//! Common main() implementation: parse args, run scan_fn(path, id, token),
//! ship the resulting JSON. scan_fn returns the full reply object as a
//! string; an empty string means the plug-in could not be scanned.
//!
//! echo_stdout only applies to manual runs (no session token): when spawned
//! by a host the parent never drains our stdout pipe, and a large reply
//! (lsp-plugins.clap: ~100KB for ~200 plug-ins) overflows the 64KB pipe
//! buffer and can stall the scan.
//!
//! watchdog_seconds guards against a host that never becomes reachable; it
//! should match the host-side scan timeout. It must never abort a scan that
//! already produced a payload: the scan itself runs synchronously in a
//! posted handler (the timer cannot fire mid-scan), and once the payload is
//! on its way out only the 500ms flush timer remains.
template <typename ScanFn>
int puppet_main(
    int argc, char** argv, int default_port, const char* name, bool echo_stdout,
    int watchdog_seconds, ScanFn&& scan_fn)
{
  const auto args = parse_arguments(argc, argv, default_port);
  if(!args.valid)
    return 1;

  client c{args.port};
  c.log_name = name;
  c.echo_stdout = echo_stdout && args.token.empty();

  boost::asio::post(c.ctx, [&] {
    c.set_payload(scan_fn(args.path, args.request_id, args.token));
  });

  boost::asio::steady_timer watchdog{c.ctx};
  watchdog.expires_after(std::chrono::seconds(watchdog_seconds));
  watchdog.async_wait([&](auto ec) {
    if(ec)
      return;
    // The send is already in flight (a scan longer than the watchdog leaves
    // both the expired timer and the socket ready; asio dispatches the
    // socket first): let the flush timer finish the exit instead
    if(c.socket_ready && c.payload_ready)
      return;
    std::fprintf(stderr, "[%s] timeout\n", name);
    std::_Exit(1);
  });

  c.ctx.run();
  c.ctx.restart();
  c.ctx.run();
  return c.payload.empty() ? 1 : 0;
}
}
