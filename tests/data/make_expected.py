# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

"""Dump what the x3pio reference implementation reads from the fixtures.

Run it with an interpreter that has x3pio installed:
``python make_expected.py > expected.txt`` for the fixtures of this folder, or
``python make_expected.py FOLDER > FOLDER/expected.txt`` for any other folder,
such as one of larger files that the tests use when ``X3PIO_LOCAL_SAMPLES``
names it. The C tests read the
same files with libx3pio and compare, so the two implementations check each
other. One line per file, the fields separated by tabs:

    path feature layers shape valid sum_x sum_y sum_z z_first z_last

``shape`` is the shape of the stored points (dimensions of size one dropped),
``valid`` the number of points that are not missing, the sums are over the
global coordinates of the valid points, ``z_first`` and ``z_last`` the global
z of the first and the last valid point in storage order.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import x3pio

FOLDER = Path(__file__).parent


def main() -> None:
    folder = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else FOLDER
    for path in sorted(folder.rglob("*.x3p")):
        file = x3pio.read(str(path), verify=True)
        points = np.concatenate([layer.points().reshape(-1, 3) for layer in file.layers])
        valid = np.isfinite(points).all(axis=1)
        good = points[valid]
        shape = "x".join(str(n) for n in file.layers[0].z.shape if n != 1) or "1"
        feature = type(file).__name__.lower()
        relative = path.relative_to(folder).as_posix()
        print(
            relative,
            feature,
            len(file.layers),
            shape,
            int(valid.sum()),
            repr(float(good[:, 0].sum())),
            repr(float(good[:, 1].sum())),
            repr(float(good[:, 2].sum())),
            repr(float(good[0, 2])),
            repr(float(good[-1, 2])),
            sep="\t",
        )


if __name__ == "__main__":
    sys.exit(main())
