#!/usr/bin/env python3
"""
Stepper PID Tuner — PC-side tool for MSPM0G3507 balance-ball controller.

Connects to the stepper board's DEBUG UART (PA10/PA11, 115200 8N1).
Parses the live printf status lines and lets you send tuning commands interactively.

Usage:
    python stepper_tuner.py COM3              # interactive mode
    python stepper_tuner.py COM3 --log run1    # log to run1.csv
    python stepper_tuner.py COM3 --sim         # also send simulated ball positions

Dependencies: pyserial (pip install pyserial)
Optional:     matplotlib (pip install matplotlib) for real-time plotting
"""

import sys
import os
import time
import csv
import math
import struct
import threading
import argparse
import re
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

HAS_PLOT = False
try:
    import matplotlib
    matplotlib.use('QtAgg')
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    from collections import deque
    HAS_PLOT = True
except Exception:
    pass  # matplotlib not available, plotting disabled


# ==================== Data Model ====================
class StepperState:
    """Parsed state from the stepper board's printf output."""
    def __init__(self):
        self.time_ms = 0
        self.ball_x_mm = 0.0
        self.target_tilt_deg = 0.0
        self.encoder_deg = 0.0
        self.stepper_busy = 0
        self.bal_kp = 0.0
        self.enc_kp = 0.0
        self.timestamp = 0.0  # PC time

    def __repr__(self):
        return (f"t={self.time_ms}ms ball={self.ball_x_mm:.1f}mm "
                f"target={self.target_tilt_deg:.2f}deg enc={self.encoder_deg:.2f}deg "
                f"busy={self.stepper_busy}")


# ==================== Serial Handler ====================
class SerialHandler:
    """Manages serial connection, non-blocking read with line buffering."""

    def __init__(self, port, baud=115200):
        self.port = port
        self.baud = baud
        self.ser = None
        self.rx_buf = b""
        self.lines = []  # complete lines ready for parsing

    def connect(self):
        self.ser = serial.Serial(self.port, self.baud, timeout=0.01)
        self.rx_buf = b""
        print(f"[SERIAL] connected to {self.port} at {self.baud} baud")

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[SERIAL] disconnected")

    def send(self, text):
        """Send a text command + newline to the stepper board."""
        if self.ser and self.ser.is_open:
            cmd = (text + "\r\n").encode('ascii')
            self.ser.write(cmd)
            # Echo to local console
            print(f"  >> {text}")

    def poll(self):
        """Non-blocking read, returns list of complete lines."""
        self.lines = []
        if not self.ser or not self.ser.is_open:
            return self.lines

        try:
            waiting = self.ser.in_waiting
            if waiting > 0:
                data = self.ser.read(waiting)
                self.rx_buf += data

                # Split on \r\n or \n
                while True:
                    # Find line ending
                    idx_rn = self.rx_buf.find(b'\r\n')
                    idx_n = self.rx_buf.find(b'\n')
                    idx = -1
                    if idx_rn >= 0 and idx_n >= 0:
                        idx = min(idx_rn, idx_n)
                    elif idx_rn >= 0:
                        idx = idx_rn
                    elif idx_n >= 0:
                        idx = idx_n
                    else:
                        break

                    line = self.rx_buf[:idx].decode('ascii', errors='replace').strip()
                    # Skip the delimiter
                    skip = 2 if self.rx_buf[idx:idx+2] == b'\r\n' else 1
                    self.rx_buf = self.rx_buf[idx+skip:]
                    if line:
                        self.lines.append(line)
        except serial.SerialException as e:
            print(f"[SERIAL ERROR] {e}")

        return self.lines


# ==================== Protocol Parser ====================
BAL_RE = re.compile(
    r'\[BAL\]\s+t=(\d+)ms\s+ball=([\d.-]+)mm\s+target=([\d.-]+)deg\s+'
    r'enc=([\d.-]+)deg\s+busy=(\d+)\s+kp_b=([\d.-]+)\s+kp_e=([\d.-]+)'
)

OK_RE = re.compile(r'\[OK\]\s+(.*)')
ERR_RE = re.compile(r'\[ERR\]\s+(.*)')
TEST_RE = re.compile(r'\[TEST\]\s+(.*)')


