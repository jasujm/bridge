# Bridge

[Contract bridge](https://en.wikipedia.org/wiki/Contract_bridge) is a
well-known trick-taking card game where two partnerships compete each other.

The goal of this project is to produce a lightweight computer bridge
application for social games. Currently it is rather limited. It is a hobby
project and will see improvements eventually. :wink:

## Features

Currently you can only do few things with the application

- Only command-line interface with verbose syntax
- All four players are controlled locally…
- …and the cards are visible all the time
- Duplicate scoring

## Installing

[CMake](https://cmake.org/) is currently the only supported build system for
the project.

Bridge uses many features from C++14 — deliberately as one of the goals of
this project was to learn C++14 — so you need modern-ish compiler. The only
external dependency for the project is [Boost](http://www.boost.org/).

[Googletest](https://github.com/google/googletest) is used to build unit
tests. As recommended by the maintainers of the project, instead of relying on
any version of googletest found on the local computer, it is downloaded when
required.

To build and run unit tests

    mkdir /the/build/directory
    cd /the/build/directory
    cmake -D BRIDGE_BUILD_TESTS=ON /the/source/directory
    make
    ctest

## Usage

Run the application located in the top level build directory

    /the/build/directory/bridge

You will see information about the current deal. More becomes available as the
deal progresses. There are three commands:

### Score

Display scoresheet

    score

### Call

Make a call during the auction. The call type (pass, bid, double, redouble) is
named after the call command. A bid is specified after the bid command by
naming the level (number between 1–7) and the strain (clubs, diamonds, hearts,
spades, notrump).

    call pass
    call bid 1 clubs
    call bid 7 notrump
    call double
    call redouble

### Play

Play a card during the playing phase. The card is specified by naming the rank
and the suit (clubs, diamonds, hearts, spades) separated by whitespace. The
rank of non-honors is a number between 2–10, and the rank of honor is either
jack, queen, king or ace.

    play 2 clubs
    play ace spades

## TODO

Obviously the application isn’t very nice yet. My goal is to make it a
lightweight bridge game that supports playing over network. Instead of
centralized server I plan to use mental card game library to implement
peer-to-peer communication.

Short term goals:

- Nice GUI
  - Ideally the GUI is implemented in separate process communicating with the
    game using e.g. [ZeroMQ](http://zeromq.org/)
- Networking
  - Asynchronous IO. Use ZeroMQ as message queue both internally and
    externally?
  - In the first phase I’ll use some simple plain text protocol for
    non-serious use

Long term goals:

- Integrating mental card game library for more serious use
  (e.g. [LibTMCG](http://www.nongnu.org/libtmcg/))

## Copyright

Copyright © 2015 Jaakko Moisio <jaakko@moisio.fi>

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
