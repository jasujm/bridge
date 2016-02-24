# Bridge

[Contract bridge](https://en.wikipedia.org/wiki/Contract_bridge) is a
well-known trick-taking card game where two partnerships compete each other.

The goal of this project is to produce a lightweight computer bridge
application for social games. Currently it is rather limited. It is a hobby
project and will see improvements eventually. :wink:

## Features

Currently you can only do few things with the application

- Run backend application and simple GUI
- All four players are controlled locally…
- …and the cards are visible all the time
- Duplicate scoring

## Installing

[CMake](https://cmake.org/) is currently the only supported build system for
the project.

Bridge is written in C++ (backend) and Python (GUI). The backend does all
actual game logic and communicates with the frontend using TCP sockets.

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

Run the backend located in the top level build directory

    /the/build/directory/bridge

Run the GUI

    python /the/source/directory/python/bridge_gui.py

You can run the backend and the GUI in any order. Note, however, that the
frontend blocks before both are running. Backend runs until interrupted. The
GUI can connect to the backend at any point. In fact, there can be multiple
GUIs connected to the same backend (they all then share and control the same
state). Currently the backend opens fixed ports so multiple backends cannot be
run at the same time.

There are three parts in the screen:

### Score

Scoresheet is displayed on the right. More rows are added to the sheet after
every deal.

### Call

Bidding for the current deal is displayed on the right. Calls are made using
the buttons and the bidding is shown in the table underneath.

### Play

Cards for all the players are displayed in the middle. Enabled buttons
correspond to cards currently held by the player. After the auction cards are
played by pushing the buttons.

## TODO

Obviously the application isn’t very nice yet. My goal is to make it a
lightweight bridge game that supports playing over network. Instead of
centralized server I plan to use mental card game library to implement
peer-to-peer communication.

Short term goals:

- Networking
  - Use ZeroMQ to send and receive messages between different Bridge
    application instances
  - In the first phase I’ll use some simple plain text protocol for
    non-serious use

Long term goals:

- Actually usable GUI
- Integrating mental card game library for more serious use
  (e.g. [LibTMCG](http://www.nongnu.org/libtmcg/))

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
