"""Utilities for handling positions

This module contains functions that can be used to manipulate player positions
and partnerships. In particular it offers utilities to convert between
serialized and internal representations.

The internal position representation is Position enumeration. This enumeration
can direcly be used to index the POSITION_TAGS tuple providing the serialized
representation of the positions as specified by the bridge protocol
specification.

The internal partnership representation is Partnership enumeration. This
enumeration can direcly be used to index the PARTNERSHIP_TAGS tuple providing
the serialized representation of the partnerships as specified by the bridge
protocol specification

Functions:
asPartnership    -- convert serialized position into Position enumeration
positionLabel    -- return human readable label for given position
partner          -- return the partner of given position
rotate           -- return a list of positions starting from given position
asPartnership    -- convert serialized partnership into Partnership enumeration
partnershipLabel -- return human readable label for given partnership
partnershipFor   -- determine partnership for position
"""

import enum

import bridgegui.messaging as messaging

NORTH_TAG = "north"
EAST_TAG = "east"
SOUTH_TAG = "south"
WEST_TAG = "west"
NORTH_SOUTH_TAG = "northSouth"
EAST_WEST_TAG = "eastWest"

POSITION_TAGS = (NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG)
PARTNERSHIP_TAGS = (NORTH_SOUTH_TAG, EAST_WEST_TAG)

class Position(enum.IntEnum):
    """Position enumeration"""
    north = 0
    east = 1
    south = 2
    west = 3

class Partnership(enum.IntEnum):
    """Partnership enumeration"""
    northSouth = 0
    eastWest = 1

# TODO: Localization
POSITION_FORMATS = {
    Position.north: "North", Position.east: "East",
    Position.south: "South", Position.west: "West"
}
PARTNERSHIP_FORMATS = {
    Partnership.northSouth: "North South", Partnership.eastWest: "East West"
}

POSITION_PARTNERSHIPS = {
    Position.north: Partnership.northSouth,
    Position.east: Partnership.eastWest,
    Position.south: Partnership.northSouth,
    Position.west: Partnership.eastWest
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


def positionLabel(position):
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


def asPartnership(partnership):
    """Convert serialized partnership representation into Partnership enumeration

    The serialized representation is one of the strings in PARTNERSHIP_TAGS
    tuple. This function converts a string into the corresponding Partnership
    enumeration. The function is idempotent, meaning that it is safe to call it
    also with Position enum.

    Keyword Arguments:
    partnership -- the partnership to convert
    """
    if isinstance(partnership, Partnership):
        return partnership
    try:
        return Partnership(PARTNERSHIP_TAGS.index(partnership))
    except Exception:
        raise messaging.ProtocolError("Invalid partnership: %r" % partnership)


def partnershipLabel(partnership):
    """Return human readable label for given partnership"""
    return PARTNERSHIP_FORMATS[asPartnership(partnership)]


def partnershipFor(position):
    """Return partnership for given position"""
    return POSITION_PARTNERSHIPS[asPosition(position)]
