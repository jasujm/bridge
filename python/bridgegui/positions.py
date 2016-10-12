NORTH_TAG = "north"
EAST_TAG = "east"
SOUTH_TAG = "south"
WEST_TAG = "west"

POSITION_TAGS = (NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG)

def partner(position):
    n = POSITION_TAGS.index(position)
    return POSITION_TAGS[(n + 2) % 4]
