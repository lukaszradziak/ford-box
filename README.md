# ford-box

- **HS/CAN1 @ 500000 bps**;
- MCU: PB8/PB9, STB=PB3;
- transceiver: U4;
- OBD: piny 3/11.

## build & upload (ST-Link)

```bash
pio run -t upload
```

## monitor (RTT)

```bash
python3 rtt_monitor.py
```
