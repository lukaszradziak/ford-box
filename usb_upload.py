#!/usr/bin/env python3
"""Build and update ucds-box through its USB connector.

Usage:
    python3 usb_upload.py [port]

The script sends the ``u`` command over USB CDC, waits for the STM32 factory
DFU bootloader, flashes firmware.bin and waits for the application CDC port to
return. Close usb_monitor.py before running this script.
"""

import glob
import datetime
import os
import shutil
import subprocess
import sys
import time


ENVIRONMENT = "genericSTM32F105RB"
FIRMWARE = os.path.join(".pio", "build", ENVIRONMENT, "firmware.bin")
PORT_PATTERNS = ("/dev/cu.usbmodem*", "/dev/ttyACM*")
DFU_USB_ID = "0483:df11"
DFU_BANNER_PREFIXES = (
    "dfu-util ",
    "Copyright ",
    "This program is Free Software",
    "Please report bugs to ",
)
# STM32F105 ROM v2.2 tries several clock/interface states before the delayed
# D+ reconnect is accepted. The tested board enumerates after about 13 s.
DFU_TIMEOUT = 25
CDC_TIMEOUT = 15
LOG_FILE = None


def strip_dfu_banner(output):
    """Remove dfu-util's licence banner while preserving useful output."""
    kept = []
    for line in output.splitlines(keepends=True):
        stripped = line.strip()
        if any(stripped.startswith(prefix) for prefix in DFU_BANNER_PREFIXES):
            continue
        kept.append(line)
    return "".join(kept).lstrip("\r\n")


class Tee:
    def __init__(self, terminal, log_file):
        self.terminal = terminal
        self.log_file = log_file

    def write(self, data):
        self.terminal.write(data)
        self.log_file.write(data)
        self.log_file.flush()
        return len(data)

    def flush(self):
        self.terminal.flush()
        self.log_file.flush()

    def isatty(self):
        return self.terminal.isatty()


def start_logging():
    global LOG_FILE
    os.makedirs("logs", exist_ok=True)
    path = datetime.datetime.now().strftime("logs/usb_upload_%Y%m%d_%H%M%S.log")
    LOG_FILE = open(path, "a", encoding="utf-8", buffering=1)
    sys.stdout = Tee(sys.__stdout__, LOG_FILE)
    sys.stderr = Tee(sys.__stderr__, LOG_FILE)
    print(f"==> Diagnostic log: {os.path.abspath(path)}")


def log_only(text):
    if LOG_FILE is not None:
        LOG_FILE.write(text)
        if not text.endswith("\n"):
            LOG_FILE.write("\n")
        LOG_FILE.flush()


def find_port():
    for pattern in PORT_PATTERNS:
        ports = sorted(glob.glob(pattern))
        if ports:
            return ports[0]
    return None


def find_ports():
    ports = []
    for pattern in PORT_PATTERNS:
        ports.extend(sorted(glob.glob(pattern)))
    return ports


def find_tool(name, platformio_relative_path):
    executable = shutil.which(name)
    if executable:
        return executable
    candidate = os.path.expanduser(
        os.path.join("~", ".platformio", platformio_relative_path)
    )
    return candidate if os.path.isfile(candidate) else None


def dfu_status(dfu_util):
    result = subprocess.run(
        [dfu_util, "-l"], capture_output=True, text=True, check=False
    )
    raw_listing = result.stdout + result.stderr
    listing = strip_dfu_banner(raw_listing).strip()
    return DFU_USB_ID in raw_listing.lower(), listing, result.returncode


def dfu_is_present(dfu_util):
    present, _, _ = dfu_status(dfu_util)
    return present


