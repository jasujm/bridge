"""Utilities for handling positions

This module contains functions that can be used to manipulate player
positions. In particular it offers utilities to convert between serialized and
internal position representation.

The internal position representation is Position enumeration. This enumeration
can direcly be used to index the POSITION_TAGS tuple providing the serialized
representation of the positions as specified by the bridge protocol
specification.

Functions:
label   -- return human readable label for given position
partner -- return the partner of given position
rotate  -- return a list of positions starting from given position
"""

import enum

import bridgegui.messaging as messaging

NORTH_TAG = "north"
EAST_TAG = "east"
SOUTH_TAG = "south"
WEST_TAG = "west"

POSITION_TAGS = (NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG)


class Position(enum.IntEnum):
    """Position enumeration"""
    north = 0
    east = 1
    south = 2
    west = 3

# TODO: Localization
POSITION_FORMATS = {
    Position.north: "North", Position.east: "East",
    Position.south: "South", Position.west: "West"
}


def asPosition(position):
    """Convert serialized position representation into Position enumeration

    The serialized representation is one of the strings in POSITION_TAGS
    tuple. This function converts a string into the corresponding Position
    enumeration. The function is idempotent, meaning that it is safe to call it
    also with Position enum.

    Keyword Arguments:
    position -- the position to convert
    """
    if isinstance(position, Position):
        return position
    try:
        return Position(POSITION_TAGS.index(position))
    except Exception:
        raise messaging.ProtocolError("Invalid position: %r" % position)


def label(position):
    """Return human readable label for given position"""
    return POSITION_FORMATS[asPosition(position)]


def partner(position):
    """Return Position enumeration for the partner of the position"""
    return Position((asPosition(position) + 2) % 4)


def rotate(position):
    """Return list of positions starting from given position

    The list contains all four positions starting from the position given as
    argument and proceeding clockwise. For example if the function is called
    with Position.east as argument, the list produced is

    [Position.east, Position.south, Position.west, Position.north]

    Keyword Arguments:
    position -- the initial position
    """
    position = asPosition(position)
    positions = list(Position)
    return positions[position:] + positions[:position]
