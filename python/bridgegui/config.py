"""Configuration utilities for the bridge frontend

This module is responsible for generating and loading configurations from a
configuration file.

Functions:
"""

import logging
import uuid
import os


def getIdentity(filename=None):
    """Get the player identity from the given file

    If the file given as argument does not exist, it will be created with new
    identity (an UUID). Otherwise the identity from the file will be used. The
    representation returned from this function is a Python UUID object.
    """
    if filename and os.path.isfile(filename):
        try:
            with open(filename) as f:
                identity = f.readline().strip()
                return uuid.UUID(identity)
        except Exception:
            logging.warning("Failed to load UUID from %s", filename)
            return uuid.uuid4()
    elif filename:
        identity = uuid.uuid4()
        try:
            with open(filename, 'w') as f:
                print(identity, file=f)
        except Exception:
            logging.warning("Failed to write the identity to %s", filename)
        return identity
    else:
        return uuid.uuid4()
