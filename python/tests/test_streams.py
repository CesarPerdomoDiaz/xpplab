"""Loading and closing models must not leave C streams open.

Each model loads its own copy of the engine library, and all copies share
the process's C runtime. A stream a copy leaves open when it is unloaded
stays taken in that runtime: about 500 loads in one process once used up
the 512 streams of the Windows runtime, and the next fopen() killed the
process (the full test suite died in whichever test came next).
"""
import ctypes
import os
import sys

import pytest

import xpplab as xs

TEXT = "x'=-x\ny'=-y\ninit x=1,y=1\n@ total=5\ndone\n"


def open_streams():
    """A count that grows by one for each C stream left open: free stream
    slots (negated) on Windows, open file descriptors elsewhere."""
    if sys.platform == "win32":
        crt = ctypes.CDLL("ucrtbase")
        crt.fopen.restype = ctypes.c_void_p
        crt.fopen.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        crt.fclose.argtypes = [ctypes.c_void_p]
        got = []
        while True:
            f = crt.fopen(b"NUL", b"r")
            if not f:
                break
            got.append(f)
        for f in got:
            crt.fclose(f)
        return -len(got)
    for d in ("/proc/self/fd", "/dev/fd"):
        if os.path.isdir(d):
            return len(os.listdir(d))
    pytest.skip("no way to count open files here")


def test_closed_models_leave_no_stream_open():
    with xs.Model.from_text(TEXT) as m:   # warm up: imports, first-use state
        m.run()
    before = open_streams()
    for _ in range(20):
        with xs.Model.from_text(TEXT) as m:
            m.run()
    assert open_streams() - before <= 0
