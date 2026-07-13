# ford-box

This project was developed and tested for a 2017 Ford Mondeo Mk5 using the
CGEA electrical architecture.

- **HS/CAN1 @ 500000 bps**;
- MCU: PB8/PB9, STB=PB3;
- transceiver: U4;
- OBD: pins 3/11.

## hardware

- red UCDS from aliexpress
- st-link v2

## shortcuts

```bash
pio run -t upload
```

```bash
python3 usb_upload.py
```

```bash
python3 usb_monitor.py
```

```bash
python3 usb_upload.py && python3 usb_monitor.py
```

## build and upload (ST-Link)

Install PlatformIO, connect ST-Link and flash the firmware:

```bash
pio run -t upload
```

## USB serial monitor

ST-Link is used only for flashing. Runtime output and commands use the board's
USB CDC port (PA11/PA12).

Install the Python dependency once:

```bash
pip3 install -r requirements.txt
```

After flashing, ST-Link may be disconnected. Connect the board's USB port with
a data-capable USB cable and start the monitor:

```bash
python3 usb_monitor.py
```

The monitor automatically detects `/dev/cu.usbmodem*` on macOS (or
`/dev/ttyACM*` on Linux), reconnects after a USB disconnect and saves output to
`logs/usb_<date>_<time>.log`. A port can also be selected explicitly:

```bash
python3 usb_monitor.py /dev/cu.usbmodemXXXXXXXX 115200
```

Press `Ctrl+C` to close the monitor.

Commands are handled immediately, without pressing Enter:

| Command | Action                                       |
|---------|----------------------------------------------|
| `h`     | Show help and the current transmission state |
| `1`     | Toggle periodic `0x2B4` transmission ON/OFF  |
| `u`     | Reboot into the factory USB DFU bootloader   |

CAN logger and filter commands are completed with Enter:

| Command                   | Action                                               |
|---------------------------|------------------------------------------------------|
| `l.can1`                  | Log CAN1, OBD 3/11 at 500 kbps                       |
| `l.can2`                  | Log CAN2, OBD 6/14 at 500 kbps                       |
| `l.can3`                  | Log CAN3, OBD 1/8 at 125 kbps                        |
| `l` / `l.off`             | Stop logging                                         |
| `f.2b4`                   | Accept only standard CAN ID `0x2B4`                  |
| `f.200.2ff`               | Accept the inclusive standard-ID range `0x200-0x2FF` |
| `f`                       | Clear the filter and accept all IDs                  |
| `q.7e0.0322f40b00000000`  | Send standard ID `0x7E0` with eight data bytes       |

Only one logger bus is selected at a time. CAN2 and CAN3 are two pin mappings
of the STM32F105's second bxCAN controller and cannot operate simultaneously.
The filter may be configured before or after starting a logger and is retained
when switching buses or stopping the logger. It uses the bxCAN hardware filter
banks, with an exact software check when a range cannot fit those banks.

The generic test-frame syntax is `q.<standard-id>.<data-hex>`. It accepts a
standard 11-bit CAN ID and from one to eight complete data bytes. A logger must
be active because its selected CAN bus is also used for transmission. The
command reports `queued` when the frame enters a CAN transmit mailbox; this
does not by itself guarantee that another node acknowledged it.

Starting a logger automatically disables periodic `0x2B4` transmission. The
`1` command cannot enable transmission while a logger is active. Stopping the
logger leaves transmission disabled; press `1` explicitly to resume it.

Received frames use the same format as `ucds-logger`:

```text
<ID hex> <byte0> <byte1> ... <byteN>
```

For example: `2B4 10 45 3B 01 01 01 DA 31`.

The periodic transmission is enabled by default. Disabling it also pauses the
mock vehicle-state updates. After enabling it again, the next record is sent
after `DATA_INTERVAL_MS` (currently 1000 ms).

## Web CAN logger

[`logger.html`](logger.html) is a standalone, dependency-free Web Serial UI for
the CAN logger. It works in Chromium-based browsers with Web Serial support,
such as Google Chrome and Microsoft Edge. Safari and Firefox do not currently
provide this API.

Close `usb_monitor.py` before using the web logger because only one application
can keep the USB CDC port open. Open `logger.html` in a supported browser and:

