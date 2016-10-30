"""Configuration utilities for the bridge frontend

This module is responsible for generating and loading configurations from a
configuration file.

Functions:
getIdentity -- get identity for the player
"""

import logging
import uuid

def _create_identity():
    return uuid.uuid4()

def _get_identity_from_file(filename):
    try:
        with open(filename) as f:
            identity = f.readline(40)  # Length of UUID + some slack
        return uuid.UUID(identity.strip())
    except FileNotFoundError:
        try:
            with open(filename, 'w') as f:
                identity = _create_identity()
                print(identity, file=f)
            return identity
        except Exception as e:
            logging.warning(
                "Failed to write config to %s: %s", filename, str(e))
    except Exception as e:
        logging.warning(
            "Failed to load config from %s: %s", filename, str(e))
    return _create_identity()


def getIdentity(filename=None):
    """Get the player identity from the given file

    If the file given as argument does not exist, it will be created with new
    identity (an UUID). Otherwise the identity from the file will be used. The
    representation returned from this function is a Python UUID object.

    If the given identity is None, the function returns a new identity not
    stored in any file.
    """
    if filename:
        return _get_identity_from_file(filename)
    else:
        return _create_identity()
