#!/usr/bin/env python3
"""Host renderer for the obegransad simulator.

The firmware's panel_sim backend connects here over TCP, sends 16x16 frames
(one byte per pixel) and accepts simulated button presses back. Under QEMU the
connection arrives from 10.0.2.2 via slirp; on a host build from 127.0.0.1.
Note: slirp connects from the host interface address rather than loopback, so
the QEMU runner binds 0.0.0.0; use --host 127.0.0.1 when running locally only.

Modes:
  default   tkinter window with the panel, connection status and buttons
  --ascii   headless text rendering (for SSH / automated checks)

Keys and buttons send a single command byte to the firmware:
  s = short press, d = double press, l = long press
"""

import argparse
import queue
import socket
import sys
import threading
import time

MAGIC = b"OBG\x01"
PANEL = 16
FRAME_BYTES = PANEL * PANEL

# Logical brightness levels -> display colors. The firmware maps pixels to
# PANEL_BRIGHTNESS_1/2/3 (3/63/255); anything else gets a linear gray.
LEVELS = {
    0: "#0d0d0d",
    3: "#303030",
    63: "#8c8c8c",
    255: "#f2f2f2",
}
ASCII_LEVELS = {0: " ", 3: "\u2591", 63: "\u2592", 255: "\u2588"}


def gray_for(value: int) -> str:
    if value in LEVELS:
        return LEVELS[value]
    return "#%02x%02x%02x" % (value, value, value)


def ascii_for(value: int) -> str:
    if value in ASCII_LEVELS:
        return ASCII_LEVELS[value]
    return " " if value == 0 else "\u2593"


def recv_exact(sock: socket.socket, n: int):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


