# Bridge

[Contract bridge](https://en.wikipedia.org/wiki/Contract_bridge) is a
well-known trick-taking card game where two partnerships compete each other.

The goal of this project is to produce a lightweight peer‐to‐peer computer
bridge application for social games.

## Features

Currently you can only do few things with the application

- One or several backend peer applications can be started and connected to
  each other (no peer discovery yet)
- The protocol between the peers is as of now cleartext, so no peeking the
  network traffic to figure out what cards the other peers are dealt :innocent:
- Frontend client applications can be connected to backends
- After that you are ready to play bridge with duplicate scoring

## Installing

[CMake](https://cmake.org/) is currently the only supported build system for
the project.

Bridge is written in C++ (backend) and Python (GUI). The backend does all
actual game logic and communicates with the frontend and other peers using TCP
sockets.

The backend uses many features from C++14 — deliberately as one of the goals
of this project was to learn C++14 — so you need modern-ish compiler. The
project depends on [ZeroMQ](http://zeromq.org/) for interprocess
communication, [json](https://github.com/nlohmann/json) to serialize and
deserialize messages and [Boost](http://www.boost.org/) for various things.

[Googletest](https://github.com/google/googletest) is used to build unit
tests. As recommended by the maintainers of the project, instead of relying on
any version of googletest found on the local computer, it is downloaded when
required.

The GUI depends on [Kivy](https://kivy.org/) as GUI toolkit and
[PyZQM](https://github.com/zeromq/pyzmq) for communication.

To build and run unit tests for the backend

    mkdir /the/build/directory
    cd /the/build/directory
    cmake -D BRIDGE_BUILD_TESTS=ON /the/source/directory
    make
    ctest

## Usage

Run the backend (server) located in the top level build directory

    /the/build/directory/bridge control event positions peers

Arguments:

    control    The ZMQ endpoint the control socket binds to. The control socket
               is used to receive commands from the frontend and other peers.
               E.g. tcp://*:5555
    event      The ZMQ endpoint the event socket binds to. The event socket is
               used to notify events to the frontends. E.g. tcp://*.5556
    positions  JSON list containing positions the backend instance controls.
               E.g. ["north"]
    peers      JSON list containing ZMQ endpoints for the control sockects of
               the peers the backend connects to. E.g.
               ["tcp://peer1.example.com:5555", "tcp://peer2.example.com:5555"]

When connecting peers to each other, no position can be controlled by two
peers and each position must be controlled by some peer.

Run the four frontend (client) instances

    python /the/source/directory/python/bridge_gui.py control event

where control and event refer to the control and event endpoints of the
backend the client is connected to. Exactly one frontend needs to be
controlled for each position the backend controls. The backend automatically
assigns positions in order the frontends connect.

By adjusting the positions and peers arguments in the backend application,
different kinds of network topologies can be made. In the pure client‐server
model one backend controls all positions and all frontends connect to it.

    $ /the/build/directory/bridge tcp://*:5555 tcp://*:5556 \
    > '["north","east","south","west"]' '[]'
    
    $ for n in {1..4}; do
    > /the/source/directory/python/bridge_gui.py tcp://server.example.com:5555 \
    > tcp://server.example.com:5556 &
    > done

In pure peer‐to‐peer model each player has their own instance of backend
application and (presumably) local frontend connecting to it.

    $ /the/build/directory/bridge tcp://*5555 tcp://127.0.0.1:5556 \
    > '["north"]' '["tcp://peer1.example.com:5555",…]' &
    $ /the/source/directory/python/bridge.gui tcp://localhost:5555 \
    > tcp://localhost:5556

One seat is assined to each frontend. Players and their cards are shown in the
middle of the screen. The player whose position is bolded has turn to call
(during auction) or play a card to the trick (during playing). Only the cards
of the player in turn and (during the playing phase) the dummy are shown at
any time.

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

If [LibTMCG](http://www.nongnu.org/libtmcg/) is found in the system,
cardserver is built in addition to the bridge application. When integrated
with the bridge application, card server is responsible for executing the
actual mental card game protocol. Small Python program can be used to test the
card server intercation with peers.

    $ cd /the/build/directory
    $ python ../python/test_card_server.py

## TODO

Obviously the application isn’t complete yet. Because this is hobby project
and I’m interested in implementing a peer protocol, the interesting networking
stuff is up first, and the usability stuff comes after that.

Short term goals:

- Make bridge application use card server for secure mental card game protocol

Long term goals:

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
