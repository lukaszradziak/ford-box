#include "DfuBootloader.h"
#include <stm32f1xx_hal.h>

// STM32F105/F107 system-memory bootloader base, documented in ST AN2606.
static constexpr uint32_t kBootloaderAddress = 0x1FFFB000UL;

// TIM2 update DMA writes these values to USB OTG DCTL after ROM has entered
// its USB path: first soft-disconnect, then reconnect. Keep this in Flash;
// ROM uses/overwrites the first 4 KiB of SRAM.
alignas(4) static const uint32_t kUsbReconnectSequence[] = {
    USB_OTG_DCTL_SDIS,
    0,
};

extern "C" __attribute__((used, noinline)) void dfuArmDelayedUsbReconnect() {
    // ROM's transport detector runs from a 24 MHz HSI clock. TIM2 generates an
    // update roughly every second (the exact interval can change when ROM
    // switches clocks, which is harmless here). DMA1 channel 2 is TIM2_UP on
    // STM32F105 and writes two 32-bit DCTL values without CPU involvement.
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;

    DMA1_Channel2->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF2 | DMA_IFCR_CTCIF2 |
                 DMA_IFCR_CHTIF2 | DMA_IFCR_CTEIF2;
    DMA1_Channel2->CNDTR = 2;
    DMA1_Channel2->CPAR = 0x50000804UL; // USB OTG FS device DCTL
    DMA1_Channel2->CMAR = (uint32_t)kUsbReconnectSequence;
    DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC |
                         DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 |
                         DMA_CCR_PL_1;

    TIM2->PSC = 23999;  // 24 MHz / 24000 = 1 kHz before ROM clock switch
    TIM2->ARR = 999;    // one DMA write per ~1 s
    TIM2->CNT = 0;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UDE;

    DMA1_Channel2->CCR |= DMA_CCR_EN;
    TIM2->CR1 = TIM_CR1_CEN;
    __DSB();
}

// ROM v2.2 normally calls these routines itself from 0x1FFFE5F4. We retain
// control only long enough to arm the delayed D+ reconnect between transport
// selection and the blocking DFU routine.
__attribute__((naked, noreturn)) static void enterDfuRomV22() {
    __asm volatile(
        "ldr r0, =0x20000FF0\n" // ROM's initial stack pointer
        "msr msp, r0\n"
        "ldr r3, =0x1FFFE931\n" // ROM runtime/data initialization
        "blx r3\n"
        "ldr r3, =0x1FFFE669\n" // transport detection, returns 3 for USB
        "blx r3\n"
        "cmp r0, #3\n"
        "bne 1f\n"
        "bl dfuArmDelayedUsbReconnect\n"
        "ldr r3, =0x1FFFC9AB\n" // blocking ROM USB DFU routine
        "blx r3\n"
        "1:\n"
        "ldr r0, =0xE000ED0C\n" // reset after DFU Leave or failed detect
        "ldr r1, =0x05FA0004\n"
        "str r1, [r0]\n"
        "dsb\n"
        "2: b 2b\n"
    );
}

uint32_t dfuBootloaderResetVector() {
    return *(volatile uint32_t *)(kBootloaderAddress + sizeof(uint32_t));
}

const char *dfuBootloaderVersion() {
    switch (dfuBootloaderResetVector()) {
        case 0x1FFFE945UL: return "2.0";
        case 0x1FFFE9A1UL: return "2.1";
        case 0x1FFFE9C1UL: return "2.2";
        default: return "unknown/1.0";
    }
}

void dfuRequestBootloader() {
    const uint32_t stack = *(volatile uint32_t *)kBootloaderAddress;
    const uint32_t resetVector = dfuBootloaderResetVector();

    // AN2606 requires a software jump to leave clocks, peripherals and pending
    // interrupts in their reset state. In particular, jumping after the normal
    // application startup left USB OTG and PLL state behind and ROM v2.2 never
    // selected DFU even though its reset handler was executing.
    __disable_irq();

    // USBD_DeInit() stops the CDC stack but does not guarantee that the OTG
    // core and its RX FIFO return to their hardware-reset state. ROM v2.2 can
    // otherwise inherit stale CDC packet status and never enumerate as DFU.
    __HAL_RCC_USB_OTG_FS_FORCE_RESET();
    __DSB();
    for (volatile uint32_t delay = 0; delay < 256; ++delay) {}
    __HAL_RCC_USB_OTG_FS_RELEASE_RESET();
    __DSB();

    HAL_DeInit();
    HAL_RCC_DeInit();

    // HAL_RCC_DeInit() calls HAL_InitTick(), therefore SysTick must be reset
    // after the HAL/RCC cleanup. ROM uses SysTick to identify the HSE
    // frequency; leaving the application's tick running makes it reject the
    // otherwise supported 8 MHz crystal and issue a software reset.
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    for (uint32_t bank = 0; bank < 8; ++bank) {
        NVIC->ICER[bank] = 0xFFFFFFFFUL;
        NVIC->ICPR[bank] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = kBootloaderAddress;
    __DSB();
    __ISB();

    // The F105 USB bootloader uses interrupts and does not unmask PRIMASK on
    // every path, so global interrupts must be enabled before entering ROM.
    __enable_irq();

    if (resetVector == 0x1FFFE9C1UL) {
        enterDfuRomV22();
    }

    __set_MSP(stack);
    __DSB();
    __ISB();
    __asm volatile("bx %0" : : "r"(resetVector));

    for (;;) {}
}
