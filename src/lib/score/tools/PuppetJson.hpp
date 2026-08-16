#pragma once

// Shared helpers for the out-of-process plug-in scanners ("puppets"):
// JSON string escaping and command-line parsing. Header-only, no Qt —
// the puppets only link ossia + fmt.
//
// Scanner protocol: a puppet is spawned as
//     ossia-score-xxxpuppet <plugin-path> [request-id] [port] [token]
// and replies on ws://127.0.0.1:<port> with a single JSON object that
// echoes back "Request": <id> and "Token": "<token>". The host drops
// any reply whose token does not match the scan session, so replies
// from another score instance's puppets (or stale puppets from a
// previous scan) can never pollute the plug-in database.
// [port]/[token] are optional so that a puppet can still be run by hand
// against the legacy fixed port for debugging.

#include <charconv>
#include <string>
#include <string_view>

namespace score::puppet
{
//! Escape a string for inclusion inside a JSON string literal.
//! Plug-in metadata (names, vendors, descriptions) routinely contains
//! quotes; unescaped, one such plug-in silently breaks its whole reply.
inline std::string json_escape(std::string_view s)
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(s.size() + 8);
  for(unsigned char c : s)
  {
    switch(c)
    {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if(c < 0x20)
        {
          out += "\\u00";
          out += hex[(c >> 4) & 0xf];
          out += hex[c & 0xf];
        }
        else
        {
          out += (char)c;
        }
        break;
    }
  }
  return out;
}

inline std::string json_escape(const char* s)
{
  return s ? json_escape(std::string_view{s}) : std::string{};
}

struct puppet_arguments
{
  std::string path;
  int request_id{0};
  int port{0};
  std::string token;

  bool valid{false};
};

//! Parse `puppet <path> [id] [port] [token]`.
//! Missing id / port fall back to 0 / default_port; missing token stays
//! empty (accepted by hand-run debugging, rejected by a token-checking host).
inline puppet_arguments
parse_arguments(int argc, char** argv, int default_port) noexcept
{
  puppet_arguments res;
  res.port = default_port;
  if(argc <= 1)
    return res;

  res.path = argv[1];
  if(res.path.empty())
    return res;

  auto to_int = [](const char* str, int fallback) {
    int value{};
    std::string_view sv{str};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if(ec != std::errc{} || ptr != sv.data() + sv.size())
      return fallback;
    return value;
  };

  if(argc > 2)
    res.request_id = to_int(argv[2], 0);
  if(argc > 3)
  {
    if(int p = to_int(argv[3], 0); p > 0 && p <= 65535)
      res.port = p;
  }
  if(argc > 4)
    res.token = argv[4];

  res.valid = true;
  return res;
}
}
