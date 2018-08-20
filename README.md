# Bridge

[Contract bridge](https://en.wikipedia.org/wiki/Contract_bridge) is a
well‐known trick‐taking card game where two partnerships compete each other.

The goal of this project is to produce a lightweight peer‐to‐peer computer
bridge application for social games.

## Features

- GUI for playing social bridge games
- Multiple models of networking: client‐server and peer‐to‐peer
- Cards can be exchanged between peers by mental card game protocol preventing
  cheating (although see section Security for disclaimers)
- Duplicate scoring (but no support for duplicate deals yet…)

## Installing

[CMake](https://cmake.org/) is currently the only supported build system for
the project.

Bridge is written in C++ (backend) and Python (GUI). The backend does all
actual game logic and communicates with the frontend and other peers using TCP
sockets.

The backend needs modern‐ish C++ compiler supporting C++17. The project depends
on
- [ZeroMQ](http://zeromq.org/) for messaging
- [json](https://github.com/nlohmann/json) to serialize and deserialize messages
- [Lua](https://www.lua.org/) for configuration parsing
- [Boost](http://www.boost.org/) for various things.

Optionally [LibTMCG](http://www.nongnu.org/libtmcg/) is used for secure card
exchange.

[Googletest](https://github.com/google/googletest) is used to build unit
tests. As recommended by the maintainers of the project, instead of relying on
any version of googletest found on the local computer, it is downloaded when
required.

To build and run unit tests for the backend

    $ mkdir /the/build/directory
    $ cd /the/build/directory
    $ cmake /the/source/directory
    $ make
    $ make test
    $ make install

## Bridge GUI

Note! I’m in the process of migrating command line arguments into config file
configurations. Inconsistencies may exist.

This repository only contains code for the backend. In order to play bridge,
you’ll also need a frontend. A GUI written in Python can be found in my other
repository at https://github.com/jasujm/bridgegui.

The GUI depends on
[PyQt5](https://www.riverbankcomputing.com/software/pyqt/download5) to provide
the GUI framework and [PyZQM](https://github.com/zeromq/pyzmq) for messaging.

## Usage

### Backend

Run the backend (server):

    $ bridge --config=config‐file --positions=positions    \
    >   --connect=peer‐endpoints                           \
    >   --cs-cntl=cardserver‐control‐endpoint              \
    >   --cs-peer=cardserver‐base‐peer‐endpoint

The backend opens two sockets into two consequtive ports (by default 5555 and
5556 unless otherwise configured). The first one is used to receive commands
from the frontend and peers. The second one is for publishing events to the
clients.

The options are:

    config     A lua script used to configure the backend. There is a sample
               config file in sample/config.lua of the repository with comments.
    positions  JSON list containing positions the backend instance controls.
               E.g. ["north"]. If omitted, all positions are controlled.
    connect    JSON list containing the control endpoints of the peers. E.g.
               ["tcp://peer1.example.com:5555", "tcp://peer2.example.com:5555"].
               If omitted, the game is peerless.
    cs-cntl    Card server control endpoint.
    cs-peer    Base endpoint for peer card servers.

`positions` option is ignored unless `connect` option is also specified. When
connecting peers to each other, no position can be controlled by multiple peers
and each position must be controlled by some peer. If the game is peerless (and
thus the backend acts purely as server), the single backend always controls all
players. See the Examples section for more information.

If `cs-cntl` and `cs-peer` options are present, card server is used for secure
card exchange between the peers. `cs-cntl` is an endpoint reserved for the
communication between the bridge backend and it’s card server. `cs-peer` is the
(first) endpoint reserved for communication between the card server instances of
each peer. See the Card server section for more information. If the two `cs`
options are not present, plaintext simple card exchange protocol is used, so no
peeking the network traffic :innocent:

To increase logging level, -v and -vv flags can be used for INFO and DEBUG level
logging, respectively.

A single backend application can host multiple games identified by UUID. In
peerless backend, no games initially exist and must be created by the client.
If instructed to run in peer mode by providing the `connect` option, the backend
automatically configures a game based on the command line arguments, identified
by null UUID.

### Frontend

Run the four frontend (client) instances

    $ bridgegui [--game UUID] [--create-game] endpoint

where endpoint is the control endpoint of the backend application. Run
`bridgegui --help` for the remaining options. Exactly one frontend must connect
for each position the backend controls. The backend automatically assigns
positions in order the frontends connect.

The client handles creating and joining a game based on it’s command line
arguments. See the Examples section or use `bridgegui --help`.

### Examples

By using different command line arguments in the backend application, different
kinds of network topologies can be made. In the pure client‐server model one
backend controls all positions and all frontends connect to it.

    server@example.com$ bridge &

    client1@example.com$ bridgegui --create-game tcp://example.com:5555 &
    client234@example.com$ for n in {2..4}; do
    >   bridgegui tcp://example.com:5555 &
    > done

One of the clients must create a game. Games are identified by UUID, but the
UUID can be omitted to allow the server to pick a random one.

In pure peer‐to‐peer model each player has their own instance of backend
application and (presumably) local frontend connecting to it.

    peer@example.com$ bridge --positions='["north"]'        \
    >     --connect='["tcp://peer1.example.com:5555",…]' &
    peer@example.com$ bridgegui tcp://localhost:5555

The peers automatically set up a game identified by null UUID. In this case no
client should create a game (it would be a new game unrelated to the one the
peers are setting up).

Note! The application does not yet correctly handle peers leaving and rejoining
the game. If a peer or a card server crashes, the session is lost.

### Frontend display

Score sheet is displayed on the right. More rows are added to the sheet after
every deal.

Players and their cards are shown in the middle of the screen. The player whose
position is bolded has turn to call (during auction) or play a card to the trick
(during playing).

### Call

Bidding for the current deal is displayed on the left. Calls are made pushing
the buttons and the call sequence is shown in the table underneath. Enabled
buttons correspond to the allowed calls.

### Play

Cards for all the players are displayed in the middle area. During the playing
phase cards are played by clicking the card. Allowed cards are opaque and cards
that cannot be played to the current trick are slightly transparent.

## Card server

If [LibTMCG](http://www.nongnu.org/libtmcg/) is found in the system, the card
server is built in addition to the bridge application. Card server can be used
to execute secure mental card game protocol (ensuring that peers cannot know the
cards of the other peers except when all peers cooperate within the normal laws
of contract bridge). Small Python program used to test the card server is run as
part of the ctest suite.

In order to the use the card server with the bridge application, each peer must
supply the `cs-cntl` and `cs-peer` options when starting the backend
application. The card server is started, giving the same endpoints as
arguments.

The `cs-cntl` endpoint is reserved for communication between the backend and
it’s card served, and should not be visible to other peers. The `cs-peer`
endpoints are reserved for communication _between_ card server instances, and
must be accessible to other peers.

    peer@example.com$ bridge --positions='["north"]'                           \
    >     --connect='["tcp://peer1.example.com:5555",…]'                       \
    >     --cs-cntl=tcp://127.0.0.1:5560 --cs-peer=tcp://*:5565 &
    peer@example.com$ bridgecs tcp://127.0.0.1:5560 tcp://*:5565 &
    peer@example.com$ bridgegui tcp://localhost:5555

In the example above, the local TCP port 5560 is reserved for controlling the
card server. Public TCP ports 5565–5567 (assuming three peers) are reserved for
communicating with remote card server peers.

## Building documentation

[Doxygen](http://www.stack.nl/~dimitri/doxygen/) is used to generate the
documentation for the C++ code. The documentation is generated by making the doc
target

    $ cd /the/build/directory
    $ make doc

## Security

**Note! This feature is still experimental.**

The communication between nodes can be authenticated and encrypted using CURVE
mechanism implemented in the ZeroMQ library (http://curvezmq.org/).

To start the backend with CURVE support, load a configuration file containing
CURVE keys. There is a sample file in the repository.

    $ export BRIDGE_USE_CURVE=1
    $ bridge --config=sample/config.lua …rest of the args…

The GUI needs to configure the public key of the server. It can be extracted
from the server configuration script.

    $ export BRIDGE_USE_CURVE=1
    $ lua5.3 -e 'dofile("sample/config.lua"); print(curve_public_key)' |      \
    >     bridgegui --server-key-file=- …rest of the args…

The card server acts both as server and client, and needs both keys

    $ export BRIDGE_USE_CURVE=1
    $ lua5.3 -e 'dofile("sample/config.lua"); print(curve_public_key); print(curve_public_key)' | \
    >     bridgecs --secret-key-file=- --public-key-file=- …rest of the args…

Note! Currently all peers and card servers must share the same keypair. When
conneting to other peers, they authenticate “their” public key.

The backend performs no authentication of the frontend instances connecting to
it. Clients authenticate the server.

Please see LibTMCG documentation for further information about it’s security
model and assumptions, in particular the honest‐but‐curious security model.

## TODO

Obviously the application is never complete. Because this is hobby project I do
to learn about network software — and because I’m an engineer and not a normal
person — the interesting network stuff comes first and the (in comparison)
boring UX stuff comes later. In approximate order of importance the next goals
for this project are:

- Security for public networks (encryption/authentication based on the
  [CurveZMQ](http://curvezmq.org/) implemented in the ZeroMQ library)
- More user friendly installation and configuration of the application
- Persistent state for the backend (persistent sessions, keep records etc.)
- Nicer user interface and other usability oriented features (claiming tricks,
  canceling moves etc.)
- Peer discovery

## Copyright

Copyright © 2015–2018 Jaakko Moisio <jaakko@moisio.fi>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.
