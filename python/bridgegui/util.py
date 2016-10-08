"""Utilities for bridge frontend

This module contains miscellaneous utilities that are too small by themselves to
have a module of their own.
"""

def freeze(d):
    """Freeze object

    This object recursively freezes its (possibly) mutable argument consisting
    of nested lists, tuples, sets and dictionaries. Any object not belonging to
    the aforementioned types (including any container) must be hashable. The
    result is a hashable object that can be used e.g. as dictionary key.

    Not all distinct objects are distinct after being frozen. List freezes into
    tuple, set freezes into frozenset and dict freezes into frozenset containing
    tuples of key–value pairs.
    """
    if isinstance(d, (list, tuple)):
        return tuple(freeze(d2) for d2 in d)
    elif isinstance(d, set):
        return frozenset(freeze(d2) for d2 in d)
    elif isinstance(d, dict):
        return frozenset(freeze(d2) for d2 in d.items())
    else:
        return d
