#!/bin/bash
# usage: netns-run.sh <catcher-log> cmd...   -- runs cmd in a private netns where all
# outgoing TCP/UDP lands in catcher.py
HERE=$(cd "$(dirname "$0")" && pwd)
LOG=$1; shift
LOOPBACK_RULE=""
[ -n "$NETNS_LOOPBACK_DIRECT" ] && LOOPBACK_RULE="ip daddr 127.0.0.0/8 accept"
export LOOPBACK_RULE
exec unshare -rn bash -c '
set -e
ip link set lo up
ip link add d0 type dummy; ip link set d0 up
ip addr add 10.99.0.1/8 dev d0
ip route add default dev d0
nft -f - <<NFT
table ip nat {
  chain out {
    type nat hook output priority -100; policy accept;
    $LOOPBACK_RULE
    tcp dport != 1 redirect to :1
    udp dport != 1 redirect to :1
  }
}
NFT
python3 '"$HERE"'/catcher.py "'"$LOG"'" &
CP=$!
for i in $(seq 50); do grep -q ready "'"$LOG"'" 2>/dev/null && break; sleep 0.05; done
set +e
"$@"
rc=$?
kill $CP
exit $rc
' bash "$@"
