#!/usr/bin/env python3

from collections import namedtuple
import itertools
import subprocess
import json

import zmq

IDENTITY_KEY = "identity"
ENDPOINT_KEY = "endpoint"
CONTROL_KEY = "control"
CARDS_KEY = "cards"
RANK_KEY = "rank"
SUIT_KEY = "suit"
PeerEntry = namedtuple("PeerEntry", (IDENTITY_KEY, CONTROL_KEY, ENDPOINT_KEY))
Card = namedtuple("Card", (RANK_KEY, SUIT_KEY))

INIT_COMMAND = b'init'
SHUFFLE_COMMAND = b'shuffle'
DRAW_COMMAND = b'draw'
REVEAL_COMMAND = b'reveal'
REVEAL_ALL_COMMAND = b'revealall'
TERMINATE_COMMAND = b'terminate'
ORDER_COMMAND = b'order'
PEERS_COMMAND = b'peers'
CARDS_COMMAND = b'cards'
ID_COMMAND = b'id'

def get_endpoint(port):
    return "tcp://127.0.0.1:%d" % port

REPLY_SUCCESS = [b'success']
PEERS = [
    PeerEntry("peer1", 5501, 5510),
    PeerEntry("peer2", 5502, 5520),
    PeerEntry("peer3", 5503, 5530),
    PeerEntry("peer4", 5504, 5540)]

CARD_RANGE = [list(range(i*13, (i+1)*13)) for i in range(len(PEERS))]

zmqctx = zmq.Context.instance()
servers = [
    subprocess.Popen(
        ["./cardserver", get_endpoint(peer.control),
         get_endpoint(peer.endpoint)]) for peer in PEERS]
sockets = [zmqctx.socket(zmq.PAIR) for peer in PEERS]

print("Init...")

for (n, (socket, peer)) in enumerate(zip(sockets, PEERS)):
    entries = [
        {IDENTITY_KEY: other.identity,
         ENDPOINT_KEY: get_endpoint(other.endpoint) if n >= m else None} for
        (m, other) in enumerate(PEERS) if n != m]
    socket.connect(get_endpoint(peer.control))
    socket.send(INIT_COMMAND, flags=zmq.SNDMORE)
    socket.send(ORDER_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(n, flags=zmq.SNDMORE)
    socket.send(PEERS_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(entries)

print("Receiving reply...")
    
for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS + [INIT_COMMAND])

print("Shuffling...")

for socket in sockets:
    socket.send(SHUFFLE_COMMAND)

print("Receiving reply...")

for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS + [SHUFFLE_COMMAND])

print("Drawing cards...")

for (i, socket) in enumerate(sockets):
    for j in range(len(sockets)):
        if i == j:
            socket.send(DRAW_COMMAND, flags=zmq.SNDMORE)
            socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(CARD_RANGE[j])
        else:
            socket.send(REVEAL_COMMAND, flags=zmq.SNDMORE)
            socket.send(ID_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(PEERS[j].identity, flags=zmq.SNDMORE)
            socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(CARD_RANGE[j])

print("Receiving reply...")

all_cards = set()
for (i, socket) in enumerate(sockets):
    for j in range(len(sockets)):
        if i == j:
            assert(socket.recv() == REPLY_SUCCESS[0])
            assert(socket.recv(flags=zmq.NOBLOCK) == DRAW_COMMAND)
            key = socket.recv_string(flags=zmq.NOBLOCK)
            assert(key == CARDS_KEY)
            cards = socket.recv_json(flags=zmq.NOBLOCK)
            assert(
                all(
                    (card is not None) == (k in CARD_RANGE[j])
                    for (k, card) in enumerate(cards)))
            all_cards |= set(Card(**cards[k]) for k in CARD_RANGE[j])
        else:
            assert(socket.recv_multipart() == REPLY_SUCCESS + [REVEAL_COMMAND])
assert(len(all_cards) == 52)

print("Revealing dummy...")

for socket in sockets:
    socket.send(REVEAL_ALL_COMMAND, flags=zmq.SNDMORE)
    socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(CARD_RANGE[0])

print("Receiving reply...")

dummy_cards = None
for socket in sockets:
    assert(socket.recv() == REPLY_SUCCESS[0])
    assert(socket.recv(flags=zmq.NOBLOCK) == REVEAL_ALL_COMMAND)
    key = socket.recv_string(flags=zmq.NOBLOCK)
    assert(key == CARDS_KEY)
    cards = socket.recv_json(flags=zmq.NOBLOCK)
    cards = [Card(**cards[k]) for k in CARD_RANGE[0]]
    if dummy_cards is None:
        dummy_cards = cards
    assert(cards == dummy_cards)

print("Terminate...")

for socket in sockets:
    socket.send(TERMINATE_COMMAND)

print("Cleanup...")
    
for (server, socket) in zip(servers, sockets):
    server.wait()
    socket.close()

zmqctx.destroy()
