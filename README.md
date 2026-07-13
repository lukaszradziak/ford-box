# ford-box

This project was developed and tested for a 2017 Ford Mondeo Mk5 using the
CGEA electrical architecture.

- **HS/CAN1 @ 500000 bps**;
- MCU: PB8/PB9, STB=PB3;
- transceiver: U4;
- OBD: piny 3/11.

## hardware

- red UCDS from aliexpress
- st-link v2

## build and upload (ST-Link)

```bash
pio run -t upload
```

## monitor (RTT)

```bash
python3 rtt_monitor.py
```

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