def parse_line(line):
    """Parse a line from the stepper board. Returns (type, data) or None."""
    m = BAL_RE.match(line)
    if m:
        state = StepperState()
        state.time_ms = int(m.group(1))
        state.ball_x_mm = float(m.group(2))
        state.target_tilt_deg = float(m.group(3))
        state.encoder_deg = float(m.group(4))
        state.stepper_busy = int(m.group(5))
        state.bal_kp = float(m.group(6))
        state.enc_kp = float(m.group(7))
        state.timestamp = time.monotonic()
        return ('bal', state)

    m = OK_RE.match(line)
    if m:
        return ('ok', m.group(1))

    m = ERR_RE.match(line)
    if m:
        return ('err', m.group(1))

    m = TEST_RE.match(line)
    if m:
        return ('test', m.group(1))

    # Catch other useful lines (status output, help, etc.)
    if line.startswith('=') or line.startswith('[') or line.startswith('MSPM0'):
        return ('info', line)

    # Plain text
    if line:
        return ('text', line)

    return None


# ==================== Plot Window (optional) ====================
class LivePlot:
    """Matplotlib real-time plotting window."""

    def __init__(self, max_points=500):
        if not HAS_PLOT:
            raise RuntimeError("matplotlib not available")
        self.max_points = max_points
        self.t_data = deque(maxlen=max_points)
        self.ball_data = deque(maxlen=max_points)
        self.target_data = deque(maxlen=max_points)
        self.enc_data = deque(maxlen=max_points)

        plt.ion()
        self.fig, (self.ax1, self.ax2) = plt.subplots(2, 1, figsize=(10, 7),
                                                        sharex=True)
        self.fig.canvas.manager.set_window_title('Stepper PID Tuner')

        self.line_ball, = self.ax1.plot([], [], 'b-', label='Ball X (mm)', linewidth=1.5)
        self.ax1.axhline(y=0, color='gray', linestyle='--', alpha=0.5)
        self.ax1.set_ylabel('Ball Position (mm)')
        self.ax1.legend(loc='upper right')
        self.ax1.grid(True, alpha=0.3)

        self.line_target, = self.ax2.plot([], [], 'r-', label='Target Tilt (deg)', linewidth=1.5)
        self.line_enc, = self.ax2.plot([], [], 'g-', label='Encoder (deg)', linewidth=1.0, alpha=0.7)
        self.ax2.set_ylabel('Angle (deg)')
        self.ax2.set_xlabel('Time (s)')
        self.ax2.legend(loc='upper right')
        self.ax2.grid(True, alpha=0.3)

        self.start_time = None
        plt.tight_layout()

    def update(self, state):
        """Add a data point and refresh plot."""
        if self.start_time is None:
            self.start_time = state.timestamp

        t = state.timestamp - self.start_time
        self.t_data.append(t)
        self.ball_data.append(state.ball_x_mm)
        self.target_data.append(state.target_tilt_deg)
        self.enc_data.append(state.encoder_deg)

        self.line_ball.set_data(list(self.t_data), list(self.ball_data))
        self.line_target.set_data(list(self.t_data), list(self.target_data))
        self.line_enc.set_data(list(self.t_data), list(self.enc_data))

        if self.t_data:
            self.ax1.set_xlim(max(0, self.t_data[-1] - 30), max(30, self.t_data[-1] + 1))
            self.ax2.set_xlim(max(0, self.t_data[-1] - 30), max(30, self.t_data[-1] + 1))

        y_max = max(5, max(abs(v) for v in self.ball_data) * 1.3) if self.ball_data else 5
        self.ax1.set_ylim(-y_max, y_max)

        self.fig.canvas.draw_idle()
        self.fig.canvas.flush_events()