1. click **Connect USB** and select the UCDS USB serial port;
2. select CAN1, CAN2 or CAN3;
3. click **Start logger**;
4. optionally select an exact ID or an inclusive ID range and click
   **Apply filter**;
5. optionally select a predefined test request, edit its CAN ID or data and
   click **Send frame**;
6. use **Stop logger** to leave logger mode.

**Pause vehicle data** disables the periodic simulated vehicle-state output;
the same button changes to **Resume vehicle data** when transmission is off.
It is disabled while the CAN logger is active because logger mode already
pauses that transmission automatically.

The predefined test requests come from the reverse engineering notes for
`ori.bin`. They include boost pressure, catalyst/exhaust temperature, engine
temperatures, voltage, load, TPMS and several other UDS DIDs. Presets only fill
the editable CAN ID and DATA HEX fields; arbitrary standard frames can be
entered manually. Responses are still controlled by the selected logger
filter, so make sure it includes the expected response ID (for example `0x7E8`
for requests sent to `0x7E0`).

Boost requires both the `F433` barometric-pressure request and the `F40B`
absolute-intake-pressure request. Multi-frame responses require sending the
provided ISO-TP Flow Control frame after their First Frame. Custom frames are
transmitted exactly as entered and may affect vehicle modules; use them only on
a stationary vehicle or bench setup when their meaning is known.

The filter accepts standard 11-bit hexadecimal CAN IDs from `000` to `7FF`.
Select **All IDs** to clear an existing filter. The text area shows the complete
USB output, including firmware status messages and received CAN frames. **Clear
output** only clears the browser text area and does not change the device state.

If the browser does not allow Web Serial from a directly opened local file,
serve the same single file locally from the project directory:

```bash
python3 -m http.server 8000
```

Then open `http://localhost:8000/logger.html`.

## Firmware update over USB

The first firmware installation still requires ST-Link. Later versions can be
built and installed through the same USB connector used by the serial monitor.
The updater uses the factory USB DFU bootloader built into STM32F105.
This board has factory ROM bootloader v2.2. Because `BOOT0` is permanently tied
low and VBUS is already present when the application requests an update, the
firmware uses a delayed USB D+ reconnect (TIM2 + DMA) while entering the ROM.
DFU enumeration normally takes about 13 seconds on the tested board.

Install `dfu-util` on macOS:

```bash
brew install dfu-util
```

Close `usb_monitor.py`, leave the board connected over USB and run:

```bash
python3 usb_upload.py
```

The updater prints DFU/CDC state every 0.5 seconds and writes a detailed log to
`logs/usb_upload_<date>_<time>.log`. On a timeout, the log also contains filtered
`dfu-util -l` output plus raw `ioreg` and `system_profiler` USB diagnostics.

The script performs the complete update:

1. builds the current PlatformIO project;
2. sends the `u` command through USB CDC;
3. waits for the STM32 DFU device (`0483:df11`);
4. writes the new firmware at `0x08000000`;
5. waits for the application USB serial port to return.

If the application is already in DFU mode, the updater detects it and skips
the CDC command. ST-Link remains the recovery method if an update is
interrupted or an invalid firmware image cannot start.

## OBD

| Element   | MCU pins           | OBD pins | Speed    |
|-----------|--------------------|----------|----------|
| CAN1 / U4 | PB8/PB9, STB PB3   | 3/11     | 500 kbps |
| CAN2 / U3 | PB12/PB13, STB PB7 | 6/14     | 500 kbps |
| CAN3 / U2 | PB5/PB6, STB PB4   | 1/8      | 125 kbps |

| Pin OBD-II | Signal            | Role on board |
|-----------:|-------------------|---------------|
|          1 | `CAN3H`           | CAN High 3    |
|          3 | `CAN1H`           | CAN High 1    |
|          4 | `GND`             | ground        |
|          5 | `GND`             | ground        |
|          6 | `CAN2H`           | CAN High 2    |
|          8 | `CAN3L`           | CAN Low 3     |
|         11 | `CAN1L`           | CAN Low 1     |
|         14 | `CAN2L`           | CAN Low 2     |
|         16 | `+12V`            | power         |
