from collections import namedtuple
import atexit
import binascii
import signal
import subprocess
import sys
import json

import zmq
from zmq.utils import z85

IDENTITY_KEY = "id"
ENDPOINT_KEY = "endpoint"
SERVER_KEY_KEY = "serverKey"
CONTROL_KEY = "control"
CARDS_KEY = "cards"
RANK_KEY = "rank"
SUIT_KEY = "suit"
PeerEntry = namedtuple("PeerEntry", (IDENTITY_KEY, CONTROL_KEY, ENDPOINT_KEY))
Card = namedtuple("Card", (RANK_KEY, SUIT_KEY))

TAG = b'tag'
INIT_COMMAND = b'init'
SHUFFLE_COMMAND = b'shuffle'
DRAW_COMMAND = b'draw'
REVEAL_COMMAND = b'reveal'
REVEAL_ALL_COMMAND = b'revealall'
TERMINATE_COMMAND = b'terminate'
ORDER_COMMAND = b'order'
PEERS_COMMAND = b'peers'
CARDS_COMMAND = b'cards'

CURVE_KEYS = [
    ("G-Lq6{EbJ/C</gpvtK3V:4Sx[hsdePYi7[]4a3Nx","}Nd:*=$4Fvzi5ehoQw/ew8tZ/XKI.C8o5YBqJcMR"),
    ("yR#}WrT5Dl1s3Xxw+G%BvUcY<NHQu%Av&[M>C+E5","G#y%?EjwByGvT8Rv8Rz*J/(jDz%n-$WYUF+3L=i1"),
    (".&)GUnu8a70oXRWHHjngNH1<J0N7Y7P<3k*>Y{c2","f+M<*^Z+G[W(I:u3#63BrjDz<u]T9y!)DPrHaY/C"),
    ("f:9%CxV-I3m})?r>oTj0hh1=fGOkQL??m45m?P2v","Rxh16PWco1Eq]L>d5l#k5Udw([NsxUFi}h24G3}J"),
]

def get_endpoint(port):
    return "tcp://127.0.0.1:%d" % port

def cleanup(servers):
    print("Cleanup...")
    for server in servers:
        server.terminate()
        server.wait()
        print(server.stderr.read().decode())

REPLY_SUCCESS = [b'', TAG, b'OK']
PEERS = [
    PeerEntry("12", 5501, 5510),
    PeerEntry("34", 5502, 5520),
    PeerEntry("56", 5503, 5530),
    PeerEntry("78", 5504, 5540)]

CARD_RANGE = [list(range(i*13, (i+1)*13)) for i in range(len(PEERS))]

zmqctx = zmq.Context.instance()
servers = [
    subprocess.Popen(
       [sys.argv[1],
         '-vv',
         '--secret-key-file=-',
         '--public-key-file=-',
         '--security-parameter=2',
         get_endpoint(peer.control),
         get_endpoint(peer.endpoint)],
        stdin=subprocess.PIPE, stderr=subprocess.PIPE) for peer in PEERS]
for server, keys in zip(servers, CURVE_KEYS):
    server.stdin.write(keys[0].encode())
    server.stdin.write(b'\n')
    server.stdin.write(keys[1].encode())
    server.stdin.write(b'\n')
    server.stdin.close()
sockets = [zmqctx.socket(zmq.DEALER) for peer in PEERS]

atexit.register(cleanup, servers);

print("Init...")

for (n, (socket, peer, keys)) in enumerate(zip(sockets, PEERS, CURVE_KEYS)):
    entries = [
        {
            IDENTITY_KEY: other.id,
            ENDPOINT_KEY: get_endpoint(other.endpoint),
            SERVER_KEY_KEY: binascii.hexlify(z85.decode(other_keys[1])).decode()
        } for (m, (other, other_keys)) in enumerate(zip(PEERS, CURVE_KEYS)) if n != m
    ]
    socket.curve_serverkey = keys[1].encode() + b'\0'
    socket.curve_publickey = keys[1].encode() + b'\0'
    socket.curve_secretkey = keys[0].encode() + b'\0'
    socket.connect(get_endpoint(peer.control))
    socket.send(b'', flags=zmq.SNDMORE)
    socket.send(TAG, flags=zmq.SNDMORE)
    socket.send(INIT_COMMAND, flags=zmq.SNDMORE)
    socket.send(ORDER_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(n, flags=zmq.SNDMORE)
    socket.send(PEERS_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(entries)

print("Receiving reply...")

for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS)

print("Shuffling...")

for socket in sockets:
    socket.send(b'', flags=zmq.SNDMORE)
    socket.send(TAG, flags=zmq.SNDMORE)
    socket.send(SHUFFLE_COMMAND)

print("Receiving reply...")

for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS)

print("Drawing cards...")

for (i, socket) in enumerate(sockets):
    for j in range(len(sockets)):
        if i == j:
            socket.send(b'', flags=zmq.SNDMORE)
            socket.send(TAG, flags=zmq.SNDMORE)
            socket.send(DRAW_COMMAND, flags=zmq.SNDMORE)
            socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(CARD_RANGE[j])
        else:
            socket.send(b'', flags=zmq.SNDMORE)
            socket.send(TAG, flags=zmq.SNDMORE)
            socket.send(REVEAL_COMMAND, flags=zmq.SNDMORE)
            socket.send(ORDER_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(j, flags=zmq.SNDMORE)
            socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
            socket.send_json(CARD_RANGE[j])

print("Receiving reply...")

all_cards = set()
for (i, socket) in enumerate(sockets):
    for j in range(len(sockets)):
        if i == j:
            assert([socket.recv(), socket.recv(), socket.recv()] == REPLY_SUCCESS)
            key = socket.recv_string(flags=zmq.NOBLOCK)
            assert(key == CARDS_KEY)
            cards = socket.recv_json(flags=zmq.NOBLOCK)
            assert(
                all(
                    (card is not None) == (k in CARD_RANGE[j])
                    for (k, card) in enumerate(cards)))
            all_cards |= set(Card(**cards[k]) for k in CARD_RANGE[j])
        else:
            assert(socket.recv_multipart() == REPLY_SUCCESS)
assert(len(all_cards) == 52)

print("Revealing dummy...")

for socket in sockets:
    socket.send(b'', flags=zmq.SNDMORE)
    socket.send(TAG, flags=zmq.SNDMORE)
    socket.send(REVEAL_ALL_COMMAND, flags=zmq.SNDMORE)
    socket.send(CARDS_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(CARD_RANGE[0])

print("Receiving reply...")

dummy_cards = None
for socket in sockets:
    assert([socket.recv(), socket.recv(), socket.recv()] == REPLY_SUCCESS)
    key = socket.recv_string(flags=zmq.NOBLOCK)
    assert(key == CARDS_KEY)
    cards = socket.recv_json(flags=zmq.NOBLOCK)
    cards = [Card(**cards[k]) for k in CARD_RANGE[0]]
    if dummy_cards is None:
        dummy_cards = cards
    assert(cards == dummy_cards)

for socket in sockets:
    socket.close()

zmqctx.destroy()