# ==================== Main Tuner Application ====================
class StepperTuner:
    """Interactive stepper PID tuning tool."""

    def __init__(self, port, baud=115200, log_file=None, sim_mode=False):
        self.serial = SerialHandler(port, baud)
        self.log_file = log_file
        self.csv_fh = None
        self.csv_writer = None
        self.sim_mode = sim_mode
        self.plot = None
        self.latest_state = None
        self.running = False

    def start(self):
        self.serial.connect()
        self.running = True

        if self.log_file:
            self.csv_fh = open(self.log_file, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_fh)
            self.csv_writer.writerow([
                'timestamp', 'time_ms', 'ball_x_mm', 'target_tilt_deg',
                'encoder_deg', 'stepper_busy', 'bal_kp', 'enc_kp'
            ])
            print(f"[LOG] writing to {self.log_file}")

        if HAS_PLOT:
            resp = input("Enable real-time plot? (y/n) [n]: ").strip().lower()
            if resp == 'y':
                self.plot = LivePlot()
                print("[PLOT] window opened — close it to stop plotting")

    def stop(self):
        self.running = False
        self.serial.disconnect()
        if self.csv_fh:
            self.csv_fh.close()
            print(f"[LOG] saved to {self.log_file}")
        if self.plot:
            plt.close('all')

    def show_status(self, state):
        """Print a single-line status update."""
        bar_len = 30
        if state.ball_x_mm != 0:
            ratio = min(1.0, abs(state.ball_x_mm) / 100.0)
            filled = int(bar_len * ratio)
            bar = '█' * filled + '░' * (bar_len - filled)
            if state.ball_x_mm > 0:
                bar = '  center |' + bar + '→'
            else:
                bar = '←|' + bar + 'center  '
        else:
            bar = '  center |' + '░' * bar_len + '|  '

        print(f"\r  Ball:{state.ball_x_mm:+6.1f}mm  Tilt:{state.target_tilt_deg:+6.2f}deg  "
              f"Enc:{state.encoder_deg:+6.2f}deg  {bar}", end='', flush=True)

    def run_interactive(self):
        """Main interactive loop."""
        print("\n" + "=" * 60)
        print("  Stepper PID Tuner — Interactive Mode")
        print("=" * 60)
        print("  Commands:")
        print("    kp_b 0.05    ki_b 0.001   kd_b 0.0001  (balance PID)")
        print("    kp_e 1.2     ki_e 0.1     kd_e 0.03    (encoder PID)")
        print("    freq 2 1500  (set near tier to 1500Hz)")
        print("    burst 8      tilt_max 4   deadband 2")
        print("    status       reset        self_test")
        print("    sim sine 50 3000    (simulate: type amp period_ms)")
        print("    sim off              (stop simulation)")
        print("    log filename.csv     (start CSV logging)")
        print("    quit")
        print("=" * 60)
        print("  Type commands at the prompt. Live data scrolls above.\n")

        # Start serial reader thread
        sim_thread = None
        sim_lock = threading.Lock()
        sim_config = {'active': False, 'pattern': 'sine', 'amp': 50, 'period': 3000,
                      't0': time.monotonic()}

        def sim_worker():
            """Send simulated ball positions via the K230 binary protocol."""
            while self.running:
                with sim_lock:
                    if not sim_config['active']:
                        time.sleep(0.1)
                        continue
                    pattern = sim_config['pattern']
                    amp = sim_config['amp']
                    period_ms = sim_config['period']
                    t0 = sim_config['t0']

                elapsed_ms = (time.monotonic() - t0) * 1000

                if pattern == 'sine':
                    x = amp * math.sin(2 * math.pi * elapsed_ms / period_ms)
                elif pattern == 'step':
                    half = period_ms / 2
                    x = amp if (elapsed_ms % period_ms) < half else -amp
                elif pattern == 'ramp':
                    x = amp * (2 * (elapsed_ms % period_ms) / period_ms - 1)
                else:
                    x = 0

                # Send as binary K230 frame: AA 01 DH DL CS 55
                val = int(x) & 0xFFFF
                dh = (val >> 8) & 0xFF
                dl = val & 0xFF
                cs = (0x01 + dh + dl) & 0xFF
                frame = bytes([0xAA, 0x01, dh, dl, cs, 0x55])
                try:
                    self.serial.ser.write(frame)
                except Exception:
                    pass

                time.sleep(0.015)  # ~60Hz, close to K230 real rate

        print("[INFO] Starting simulation thread...")
        sim_thread = threading.Thread(target=sim_worker, daemon=True)
        sim_thread.start()

        # Main event loop
        import select

        try:
            while self.running:
                # Poll serial
                lines = self.serial.poll()

                for line in lines:
                    result = parse_line(line)
                    if result is None:
                        continue

                    typ, data = result
                    if typ == 'bal':
                        self.latest_state = data
                        self.show_status(data)
                        if self.csv_writer:
                            self.csv_writer.writerow([
                                datetime.now().isoformat(), data.time_ms,
                                data.ball_x_mm, data.target_tilt_deg,
                                data.encoder_deg, data.stepper_busy,
                                data.bal_kp, data.enc_kp
                            ])
                        if self.plot:
                            try:
                                self.plot.update(data)
                            except Exception:
                                pass
                    elif typ == 'ok':
                        print(f"\n  [OK] {data}")
                    elif typ == 'err':
                        print(f"\n  [ERROR] {data}")
                    elif typ == 'test':
                        print(f"\n  [TEST] {data}")
                    elif typ == 'info':
                        print(f"\n  {data}")
                    elif typ == 'text':
                        print(f"\n  {data}")

                # Check stdin for user commands
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    cmd = sys.stdin.readline().strip()
                    if not cmd:
                        continue

                    parts = cmd.split()
                    action = parts[0].lower()

                    if action == 'quit' or action == 'q':
                        self.running = False
                        break
                    elif action == 'sim':
                        with sim_lock:
                            if len(parts) >= 2 and parts[1] == 'off':
                                sim_config['active'] = False
                                print("\n  [SIM] stopped")
                            elif len(parts) >= 4:
                                sim_config['pattern'] = parts[1]
                                sim_config['amp'] = float(parts[2])
                                sim_config['period'] = int(parts[3])
                                sim_config['t0'] = time.monotonic()
                                sim_config['active'] = True
                                print(f"\n  [SIM] {parts[1]} amp={parts[2]}mm "
                                      f"period={parts[3]}ms")
                            else:
                                print("\n  Usage: sim <sine|step|ramp|off> <amp_mm> <period_ms>")
                    elif action == 'log':
                        if len(parts) >= 2:
                            fname = parts[1]
                            if self.csv_fh:
                                self.csv_fh.close()
                            self.csv_fh = open(fname, 'w', newline='')
                            self.csv_writer = csv.writer(self.csv_fh)
                            self.csv_writer.writerow([
                                'timestamp', 'time_ms', 'ball_x_mm', 'target_tilt_deg',
                                'encoder_deg', 'stepper_busy', 'bal_kp', 'enc_kp'
                            ])
                            print(f"\n  [LOG] started: {fname}")
                        else:
                            print("\n  Usage: log <filename.csv>")
                    else:
                        # Forward all other commands to stepper board
                        self.serial.send(cmd)

                time.sleep(0.001)  # Don't burn CPU

        except KeyboardInterrupt:
            print("\n[INFO] Interrupted by user")
        finally:
            self.stop()


