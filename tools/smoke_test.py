#!/usr/bin/env python3
"""Smoke test of an Obsidian Edge distribution folder in Hatari (no screen needed).

  tools/smoke_test.py --dist build/dist/OBSIDIAN --tos etos512uk.img --out build/smoke

Two runs of Hatari (4 MB, EmuTOS), with a copy of the distribution folder as
a GEMDOS drive:

 1. EDGE.PRG is started (ST, monochrome monitor, GEMDOS drive C:).  Checks:
    it opens EDGE.RSC, gets through its start-up (it looks for its
    preferences file EDGE.INF), and it is still running.
 2. Colour: an STE in medium resolution, with an empty FAT12 floppy in A:
    and the GEMDOS drive as D: (so that EDGE works on drive A: and shows its
    main screen and colour icons).  The mouse is moved over the "Undelete"
    menu title, then the first item of the Help menu is chosen.  Checks: the
    menu drops down, the help file HELP\\FILE.HLP is opened (layout of the
    distribution), no CPU exception (bombs), and EDGE is still running.  This catches stack overflows in code that the
    AES calls (USERDEF objects such as the colour icons): with TOS 1.62 one
    crashed the AES as soon as a menu was dropped down; with EmuTOS the
    menu no longer dropped down.

Step 1 uses an ST (--machine to change it: Hatari refuses TOS 1.62 on an ST).

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

# EDGE main screen, ST medium resolution (shown 640x400 by Hatari)
UNDELETE_MENU_TITLE = (350, 5)
UNDELETE_MENU_AREA = (312, 20, 540, 215)      # x0, y0, x1, y1 of the drop-down menu
AWAY_FROM_MENUS = (300, 250)
HELP_MENU_TITLE = (488, 5)
HELP_MENU_FIRST_ITEM = (520, 28)          # opens HELP\FILE.HLP in the file viewer

# CPU exceptions that mean a crash (bus error, address error, illegal instruction)
CRASH_SIGNS = ("Bus Error", "Address Error", "Illegal instruction")


class Hatari:
    """Hatari in Xvfb, with a control socket for screenshots."""

    def __init__(self, tos, hd, prg, out, tag, display, machine="st", monitor="mono", extra=()):
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
        cmd = ["hatari", "--machine", machine, "--memsize", "4096", "--tos", tos,
               "--monitor", monitor, "--sound", "off", "--statusbar", "off",
               "--borders", "off", "--zoom", "1", "--fast-boot", "true",
               "--confirm-quit", "off", "--mousewarp", "off",
               "--screenshot-dir", self.shots, "--control-socket", sock_path,
               "--harddrive", hd, "--auto", prg, "--trace", "gemdos"] + list(extra)
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

    def move(self, x, y):
        win = self.window()
        if not win:
            sys.exit("FAIL: Hatari window not found")
        self.xdo("mousemove", "--window", win, str(x), str(y))

    def screenshot(self):
        """take a screenshot, return its path"""
        before = set(os.listdir(self.shots))
        self.conn.sendall(b"hatari-shortcut screenshot\n")
        time.sleep(1.0)
        new = sorted(set(os.listdir(self.shots)) - before)
        return os.path.join(self.shots, new[-1]) if new else None

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


def blank_floppy(path):
    """an empty 720 KB FAT12 floppy image (80 tracks, 2 sides, 9 sectors)"""
    img = bytearray(80 * 2 * 9 * 512)
    bs = img[0:512]
    bs[0:2] = b"\x60\x38"                       # BRA (never executed: no checksum)
    bs[2:8] = b"OBSIDN"
    bs[8:11] = b"\x12\x34\x56"                 # serial number
    bs[11:13] = (512).to_bytes(2, "little")      # bytes per sector
    bs[13] = 2                                   # sectors per cluster
    bs[14:16] = (1).to_bytes(2, "little")        # reserved sectors
    bs[16] = 2                                   # FATs
    bs[17:19] = (112).to_bytes(2, "little")      # root entries
    bs[19:21] = (1440).to_bytes(2, "little")     # sectors
    bs[21] = 0xF9                                # media
    bs[22:24] = (3).to_bytes(2, "little")        # sectors per FAT
    bs[24:26] = (9).to_bytes(2, "little")        # sectors per track
    bs[26:28] = (2).to_bytes(2, "little")        # sides
    img[0:512] = bs
    for fat in (1, 4):
        img[fat * 512:fat * 512 + 3] = b"\xF9\xFF\xFF"
    with open(path, "wb") as f:
        f.write(img)


def changed_pixels(png1, png2, box):
    """number of pixels that differ between two screenshots, inside box"""
    from PIL import Image, ImageChops
    a = Image.open(png1).convert("RGB").crop(box)
    b = Image.open(png2).convert("RGB").crop(box)
    diff = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v else 0)
    return diff.histogram()[255]


def check(cond, msg, failures):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failures.append(msg)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dist", required=True, help="distribution folder (make dist)")
    ap.add_argument("--tos", default=os.environ.get("EMUTOS"), help="EmuTOS image (or $EMUTOS)")
    ap.add_argument("--out", required=True, help="folder for screenshots and logs")
    ap.add_argument("--machine", default="st", choices=["st", "ste", "megaste", "tt", "falcon"],
                    help="machine for step 1 (TOS 1.62 needs ste); step 2 always uses ste")
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

    # 1. EDGE.PRG, monochrome
    h = Hatari(tos, hd, "C:\\EDGE.PRG", out, "1-edge", a.display, machine=a.machine)
    try:
        h.wait_until(20)
        h.screenshot()
        alive = h.running()
    finally:
        h.stop()
    log = h.trace()
    check('EDGE.RSC' in log and 'Fopen' in log, "EDGE.PRG opened EDGE.RSC", failures)
    check('EDGE.INF' in log, "EDGE.PRG got through its start-up (looked for EDGE.INF)", failures)
    check(alive, "Hatari was still running EDGE.PRG after 20 s", failures)

    # 2. colour, STE medium resolution: drop down a menu
    floppy = os.path.join(out, "blank.st")
    blank_floppy(floppy)
    h = Hatari(tos, hd, "D:\\EDGE.PRG", out, "2-colour", a.display,
               machine="ste", monitor="rgb",
               extra=("--tos-res", "med", "--gemdos-drive", "D", "--disk-a", floppy))
    try:
        h.wait_until(22)
        h.move(*AWAY_FROM_MENUS)
        time.sleep(2)
        before = h.screenshot()
        h.move(*UNDELETE_MENU_TITLE)
        time.sleep(3)
        after = h.screenshot()
        h.move(*AWAY_FROM_MENUS)
        time.sleep(3)
        h.screenshot()
        h.move(*HELP_MENU_TITLE)
        time.sleep(2)
        h.click(*HELP_MENU_FIRST_ITEM)
        time.sleep(4)
        h.screenshot()
        alive = h.running()
    finally:
        h.stop()
    log = h.trace()
    n = changed_pixels(before, after, UNDELETE_MENU_AREA) if before and after else 0
    check(n > 2000, "colour: the Undelete menu dropped down (%d pixels changed)" % n, failures)
    start = log.find('Pexec(0, "D:\\EDGE.PRG"')
    after_start = log[start:] if start >= 0 else ""   # EmuTOS probes the hardware at boot
    check(start >= 0, "colour: EDGE.PRG was started", failures)
    crash = [c for c in CRASH_SIGNS if c in after_start]
    check(not crash, "colour: no CPU exception%s" % (" (%s)" % ", ".join(crash) if crash else ""), failures)
    check('Fopen("D:\\HELP\\FILE.HLP"' in after_start, "colour: the Help menu opened HELP\\FILE.HLP", failures)
    check(alive, "colour: Hatari was still running EDGE.PRG", failures)

    print("screenshots and logs in", out)
    if failures:
        sys.exit("%d check(s) failed" % len(failures))
    print("smoke test passed")


if __name__ == "__main__":
    main()
