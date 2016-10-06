# Bridge

[Contract bridge](https://en.wikipedia.org/wiki/Contract_bridge) is a
well‐known trick‐taking card game where two partnerships compete each other.

The goal of this project is to produce a lightweight peer‐to‐peer computer
bridge application for social games.

## Features

- GUI for playing social bridge games
- Multiple models of networking: client‐server and peer‐to‐peer
- Cards can be echanged between peers by secure protocol preventing cheating
- Duplicate scoring (but no support for duplicate deals)

## Installing

[CMake](https://cmake.org/) is currently the only supported build system for
the project.

Bridge is written in C++ (backend) and Python (GUI). The backend does all
actual game logic and communicates with the frontend and other peers using TCP
sockets.

The backend uses many features from C++14 — deliberately as one of the goals
of this project was to learn C++14 — so you need modern‐ish compiler. The
project depends on [ZeroMQ](http://zeromq.org/) for messaging,
[json](https://github.com/nlohmann/json) to serialize and deserialize messages
and [Boost](http://www.boost.org/) for various things. Optionally
[LibTMCG](http://www.nongnu.org/libtmcg/) is used for secure card exchange.

[Googletest](https://github.com/google/googletest) is used to build unit
tests. As recommended by the maintainers of the project, instead of relying on
any version of googletest found on the local computer, it is downloaded when
required.

The GUI depends on [Kivy](https://kivy.org/) as GUI toolkit and
[PyZQM](https://github.com/zeromq/pyzmq) for messaging.

To build and run unit tests for the backend

    mkdir /the/build/directory
    cd /the/build/directory
    cmake /the/source/directory
    make
    make test
    make install

## Usage

Run the backend (server) located in the top level build directory

    $ bridge --bind=endpoint --positions=positions \
    >   --connect=peer‐endpoints --cs-cntl=cardserver‐control‐endpoint  \
    >   --cs-peer=cardserver‐base‐peer‐endpoint

The backend opens two sockets into two consequtive ports. The first one is
used to receive commands from the frontend and peers. The second one is for
publishing events to the clients.

Arguments to the options are:

    bind       The endpoint for control socket. The endpoint for event socket
               has the same interface but one greater port, i.e. if the
               endpoint for control socket is tcp://*:5555, the endpoint
               for event socket is tcp://*:5556.
    positions  JSON list containing positions the backend instance controls.
               E.g. ["north"]. If omitted, all positions are controlled.
    connect    JSON list containing ZMQ endpoints for the control sockects of
               the peers the backend connects to. E.g.
               ["tcp://peer1.example.com:5555", "tcp://peer2.example.com:5555"].
               If omitted, the backend connects to no peers.
    cs-cntl    Card server control endpoint.
    cs-peer    Base endpoint for peer card servers.

When connecting peers to each other, no position can be controlled by two
peers and each position must be controlled by some peer.

If the last two parameters are present, card server is used for secure card
exchange between the peers. Otherwise plaintext simple card exchange protocol
is used, so no peeking the network traffic :innocent:

Run the four frontend (client) instances

    python /the/source/directory/python/bridge_gui.py endpoint

where endpoint is the control endpoint of the backend application. Exactly one
frontend must connect for each position the backend controls. The backend
automatically assigns positions in order the frontends connect.

By adjusting the positions and peers arguments in the backend application,
different kinds of network topologies can be made. In the pure client‐server
model one backend controls all positions and all frontends connect to it.

    $ bridge --bind=tcp://*:5555
    
    $ for n in {1..4}; do
    >   /the/source/directory/python/bridge_gui.py tcp://example.com:5555 &
    > done

In pure peer‐to‐peer model each player has their own instance of backend
application and (presumably) local frontend connecting to it.

    $ bridge --bind=tcp://*:5555 --positions='["north"]' \
    >   --connect='["tcp://peer1.example.com:5555",…]' &
    $ /the/source/directory/python/bridge_gui.py tcp://localhost:5555

Players and their cards are shown in the middle of the screen. The player
whose position is bolded has turn to call (during auction) or play a card to
the trick (during playing). Only the cards of the player in turn and (during
the playing phase) the dummy are shown at any time.

Note! The application does not yet correctly handle peers and clients leaving
and rejoining the game. If connection is lost, the player is never reassigned.

### Score

Score sheet is displayed on the right. More rows are added to the sheet after
every deal.

### Call

Bidding for the current deal is displayed on the right. Calls are made pushing
the buttons and the call sequence is shown in the table underneath. Enabled
buttons correspond to the allowed cals.

### Play

Cards for all the players are displayed in the middle as buttons. During the
playing phase cards are played by pushing the buttons. Enabled buttons
correspond to the cards that can be played to the current trick.

## Card server

If [LibTMCG](http://www.nongnu.org/libtmcg/) is found in the system, the card
server is built in addition to the bridge application. When integrated with
the bridge application, card server is responsible for executing the actual
mental card game protocol. Small Python program used to test the card server
is run as part of the ctest suite.

In order to use card server with bridge application, each peer must supply
cs-cntl and cs-peer arguments when starting the backend application. The card
server is started with the same arguments. The control endpoint does needs to
be visible only to the backend. The peer endpoints must be accessible to the
peers.

    $ bridge --bind=tcp://*:5555 --positions='["north"]' \
    >   --connect='["tcp://peer1.example.com:5555",…]'                        \
    >   --cs-cntl=tcp://127.0.0.1:5560 --cs-peer=tcp://*:5565 &
    $ bridge-cs tcp://127.0.0.1:5560 tcp://*:5565 &
    $ /the/source/directory/python/bridge_gui.py tcp://localhost:5555

One port is reserved for each peer. In the example above that means ports
5565–5567 assuming three peers.

## Building documentation

[Doxygen](http://www.stack.nl/~dimitri/doxygen/) is used to generate
documentation for the C++ code. The documentation is generated by making the doc
target

    cd /the/build/directory
    make doc

## TODO

Obviously the application is never complete. Because this is hobby project and
I’m interested in implementing a peer protocol, the interesting networking
stuff is up first, and the usability stuff comes after that.

- More user friendly configuring and installation
- Actually usable GUI
- Replace the cumbersome command line configurations with nicer mechanism
- Peer discovery
- Recovery from more serious failures (lost messages, crash)

## Copyright

Copyright © 2015–2016 Jaakko Moisio <jaakko@moisio.fi>

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