# ==================== CLI Entry Point ====================
def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for p in ports:
        print(f"  {p.device} — {p.description}")


def main():
    parser = argparse.ArgumentParser(
        description='Stepper PID Tuner for MSPM0G3507 Balance-Ball Controller')
    parser.add_argument('port', nargs='?', help='Serial port (e.g., COM3)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--log', type=str, default=None, help='CSV log file')
    parser.add_argument('--sim', action='store_true', help='Enable ball position simulation')
    parser.add_argument('--list', action='store_true', help='List serial ports and exit')
    parser.add_argument('--send', type=str, default=None, help='Send a single command and exit')

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    if not args.port:
        list_ports()
        print("\nUsage: python stepper_tuner.py <PORT> [--sim] [--log file.csv]")
        print("Example: python stepper_tuner.py COM3 --sim --log test1.csv")
        return

    if args.send:
        # Single-command mode: send command, read response for 2 seconds, exit
        s = SerialHandler(args.port, args.baud)
        s.connect()
        s.send(args.send)
        start = time.monotonic()
        while time.monotonic() - start < 2.0:
            lines = s.poll()
            for line in lines:
                print(line)
            time.sleep(0.01)
        s.disconnect()
        return

    # Interactive mode
    tuner = StepperTuner(args.port, args.baud, args.log, args.sim)
    try:
        tuner.start()
        tuner.run_interactive()
    except Exception as e:
        print(f"\n[FATAL] {e}")
        tuner.stop()


if __name__ == '__main__':
    main()
