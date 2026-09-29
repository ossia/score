# Sourced by the harnesses that drive ossia-score over its local OSC device.
#
# Every score process opens that device, on 6666 / 9999 unless told otherwise:
# test binaries, other harnesses and a developer's own session included, and
# none of them take a lock. Whichever holds the port gets the harness's
# messages, and the app under test waits for them until its timeout. Each app
# run therefore listens on ports picked free for it:
#
#   pick_control_ports || exit 4
#   env SCORE_LOCAL_OSC_PORT="$OSC" SCORE_LOCAL_WS_PORT="$WS" ossia-score ...
#   oscsend 127.0.0.1 "$OSC" /script s "..."

# free_port tcp|udp -> a port nothing is bound to right now
free_port() {
  python3 -c 'import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM if sys.argv[1] == "tcp" else socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 0))
print(s.getsockname()[1])' "$1"
}

# Sets OSC and WS for one app run.
pick_control_ports() {
  OSC=$(free_port udp) && WS=$(free_port tcp) && [ -n "$OSC" ] && [ -n "$WS" ] \
    || { echo "no free control port (python3 missing?)" >&2; return 1; }
}
