#!/usr/bin/env python3
"""Smoke test of an Obsidian Edge distribution folder in Hatari (no screen needed).

  tools/smoke_test.py --dist build/dist/OBSIDIAN --tos etos512uk.img --out build/smoke

Two runs of Hatari (ST, 4 MB, monochrome monitor, EmuTOS), with a copy of the
distribution folder as GEMDOS drive C:

 1. INSTALL.PRG is started; the test types a name, selects "Install Diamond
    Mirror: No" and clicks "Install".  Checks: C:\\EDGE\\ holds EDGE.PRG,
    EDGE.RSC and the help files, and the installed EDGE.PRG differs from the
    distributed one (INSTALL.PRG writes the registration data into it).
 2. The installed EDGE.PRG is started.  Checks: it opens EDGE.RSC, then gets
    past the registration check (it looks for its preferences file EDGE.INF,
    which an unregistered copy never does), and it is still running.

Screenshots and Hatari logs (with the GEMDOS trace) are kept in --out.
This only proves that the programs start and that their GEM user interface
works under EmuTOS in Hatari; it does not test any disk function.

Prerequisites (Debian/Ubuntu): hatari xvfb xdotool, and an EmuTOS image
(--tos, or the EMUTOS environment variable).
"""
import argparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

SCREEN_W, SCREEN_H = 640, 400          # ST high resolution (monochrome monitor)

# positions on the INSTALL.PRG dialog (ST high resolution, EmuTOS)
INSTALL_NAME_FIELD = (240, 249)
INSTALL_MIRROR_NO = (498, 345)
INSTALL_BUTTON = (316, 360)


class Hatari:
    """Hatari in Xvfb, with a control socket for screenshots."""

    def __init__(self, tos, hd, prg, out, tag, display):
        self.out, self.tag, self.display = out, tag, display
        self.work = tempfile.mkdtemp(prefix="oe_hatari_")
        sock_path = os.path.join(self.work, "ctl.sock")
        self.srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.srv.bind(sock_path)
        self.srv.listen(1)
        self.env = dict(os.environ, DISPLAY=display, SDL_AUDIODRIVER="dummy", HOME=self.work)
        self.xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1024x768x24"],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)
        self.shots = os.path.join(out, tag)
        os.makedirs(self.shots, exist_ok=True)
        self.logpath = os.path.join(out, tag + "-hatari.log")
        cmd = ["hatari", "--machine", "st", "--memsize", "4096", "--tos", tos,
               "--monitor", "mono", "--sound", "off", "--statusbar", "off",
               "--borders", "off", "--zoom", "1", "--fast-boot", "true",
               "--confirm-quit", "off", "--mousewarp", "off",
               "--screenshot-dir", self.shots, "--control-socket", sock_path,
               "--harddrive", hd, "--auto", prg, "--trace", "gemdos"]
        self.log = open(self.logpath, "w")
        self.proc = subprocess.Popen(cmd, env=self.env, stdout=self.log, stderr=subprocess.STDOUT)
        self.srv.settimeout(30)
        try:
            self.conn, _ = self.srv.accept()
        except socket.timeout:
            self.stop()
            sys.exit("FAIL: Hatari did not start (see %s)" % self.logpath)
        self.t0 = time.time()

    def wait_until(self, t):
        time.sleep(max(0.0, t - (time.time() - self.t0)))

    def window(self):
        ids = subprocess.run(["xdotool", "search", "--name", "hatari"], env=self.env,
                             capture_output=True, text=True).stdout.split()
        return ids[0] if ids else None

    def xdo(self, *args):
        subprocess.run(["xdotool"] + list(args), env=self.env, stderr=subprocess.DEVNULL)

    def click(self, x, y):
        win = self.window()
        if not win:
            sys.exit("FAIL: Hatari window not found")
        self.xdo("mousemove", "--window", win, str(x), str(y))
        time.sleep(0.5)
        self.xdo("click", "1")
        time.sleep(0.5)

    def type(self, text):
        win = self.window()
        if win:
            self.xdo("windowfocus", win)
        for ch in text:
            self.xdo("key", ch)
            time.sleep(0.2)

    def screenshot(self):
        self.conn.sendall(b"hatari-shortcut screenshot\n")
        time.sleep(1.0)

    def running(self):
        return self.proc.poll() is None

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        self.xvfb.terminate()
        self.log.close()
        shutil.rmtree(self.work, ignore_errors=True)

    def trace(self):
        with open(self.logpath, errors="replace") as f:
            return f.read()


def check(cond, msg, failures):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failures.append(msg)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dist", required=True, help="distribution folder (make dist)")
    ap.add_argument("--tos", default=os.environ.get("EMUTOS"), help="EmuTOS image (or $EMUTOS)")
    ap.add_argument("--out", required=True, help="folder for screenshots and logs")
    ap.add_argument("--display", default=":98")
    a = ap.parse_args()

    if not a.tos or not os.path.isfile(a.tos):
        sys.exit("FAIL: no EmuTOS image: pass --tos or set EMUTOS")
    for tool in ("hatari", "Xvfb", "xdotool"):
        if not shutil.which(tool):
            sys.exit("FAIL: missing tool: " + tool)

    out = os.path.abspath(a.out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    hd = os.path.join(out, "drive_c")
    shutil.copytree(a.dist, hd)
    tos = os.path.abspath(a.tos)
    failures = []

    # 1. INSTALL.PRG
    h = Hatari(tos, hd, "C:\\INSTALL.PRG", out, "1-install", a.display)
    try:
        h.wait_until(12)
        h.screenshot()
        h.click(*INSTALL_NAME_FIELD)
        h.type("TEST")
        h.click(*INSTALL_MIRROR_NO)
        h.screenshot()
        h.click(*INSTALL_BUTTON)
        time.sleep(8)
        h.screenshot()
        time.sleep(20)
        h.screenshot()
    finally:
        h.stop()

    edge_dir = os.path.join(hd, "EDGE")
    installed = os.path.join(edge_dir, "EDGE.PRG")
    check(os.path.isdir(edge_dir), "INSTALL.PRG created C:\\EDGE", failures)
    for name in ("EDGE.PRG", "EDGE.RSC"):
        check(os.path.isfile(os.path.join(edge_dir, name)), "C:\\EDGE\\%s installed" % name, failures)
    if os.path.isfile(installed):
        with open(installed, "rb") as f1, open(os.path.join(a.dist, "EDGE.PRG"), "rb") as f2:
            check(f1.read() != f2.read(), "installed EDGE.PRG carries the registration data", failures)

    # 2. the installed EDGE.PRG
    if os.path.isfile(installed):
        h = Hatari(tos, hd, "C:\\EDGE\\EDGE.PRG", out, "2-edge", a.display)
        try:
            h.wait_until(20)
            h.screenshot()
            alive = h.running()
        finally:
            h.stop()
        log = h.trace()
        check('EDGE.RSC' in log and 'Fopen' in log, "EDGE.PRG opened EDGE.RSC", failures)
        check('EDGE.INF' in log, "EDGE.PRG got past the registration check (looked for EDGE.INF)", failures)
        check(alive, "Hatari was still running EDGE.PRG after 20 s", failures)

    print("screenshots and logs in", out)
    if failures:
        sys.exit("%d check(s) failed" % len(failures))
    print("smoke test passed")


if __name__ == "__main__":
    main()
