# STM32F105 ROM v2.2 USB DFU notes

These notes document the factory ROM path verified on the STM32F105RBT6 used
by this board. They are intentionally kept with the project so the ROM analysis
does not need to be repeated.

## Identification

- System memory base: `0x1FFFB000`
- Initial MSP: `0x20000FF0`
- Reset vector stored at `0x1FFFB004`: `0x1FFFE9C1`
- Identified bootloader revision: v2.2
- USB DFU VID:PID: `0483:df11`
- Internal Flash DFU alt setting: `@Internal Flash /0x08000000/128*002Kg`

## Relevant ROM flow

| Address | Role |
|---|---|
| `0x1FFFE930` | ROM runtime/data initialization; returns |
| `0x1FFFE5F4` | top-level transport dispatcher |
| `0x1FFFE668` | transport detection; returns `3` for USB |
| `0x1FFFE618` | USB branch immediately after detection |
| `0x1FFFC9AA` | blocking USB/HSE/DFU routine |
| `0x1FFFCA80` | USB initialization used from the USB path |
| `0x1FFFE9D1` | OTG FS interrupt vector/handler entry |
| `0x1FFFBBA0` | ROM-generated system reset helper |

The ROM writes `SCB->VTOR = 0x1FFFB000` during transport detection. The USB
path uses interrupts. Global interrupts therefore have to be enabled before
entering it.

## Failure mechanism found on this board

The application USB CDC port and ROM DFU use the same OTG FS peripheral. A
normal software jump successfully reached ROM and selected USB, but the host
did not send a USB reset after ROM initialized the device. Observed state:

- CPU repeatedly in `0x1FFFC9AA..0x1FFFCA48`;
- `DCTL.SDIS = 0` (logically connected);
- `DSTS = 0x7` (suspended/not enumerated);
- VBUS/PA9 high;
- no `USBRST`/`ENUMDNE` event reaching the ROM handler;
- no `0483:df11` device on macOS.

Toggling `DCTL.SDIS` manually with ST-Link **after** ROM USB initialization
immediately made `dfu-util -l` report both DFU alternate settings. Toggling it
before `0x1FFFC9AA` was insufficient because that routine initializes the USB
core again.

## Implemented entry sequence

For the known v2.2 reset vector only, `DfuBootloader.cpp` performs this flow:

1. stop CDC and deinitialize application peripherals/clocks;
2. clear SysTick and pending NVIC interrupts;
3. call ROM runtime initialization at `0x1FFFE931`;
4. call ROM transport detection at `0x1FFFE669`;
5. require return value `3` (USB);
6. arm TIM2 update DMA on DMA1 channel 2;
7. enter the blocking ROM DFU routine at `0x1FFFC9AB`;
8. DMA writes `USB_OTG_DCTL_SDIS`, then `0`, after ROM is already running.

The tested device appears as DFU after approximately 13 seconds. Two complete,
consecutive runs of `python3 usb_upload.py` erased and programmed Flash, issued
DFU Leave, and observed the application CDC port return. ST-Link was not used
for the second run.

This mechanism deliberately checks the exact v2.2 reset vector because the
internal routine addresses are ROM-revision-specific. Unknown ROM revisions use
the generic system-memory reset-vector jump instead.
