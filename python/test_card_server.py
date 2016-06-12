#!/usr/bin/env python3

from collections import namedtuple
import subprocess
import json

import zmq

IDENTITY_KEY = "identity"
ENDPOINT_KEY = "endpoint"
PeerEntry = namedtuple("PeerEntry", (IDENTITY_KEY, ENDPOINT_KEY))

INIT_COMMAND = b'init'
TERMINATE_COMMAND = b'terminate'

REPLY_SUCCESS = [b'success']
PEERS = [
    PeerEntry("peer1", "tcp://127.0.0.1:5555"),
    PeerEntry("peer2", "tcp://127.0.0.1:5556")]

zmqctx = zmq.Context.instance()
servers = [
    subprocess.Popen(["./cardserver", peer.endpoint]) for peer in PEERS]
sockets = [zmqctx.socket(zmq.PAIR) for peer in PEERS]

print("Init...")

for (i, (socket, peer)) in enumerate(zip(sockets, PEERS)):
    def get_entry(i, j, peer):
        if i == j:
            return None
        return peer._asdict()
    entries = [get_entry(i, j, peer) for (j, peer) in enumerate(PEERS)]
    socket.connect(peer.endpoint)
    socket.send(INIT_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(entries)

print("Receiving reply...")
    
for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS + [INIT_COMMAND])

print("Terminate...")

for socket in sockets:
    socket.send(TERMINATE_COMMAND)

print("Cleanup...")
    
for (server, socket) in zip(servers, sockets):
    server.wait()
    socket.close()

zmqctx.destroy()
