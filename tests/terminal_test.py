#!/usr/bin/env python3
"""Exercise the installed CLI contract and terminal lifecycle through real PTYs."""
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import tempfile
import time
import unittest

BINARY = os.path.abspath(sys.argv.pop(1))


def attributes(fd):
    result = termios.tcgetattr(fd)
    # Darwin sets PENDIN when canonical mode is restored. It is transient
    # kernel input bookkeeping, not an application-controlled mode change.
    result[3] &= ~getattr(termios, "PENDIN", 0)
    return result


class Session:
    def __init__(self, *args, term="xterm-256color", demo=True):
        self.master, self.slave = pty.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
        self.before = attributes(self.slave)
        env = dict(os.environ, TERM=term)
        env.pop("COLORTERM", None)
        env.pop("NO_COLOR", None)
        command = [BINARY, *( ["--demo"] if demo else [] ), *args]
        self.proc = subprocess.Popen(command, stdin=self.slave,
                                     stdout=self.slave, stderr=self.slave, env=env)
        self.output = b""

    def drain(self, seconds=.1):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if select.select([self.master], [], [], max(0, end-time.monotonic()))[0]:
                self.output += os.read(self.master, 65536)

    def finish(self):
        end = time.monotonic() + 5
        while self.proc.poll() is None and time.monotonic() < end:
            self.drain(.03)
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
            raise AssertionError("terminal session did not exit")
        self.drain(.03)
        return self.proc.returncode

    def close(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        os.close(self.master)
        os.close(self.slave)


class TerminalTests(unittest.TestCase):
    def session(self, *args, **kwargs):
        session = Session(*args, **kwargs)
        self.addCleanup(session.close)
        return session

    def restored(self, session):
        self.assertEqual(attributes(session.slave), session.before)
        self.assertIn(b"\x1b[?25h", session.output)
        self.assertIn(b"\x1b[?1049l", session.output)

    def test_cli(self):
        for args in (["--fps", "nan"], ["--size", "0x2"], ["--size", "501x1"],
                     ["--frames", "0"], ["--duration", "inf"], ["--image-strength", "1.1"], ["--mode", "bogus"],
                     ["--unknown"], ["--fps"], ["--seed", "-1"], ["--output", "terminal"]):
            with self.subTest(args=args):
                result = subprocess.run([BINARY, *args], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"bmatrix:", result.stderr)
        result = subprocess.run([BINARY, "--demo", "--size", "10x4"], capture_output=True, check=True)
        self.assertNotIn(b"\x1b", result.stdout)
        self.assertEqual(len(result.stdout.splitlines()), 4)

    def test_finite_and_paced(self):
        start = time.monotonic()
        session = self.session("--frames", "5", "--fps", "10")
        self.assertEqual(session.finish(), 0)
        self.assertGreaterEqual(time.monotonic() - start, .35)
        self.restored(session)

    def test_default_matrix_continues(self):
        session = self.session("--frames", "2", "--fps", "30")
        self.assertEqual(session.finish(), 0)
        # More than one paint is emitted: the default is a living field, not a
        # one-shot image render.
        self.assertGreater(session.output.count(b"\x1b["), 4)
        self.restored(session)

    def test_gallery_next_key(self):
        with tempfile.TemporaryDirectory() as directory:
            for index, color in enumerate((b"\xff\x00\x00", b"\x00\x00\xff"), 1):
                with open(os.path.join(directory, f"{index:02}.ppm"), "wb") as image:
                    image.write(b"P6\n1 1\n255\n" + color)
            session = self.session("--gallery", directory, "--effect", "none", "--mode", "full",
                                   "--color", "truecolor", demo=False)
            session.drain(.15)
            red = b"\x1b[38;2;255;0;0m"
            blue = b"\x1b[38;2;0;0;255m"
            self.assertIn(red, session.output)
            self.assertNotIn(blue, session.output)
            os.write(session.master, b" ")
            session.drain(.1)
            before = len(session.output)
            os.write(session.master, b"n")
            session.drain(.15)
            self.assertIn(blue, session.output[before:])
            self.assertNotIn(red, session.output[before:])
            before = len(session.output)
            os.write(session.master, b"N")
            session.drain(.15)
            self.assertIn(red, session.output[before:])
            self.assertNotIn(blue, session.output[before:])
            before = len(session.output)
            session.drain(.1)
            self.assertEqual(before, len(session.output), "gallery advance preserves pause")
            os.write(session.master, b"q")
            self.assertEqual(session.finish(), 0)
            self.restored(session)

    def test_gallery_loads_only_current_file(self):
        with tempfile.TemporaryDirectory() as directory:
            with open(os.path.join(directory, "01.ppm"), "wb") as image:
                image.write(b"P6\n1 1\n255\n\xff\x00\x00")
            with open(os.path.join(directory, "02.ppm"), "wb") as image:
                image.write(b"invalid image")
            session = self.session("--gallery", directory, "--effect", "none", demo=False)
            session.drain(.15)
            self.assertIsNone(session.proc.poll(), "later gallery files are not decoded eagerly")
            os.write(session.master, b"n")
            self.assertNotEqual(session.finish(), 0)
            self.restored(session)
            self.assertIn(b"bmatrix:", session.output)

    def test_interrupt(self):
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGQUIT):
            with self.subTest(sig=sig):
                session = self.session()
                session.drain(.1)
                self.assertFalse(termios.tcgetattr(session.slave)[3] & termios.ECHO)
                session.proc.send_signal(sig)
                self.assertEqual(session.finish(), 0)
                self.restored(session)

    def test_resize_pause_replay_quit(self):
        session = self.session()
        session.drain(.1)
        os.write(session.master, b" ")
        session.drain(.1)
        before = len(session.output)
        session.drain(.15)
        self.assertEqual(before, len(session.output))
        fcntl.ioctl(session.slave, termios.TIOCSWINSZ, struct.pack("HHHH", 3, 5, 0, 0))
        session.proc.send_signal(signal.SIGWINCH)
        os.write(session.master, b" r")
        session.drain(.15)
        self.assertGreater(len(session.output), before)
        os.write(session.master, b"q")
        self.assertEqual(session.finish(), 0)
        self.restored(session)

    def test_suspend_resume(self):
        session = self.session()
        session.drain(.1)
        session.proc.send_signal(signal.SIGTSTP)
        session.drain(.15)
        self.assertEqual(attributes(session.slave), session.before)
        session.proc.send_signal(signal.SIGCONT)
        session.drain(.15)
        self.assertFalse(termios.tcgetattr(session.slave)[3] & termios.ECHO)
        os.write(session.master, b"q")
        self.assertEqual(session.finish(), 0)
        self.restored(session)

    def test_paused_resize_and_resume_repaint(self):
        session = self.session("--effect", "none", "--mode", "full", "--color", "truecolor")
        session.drain(.15)
        os.write(session.master, b" ")
        session.drain(.1)
        fcntl.ioctl(session.slave, termios.TIOCSWINSZ, struct.pack("HHHH", 12, 40, 0, 0))
        before = len(session.output)
        session.proc.send_signal(signal.SIGWINCH)
        session.drain(.15)
        self.assertIn(b"38;2;", session.output[before:], "paused resize repaints visible cells")
        before = len(session.output)
        session.drain(.1)
        self.assertEqual(before, len(session.output), "resize does not unpause")
        session.proc.send_signal(signal.SIGTSTP)
        session.drain(.15)
        self.assertEqual(attributes(session.slave), session.before)
        before = len(session.output)
        session.proc.send_signal(signal.SIGCONT)
        session.drain(.15)
        self.assertIn(b"38;2;", session.output[before:], "paused resume repaints visible cells")
        before = len(session.output)
        session.drain(.1)
        self.assertEqual(before, len(session.output), "resume does not unpause")
        os.write(session.master, b"q")
        self.assertEqual(session.finish(), 0)
        self.restored(session)

    def test_capabilities(self):
        for term in ("dumb", "bmatrix-invalid-terminal"):
            session = self.session("--frames", "1", term=term)
            self.assertNotEqual(session.finish(), 0)
            self.assertEqual(attributes(session.slave), session.before)
        for color in ("none", "16", "256", "truecolor"):
            session = self.session("--frames", "1", "--size", "1x1", "--color", color)
            self.assertEqual(session.finish(), 0)
            self.restored(session)
            if color != "truecolor":
                self.assertNotIn(b"38;2", session.output)
            if color == "none":
                self.assertNotIn(b"\x1b[40m", session.output)
                self.assertNotIn(b"\x1b[30m", session.output)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout))
