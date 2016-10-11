"""Utilities for bridge frontend

This module contains miscellaneous utilities that are too small by themselves to
have a module of their own.
"""

import os

import PyQt5.QtGui


def getImage(filename):
    """Load image from file

    Given filename as its argument, this function loads image from the file and
    returns a QImage object containing the loaded image. The filename argument
    is prepended with the path to image directory.

    Keyword Arguments:
    filename -- the filename of the image file
    """
    imagepath = os.environ["BRIDGE_IMAGE_PATH"]
    return PyQt5.QtGui.QImage(os.path.join(imagepath, filename))
