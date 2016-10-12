"""Utilities for bridge frontend

This module contains miscellaneous utilities that are too small by themselves to
have a module of their own.
"""

import os


def getImage(filename):
    """Load image from file

    Given filename as its argument, this function loads image from the file and
    returns a QImage object containing the loaded image. The filename argument
    is prepended with the path to image directory.

    Keyword Arguments:
    filename -- the filename of the image file
    """
    from PyQt5.QtGui import QImage
    imagepath = os.environ.get("BRIDGE_IMAGE_PATH", None)
    if imagepath:
        return QImage(os.path.join(imagepath, filename))
    else:
        return QImage(50, 100, QImage.Format_Mono)
