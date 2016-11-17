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

The GUI depends
on [PyQt5](https://www.riverbankcomputing.com/software/pyqt/download5)
to provide the GUI framework
and [PyZQM](https://github.com/zeromq/pyzmq) for messaging.

To build and run unit tests for the backend

    $ mkdir /the/build/directory
    $ cd /the/build/directory
    $ cmake /the/source/directory
    $ make
    $ make test
    $ make install

## Usage

Run the backend (server):

    $ bridge --positions=positions --connect=peer‐endpoints   \
    >   --cs-cntl=cardserver‐control‐endpoint                 \
    >   --cs-peer=cardserver‐base‐peer‐endpoint endpoint

The backend opens two sockets into two consequtive ports. The first one is
used to receive commands from the frontend and peers. The second one is for
publishing events to the clients.

The options are:

    endpoint   The endpoint for control socket. The endpoint for event socket
               has the same interface but one greater port, i.e. if the
               endpoint for control socket is tcp://*:5555, the endpoint
               for event socket is tcp://*:5556.
    positions  JSON list containing positions the backend instance controls.
               E.g. ["north"]. If omitted, all positions are controlled.
    connect    JSON list containing ZMQ endpoints of the control sockects of
               the peers the backend connects to. E.g.
               ["tcp://peer1.example.com:5555", "tcp://peer2.example.com:5555"].
               If omitted, the backend connects to no peers.
    cs-cntl    Card server control endpoint.
    cs-peer    Base endpoint for peer card servers.

Positions option is only significant if connect option is specified. When
connecting peers to each other, no position can be controlled by two peers and
each position must be controlled by some peer. If there are no peers (and thus
the backend acts purely as server), it always controls all players. See the
Examples section for more information.

If the last two parameters are present, card server is used for secure card
exchange between the peers. See the Card server section for more information. If
they are not present, plaintext simple card exchange protocol is used, so no
peeking the network traffic :innocent:

To increase logging level, -v and -vv flags can be used for INFO and DEBUG level
logging, respectively.

Run the four frontend (client) instances

    $ bridgegui --config=bridge-config --position=position endpoint

where endpoint is the control endpoint of the backend application. Exactly one
frontend must connect for each position the backend controls. The backend
automatically assigns positions in order the frontends connect.

The configuration file specified with the --config option is optional but
useful. It remembers the frontend to remember its identity. If the client quits
(or worse, crashes), the old session can be recovered by specifying the same
configuration file and backend. The file can be any file to which the used has
write access.

The optional position argument is one of the positions (north, east, south,
west). If provided, it is the preferred position requested for the backend. If
the position is already taken (or it is controlled by another peer), the backend
assigns any position.

### Examples

By adjusting the positions and peers arguments in the backend application,
different kinds of network topologies can be made. In the pure client‐server
model one backend controls all positions and all frontends connect to it.

    $ bridge tcp://*:5555
    $ for n in {1..4}; do
    >   bridgegui tcp://example.com:5555 &
    > done

In pure peer‐to‐peer model each player has their own instance of backend
application and (presumably) local frontend connecting to it.

    $ bridge --positions='["north"]'                                      \
    >     --connect='["tcp://peer1.example.com:5555",…]' tcp://*:5555 &
    $ bridgegui tcp://localhost:5555

Players and their cards are shown in the middle of the screen. The player whose
position is bolded has turn to call (during auction) or play a card to the trick
(during playing).

Note! The application does not yet correctly handle peers leaving and rejoining
the game. If a peer or a card server crashes, the session is lost.

### Score

Score sheet is displayed on the right. More rows are added to the sheet after
every deal.

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
supply the cs-cntl and cs-peer options when starting the backend application.
The card server is started, giving the same endpoints as arguments. The control
endpoint does needs to be visible only to the backend. The peer endpoints must
be accessible to the peers.

    $ bridge --positions='["north"]'                                           \
    >     --connect='["tcp://peer1.example.com:5555",…]'                       \
    >     --cs-cntl=tcp://127.0.0.1:5560 --cs-peer=tcp://*:5565 tcp://*:5555 &
    $ bridgecs tcp://127.0.0.1:5560 tcp://*:5565 &
    $ bridgegui tcp://localhost:5555

One port is reserved for each peer. In the example above that means ports
5565–5567 assuming three peers.

## Building documentation

[Doxygen](http://www.stack.nl/~dimitri/doxygen/) is used to generate the
documentation for the C++ code. The documentation is generated by making the doc
target

    $ cd /the/build/directory
    $ make doc

## TODO

Obviously the application is never complete. Because this is hobby project I do
to learn about network software — and because I‘m an engineer and not a normal
person — the interesting network stuff comes first and the (in comparison)
boring UX stuff comes later. In approximate order of importance the next goals
for this project are:

- Backend to host more games and similar features one would expect from server
  software
- Peer discovery
- More user friendly configuration and installation
  - This includes replacing the cumbersome command line configurations with
    nicer mechanism
- Nicer GUI
- Persistent state for the backend (recover sessions after crash/restart, keep
  records etc.)

## Acknowledgments

Byron Knoll is acknowledged for the playing card graphics:
<http://byronknoll.blogspot.com/2011/03/vector-playing-cards.html>

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
