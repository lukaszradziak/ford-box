#!/usr/bin/env python3
"""
Uzycie:
  python3 rtt_monitor.py

Wymagania:
  - ST-Link podlaczony do plytki
  - Firmware skompilowany z SEGGER_RTT_Init() w setup()
  - PlatformIO zainstalowane (dostarcza OpenOCD)
"""

import subprocess
import socket
import select
import tty
import termios
import time
import os
import glob
import sys
import signal

OPENOCD_TELNET = 4444
RTT_PORT       = 19021
RTT_RAM_START  = 0x20000000
RTT_RAM_SIZE   = 0x10000    # 64KB RAM STM32F105


def find_openocd():
    home = os.path.expanduser("~")
    candidates = sorted(glob.glob(f"{home}/.platformio/packages/tool-openocd*/bin/openocd"), reverse=True)
    if candidates:
        return candidates[0]
    for fallback in ("/usr/local/bin/openocd", "/usr/bin/openocd", "openocd"):
        if os.path.isfile(fallback) or fallback == "openocd":
            return fallback
    return "openocd"


def find_scripts():
    home = os.path.expanduser("~")
    # PlatformIO moze miec scripts w tool-openocd/scripts/ lub tool-openocd/openocd/scripts/
    patterns = [
        f"{home}/.platformio/packages/tool-openocd*/openocd/scripts",
        f"{home}/.platformio/packages/tool-openocd*/scripts",
    ]
    for pattern in patterns:
        candidates = sorted(glob.glob(pattern), reverse=True)
        if candidates:
            return candidates[0]
    return None


def telnet_cmd(sock, cmd, wait=0.5):
    sock.sendall((cmd + "\r\n").encode())
    time.sleep(wait)
    data = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    return data.decode("utf-8", errors="replace").strip()


def wait_port_open(port, timeout=6.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.create_connection(("localhost", port), timeout=0.3)
            s.close()
            return True
        except OSError:
            time.sleep(0.2)
    return False


def main():
    openocd = find_openocd()
    scripts = find_scripts()

    if scripts is None:
        print("ERROR: Nie znaleziono OpenOCD scripts.")
        print("       Wykonaj najpierw: pio run -e ucds-stlink --target upload")
        print("       (PlatformIO pobierze wtedy tool-openocd)")
        sys.exit(1)

    print(f"OpenOCD : {openocd}")
    print(f"Scripts : {scripts}")
    print("Startuje OpenOCD z ST-Link...\n")

    proc = subprocess.Popen(
        [openocd, "-s", scripts, "-f", "interface/stlink.cfg", "-f", "target/stm32f1x.cfg"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )

    if not wait_port_open(OPENOCD_TELNET, timeout=6):
        err = proc.stderr.read().decode("utf-8", errors="replace")
        print(f"ERROR: OpenOCD nie uruchomil sie.\n\n{err}")
        print("Sprawdz:")
        print("  - Czy ST-Link jest podlaczony do plytki?")
        print("  - Czy plytka ma zasilanie?")
        print("  - Czy ST-Link jest podlaczony do komputera?")
        proc.terminate()
        sys.exit(1)

    # Konfiguruj RTT przez telnet OpenOCD
    t = socket.create_connection(("localhost", OPENOCD_TELNET))
    t.settimeout(1.0)
    time.sleep(0.3)
    try:
        t.recv(4096)  # banner OpenOCD
    except socket.timeout:
        pass

    r = telnet_cmd(t, f'rtt setup {RTT_RAM_START:#x} {RTT_RAM_SIZE:#x} "SEGGER RTT"')
    print(f"rtt setup  : {r}")
    r = telnet_cmd(t, f"rtt server start {RTT_PORT} 0")
    print(f"rtt server : {r}")
    r = telnet_cmd(t, "rtt start")
    print(f"rtt start  : {r}")
    t.close()

    # Polacz z RTT server
    if not wait_port_open(RTT_PORT, timeout=4):
        print(f"\nERROR: RTT server nie odpowiada na porcie {RTT_PORT}.")
        print("Sprawdz czy firmware wywoluje SEGGER_RTT_Init() w setup().")
        proc.terminate()
        sys.exit(1)

    rtt = socket.create_connection(("localhost", RTT_PORT))
    rtt.setblocking(False)

    print(f"\n=== RTT Monitor aktywny (Ctrl+C wyjscie) ===\n")

    # Przelacz terminal w tryb raw — kazdy znak idzie od razu bez Enter
    fd = sys.stdin.fileno()
    old_tty = termios.tcgetattr(fd)

    def cleanup(sig=None, frame=None):
        termios.tcsetattr(fd, termios.TCSADRAIN, old_tty)
        sys.stdout.write("\nZamykam RTT monitor...\n")
        sys.stdout.flush()
        try:
            rtt.close()
        except Exception:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except Exception:
            pass
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    tty.setraw(fd)

    try:
        while True:
            r, _, _ = select.select([rtt, sys.stdin], [], [], 0.05)

            if rtt in r:
                try:
                    data = rtt.recv(256)
                    if data:
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                except BlockingIOError:
                    pass

            if sys.stdin in r:
                ch = sys.stdin.buffer.read(1)
                if ch == b'\x03':  # Ctrl+C
                    cleanup()
                if ch:
                    rtt.sendall(ch)

            if proc.poll() is not None:
                sys.stdout.write("\nOpenOCD zakonczyl prace nieoczekiwanie.\n")
                break
    except Exception as e:
        sys.stdout.write(f"\nBlad: {e}\n")
    finally:
        cleanup()


if __name__ == "__main__":
    main()