def build_firmware(pio):
    print(f"==> Building {ENVIRONMENT}...")
    result = subprocess.run(
        [pio, "run", "-e", ENVIRONMENT],
        capture_output=True,
        text=True,
        check=False,
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit("ERROR: firmware build failed")


def request_dfu(port):
    try:
        import serial
    except ImportError:
        sys.exit("ERROR: pyserial is missing; run: pip3 install -r requirements.txt")

    selected_port = port or find_port()
    if not selected_port:
        sys.exit("ERROR: no USB CDC port or existing DFU device was found")

    print(f"==> Requesting DFU through {selected_port}...")
    try:
        with serial.Serial(selected_port, 115200, timeout=1) as device:
            device.write(b"u")
            device.flush()
            try:
                response = device.read(128)
                if response:
                    print("==> Device response: " + response.decode(errors="replace").strip())
                else:
                    print("==> Device response: <none; USB disconnected or timed out>")
            except (OSError, serial.SerialException) as error:
                print(f"==> CDC disconnected after DFU request: {error}")
    except (OSError, serial.SerialException) as error:
        sys.exit(f"ERROR: cannot use {selected_port}: {error}")


def wait_for_dfu(dfu_util):
    print("==> Waiting for STM32 DFU")
    started = time.monotonic()
    deadline = time.monotonic() + DFU_TIMEOUT
    last_logged_state = None
    while time.monotonic() < deadline:
        present, listing, return_code = dfu_status(dfu_util)
        elapsed = time.monotonic() - started
        ports = find_ports()
        print(
            f"    +{elapsed:05.1f}s DFU={'yes' if present else 'no'} "
            f"dfu-util-rc={return_code} CDC={ports or '<none>'}"
        )
        state = (return_code, listing)
        if state != last_logged_state:
            log_only(
                f"--- dfu-util -l state at +{elapsed:05.1f}s "
                f"(rc={return_code}) ---\n"
                f"{listing or '<no DFU device listed>'}\n"
            )
            last_logged_state = state
        if present:
            print("==> STM32 DFU found")
            return
        time.sleep(1)
    capture_usb_diagnostics(dfu_util)
    returned_port = find_port()
    if returned_port:
        sys.exit(
            f"\nERROR: DFU did not appear and application CDC returned at "
            f"{returned_port}; the ROM jump was rejected"
        )
    sys.exit(
        f"\nERROR: DFU device {DFU_USB_ID} did not appear and application CDC "
        "did not return. Power-cycle the board to recover; the factory ROM "
        "bootloader entered but did not select/enumerate USB DFU."
    )


def capture_usb_diagnostics(dfu_util):
    print("==> DFU timeout: collecting macOS/Linux USB diagnostics in the log...")
    commands = [
        [dfu_util, "-l"],
        ["ioreg", "-p", "IOUSB", "-l", "-w", "0"],
        ["system_profiler", "SPUSBDataType"],
        ["lsusb"],
    ]
    for command in commands:
        executable = shutil.which(command[0]) if "/" not in command[0] else command[0]
        if not executable or not os.path.exists(executable):
            log_only(f"--- {' '.join(command)} ---\n<command unavailable>\n")
            continue
        try:
            result = subprocess.run(
                command, capture_output=True, text=True, timeout=20, check=False
            )
            output = result.stdout + result.stderr
            if os.path.basename(command[0]) == "dfu-util":
                output = strip_dfu_banner(output)
            output = output.strip() or "<no output>"
            log_only(
                f"--- {' '.join(command)} (rc={result.returncode}) ---\n"
                f"{output}\n"
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            log_only(f"--- {' '.join(command)} ---\nERROR: {error}\n")


def flash_firmware(dfu_util):
    print(f"==> Flashing {FIRMWARE}...")
    result = subprocess.run(
        [
            dfu_util,
            "-d", DFU_USB_ID,
            "-a", "0",
            "-s", "0x08000000:leave",
            "-D", FIRMWARE,
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    sys.stdout.write(strip_dfu_banner(result.stdout))
    sys.stderr.write(strip_dfu_banner(result.stderr))

    # Some ROM bootloader versions disconnect during the final GET_STATUS and
    # make dfu-util return non-zero even though the download succeeded.
    downloaded = "File downloaded successfully" in result.stdout
    if result.returncode != 0 and not downloaded:
        sys.exit("ERROR: DFU flashing failed")


def wait_for_cdc():
    print("==> Waiting for application USB CDC", end="", flush=True)
    deadline = time.monotonic() + CDC_TIMEOUT
    while time.monotonic() < deadline:
        port = find_port()
        if port:
            print(f" found at {port}")
            return
        print(".", end="", flush=True)
        time.sleep(0.5)
    sys.exit("\nERROR: application USB CDC port did not return; use ST-Link to recover")


def main():
    arguments = sys.argv[1:]
    forced_port = arguments[0] if arguments else None
    if len(arguments) > 1:
        sys.exit("Usage: python3 usb_upload.py [port]")

    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    start_logging()
    print(f"==> Platform: {sys.platform}")
    print(f"==> Python: {sys.version.split()[0]}")
    print(f"==> Initial CDC ports: {find_ports() or '<none>'}")

    dfu_util = find_tool(
        "dfu-util", os.path.join("packages", "tool-dfuutil", "bin", "dfu-util")
    )
    if not dfu_util:
        sys.exit("ERROR: dfu-util is missing; on macOS run: brew install dfu-util")
    print(f"==> dfu-util: {dfu_util}")

    pio = find_tool("pio", os.path.join("penv", "bin", "pio"))
    if not pio:
        sys.exit("ERROR: PlatformIO pio executable was not found")
    print(f"==> PlatformIO: {pio}")
    build_firmware(pio)

    if not os.path.isfile(FIRMWARE):
        sys.exit(f"ERROR: build did not create {FIRMWARE}")

    if dfu_is_present(dfu_util):
        print("==> Device is already in DFU mode")
    else:
        request_dfu(forced_port)
        wait_for_dfu(dfu_util)

    flash_firmware(dfu_util)
    wait_for_cdc()
    print("==> USB firmware update completed")


if __name__ == "__main__":
    main()