class Link(threading.Thread):
    """TCP server; the firmware connects as a client and reconnects on its own."""

    def __init__(self, host: str, port: int, events: "queue.Queue"):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.events = events
        self._conn = None
        self._lock = threading.Lock()

    def run(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((self.host, self.port))
        server.listen(1)
        self.events.put(("listening", self.host, self.port))
        while True:
            conn, addr = server.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with self._lock:
                self._conn = conn
            self.events.put(("connected", addr[0]))
            try:
                while True:
                    packet = recv_exact(conn, len(MAGIC) + FRAME_BYTES)
                    if packet is None:
                        break
                    if packet[: len(MAGIC)] != MAGIC:
                        self.events.put(("note", "bad frame magic; skipping"))
                        continue
                    self.events.put(("frame", packet[len(MAGIC):]))
            except OSError:
                pass
            finally:
                with self._lock:
                    self._conn = None
                conn.close()
                self.events.put(("disconnected",))

    def send(self, payload: bytes) -> bool:
        with self._lock:
            if self._conn is None:
                return False
            try:
                self._conn.sendall(payload)
                return True
            except OSError:
                return False


def drain_latest_frame(events: "queue.Queue", state: dict):
    """Apply all queued events; keep only the newest frame."""
    frame = None
    while True:
        try:
            event = events.get_nowait()
        except queue.Empty:
            break
        kind = event[0]
        if kind == "frame":
            frame = event[1]
            state["frames"] += 1
        elif kind == "connected":
            state["connected"] = True
            state["peer"] = event[1]
        elif kind == "disconnected":
            state["connected"] = False
        elif kind == "listening":
            state["listening"] = (event[1], event[2])
        elif kind == "note":
            print(f"[renderer] {event[1]}", file=sys.stderr)
    return frame


def run_ascii(link: Link):
    events = link.events
    state = {"connected": False, "frames": 0, "peer": None, "listening": None}
    link.start()
    last_frame = None
    frames_at_last_stats = 0
    last_stats = time.monotonic()
    try:
        while True:
            frame = drain_latest_frame(events, state)
            now = time.monotonic()

            if frame is not None and frame != last_frame:
                last_frame = frame
                if state["connected"]:
                    status = f"connected to {state['peer']}"
                elif state["listening"]:
                    status = f"listening on {state['listening'][0]}:" \
                             f"{state['listening'][1]}"
                else:
                    status = "starting"
                print(f"--- {status}, frame {state['frames']} ---")
                for row in range(PANEL):
                    cells = frame[row * PANEL:(row + 1) * PANEL]
                    print("".join(ascii_for(v) for v in cells))
                sys.stdout.flush()

            if now - last_stats >= 5.0:
                fps = (state["frames"] - frames_at_last_stats) / (now - last_stats)
                frames_at_last_stats = state["frames"]
                last_stats = now
                if last_frame is not None:
                    print(f"[renderer] {state['frames']} frames, {fps:.1f} fps")
                    sys.stdout.flush()
            time.sleep(0.02)
    except KeyboardInterrupt:
        pass


def run_gui(link: Link, scale: int):
    import tkinter as tk

    events = link.events
    state = {"connected": False, "frames": 0, "peer": None, "listening": None}
    link.start()

    root = tk.Tk()
    root.title("obegransad simulator")
    root.configure(bg="#1a1a1a")

    header = tk.Frame(root, bg="#1a1a1a")
    header.pack(fill="x", padx=10, pady=(10, 4))
    status = tk.Label(header, text="starting...", fg="#cccccc", bg="#1a1a1a",
                      font=("monospace", 10), anchor="w")
    status.pack(side="left")

    canvas = tk.Canvas(root, width=PANEL * scale, height=PANEL * scale,
                       highlightthickness=0, bg="#000000")
    canvas.pack(padx=10)

    cells = []
    for row in range(PANEL):
        row_ids = []
        for col in range(PANEL):
            row_ids.append(canvas.create_rectangle(
                col * scale, row * scale, (col + 1) * scale, (row + 1) * scale,
                fill=LEVELS[0], outline="#242424"))
        cells.append(row_ids)

    buttons = tk.Frame(root, bg="#1a1a1a")
    buttons.pack(fill="x", padx=10, pady=8)

    def send(cmd: str):
        link.send(cmd.encode())

    for label, cmd, key in (("Short (s)", "S", "s"),
                            ("Double (d)", "D", "d"),
                            ("Long (l)", "L", "l")):
        tk.Button(buttons, text=label, command=lambda c=cmd: send(c)).pack(
            side="left", padx=(0, 6))
        root.bind(key, lambda _e, c=cmd: send(c))

    footer = tk.Label(root, text="", fg="#888888", bg="#1a1a1a",
                      font=("monospace", 9), anchor="w", justify="left")
    footer.pack(fill="x", padx=10, pady=(0, 10))

    last_frame = [None]
    fps_frames = [0]
    fps_last = [time.monotonic()]
    fps_value = [0.0]

    def tick():
        frame = drain_latest_frame(events, state)
        if frame is not None:
            if frame != last_frame[0]:
                previous = last_frame[0] or bytes(FRAME_BYTES)
                for row in range(PANEL):
                    for col in range(PANEL):
                        idx = row * PANEL + col
                        if frame[idx] != previous[idx]:
                            canvas.itemconfigure(cells[row][col],
                                                 fill=gray_for(frame[idx]))
                last_frame[0] = frame
                fps_frames[0] += 1

        now = time.monotonic()
        if now - fps_last[0] >= 1.0:
            fps_value[0] = fps_frames[0] / (now - fps_last[0])
            fps_frames[0] = 0
            fps_last[0] = now

        if state["connected"]:
            link_text = f"renderer: connected to {state['peer']}"
        elif state["listening"]:
            link_text = f"renderer: listening on {state['listening'][0]}:" \
                        f"{state['listening'][1]}"
        else:
            link_text = "renderer: starting"
        status.configure(text=link_text)
        footer.configure(text=f"frames: {state['frames']}   "
                              f"update rate: {fps_value[0]:.1f} fps")
        root.after(20, tick)

    tick()
    try:
        root.mainloop()
    except KeyboardInterrupt:
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1",
                        help="bind address (default: %(default)s)")
    parser.add_argument("--port", type=int, default=5566,
                        help="TCP port to listen on (default: %(default)s)")
    parser.add_argument("--scale", type=int, default=26,
                        help="pixel size in the tkinter window (default: %(default)s)")
    parser.add_argument("--ascii", action="store_true",
                        help="no GUI: print the panel as text")
    args = parser.parse_args()

    events = queue.Queue()
    link = Link(args.host, args.port, events)

    if args.ascii:
        run_ascii(link)
    else:
        run_gui(link, args.scale)


if __name__ == "__main__":
    main()
