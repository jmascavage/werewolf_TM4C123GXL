/*
 *  ======== EK_TM4C123GXL.c ========
 *  This file is responsible for setting up the board specific items for the
 *  EK_TM4C123GXL board.
 */

#include <stdint.h>
#include <stdbool.h>

#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_gpio.h>

#include <driverlib/gpio.h>
#include <driverlib/i2c.h>
#include <driverlib/pin_map.h>
#include <driverlib/pwm.h>
#include <driverlib/ssi.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>
#include <driverlib/udma.h>

#include "EK_TM4C123GXL.h"

#ifndef TI_DRIVERS_UART_DMA
#define TI_DRIVERS_UART_DMA 0
#endif

/*
 *  =============================== DMA ===============================
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(dmaControlTable, 1024)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=1024
#elif defined(__GNUC__)
__attribute__ ((aligned (1024)))
#endif
static tDMAControlTable dmaControlTable[32];
static bool dmaInitialized = false;

/* Hwi_Struct used in the initDMA Hwi_construct call */
static Hwi_Struct dmaHwiStruct;

/*
 *  ======== dmaErrorHwi ========
 */
static Void dmaErrorHwi(UArg arg)
{
    System_printf("DMA error code: %d\n", uDMAErrorStatusGet());
    uDMAErrorStatusClear();
    System_abort("DMA error!!");
}

/*
 *  ======== EK_TM4C123GXL_initDMA ========
 */
void EK_TM4C123GXL_initDMA(void)
{
    Error_Block eb;
    Hwi_Params  hwiParams;

    if (!dmaInitialized) {
        Error_init(&eb);
        Hwi_Params_init(&hwiParams);
        Hwi_construct(&(dmaHwiStruct), INT_UDMAERR, dmaErrorHwi,
                      &hwiParams, &eb);
        if (Error_check(&eb)) {
            System_abort("Couldn't construct DMA error hwi");
        }

        SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
        uDMAEnable();
        uDMAControlBaseSet(dmaControlTable);

        dmaInitialized = true;
    }
}

/*
 *  =============================== General ===============================
 */
/*
 *  ======== EK_TM4C123GXL_initGeneral ========
 */
void EK_TM4C123GXL_initGeneral(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
}

/*
 *  =============================== GPIO ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(GPIOTiva_config, ".const:GPIOTiva_config")
#endif

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOTiva.h>

/*
 * Array of Pin configurations
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in EK_TM4C123GXL.h
 * NOTE: Pins not used for interrupts should be placed at the end of the
 *       array.  Callback entries can be omitted from callbacks array to
 *       reduce memory usage.
 */
GPIO_PinConfig gpioPinConfigs[] = {
    /* Input pins */
    /* EK_TM4C123GXL_GPIO_PB2 */
    GPIOTiva_PB_2 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_RISING, //Distance Sensor Echo

    /* Output pins */
    /* EK_TM4C123GXL_LED_BLUE */
    GPIOTiva_PF_2 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_HIGH | GPIO_CFG_OUT_LOW,
    /* EK_TM4C123GXL_PB7 */
    GPIOTiva_PB_7 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_LOW, //Distance Sensor Trigger
    /* EK_TM4C123GXL_PE1 */
    GPIOTiva_PE_1 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_LOW, //transistor gate pin
    /* EK_TM4C123GXL_PE1 */
    GPIOTiva_PE_2 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_LOW, //breathing pin
    /* EK_TM4C123GXL_PE1 */
    GPIOTiva_PE_3 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_LOW, //howling pin
};

/*
 * Array of callback function pointers
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in EK_TM4C123GXL.h
 * NOTE: Pins not used for interrupts can be omitted from callbacks array to
 *       reduce memory usage (if placed at end of gpioPinConfigs array).
 */
GPIO_CallbackFxn gpioCallbackFunctions[] = {
};

/* The device-specific GPIO_config structure */
const GPIOTiva_Config GPIOTiva_config = {
    .pinConfigs = (GPIO_PinConfig *)gpioPinConfigs,
    .callbacks = (GPIO_CallbackFxn *)gpioCallbackFunctions,
    .numberOfPinConfigs = sizeof(gpioPinConfigs)/sizeof(GPIO_PinConfig),
    .numberOfCallbacks = sizeof(gpioCallbackFunctions)/sizeof(GPIO_CallbackFxn),
    .intPriority = (~0)
};

/*
 *  ======== EK_TM4C123GXL_initGPIO ========
 */
void EK_TM4C123GXL_initGPIO(void)
{
    /* Initialize peripheral and pins */
    GPIO_init();
}



/*
 *  =============================== PWM ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(PWM_config, ".const:PWM_config")
#pragma DATA_SECTION(pwmTivaHWAttrs, ".const:pwmTivaHWAttrs")
#endif

#include <ti/drivers/PWM.h>
#include <ti/drivers/pwm/PWMTiva.h>

PWMTiva_Object pwmTivaObjects[EK_TM4C123GXL_PWMCOUNT];

const PWMTiva_HWAttrs pwmTivaHWAttrs[EK_TM4C123GXL_PWMCOUNT] = {
    {
        .baseAddr = PWM0_BASE,
        .pwmOutput = PWM_OUT_0, //maps to M0PWM0 (PWM0_BASE output PWM_OUT_0) - which is PB6
        .pwmGenOpts = PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_DBG_RUN
    },
    {
        .baseAddr = PWM0_BASE,
        .pwmOutput = PWM_OUT_2, //maps to M0PWM2 (PWM0_BASE output PWM_OUT_2) - which is PB4
        .pwmGenOpts = PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_DBG_RUN
    },
    {
        .baseAddr = PWM0_BASE,
        .pwmOutput = PWM_OUT_3, //maps to M0PWM3 (PWM0_BASE output PWM_OUT_3) - which is PB5
        .pwmGenOpts = PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_DBG_RUN
    }

};

const PWM_Config PWM_config[] = {
    {
        .fxnTablePtr = &PWMTiva_fxnTable, //********************** JJM converted - added this entry for PB6
        .object = &pwmTivaObjects[0],
        .hwAttrs = &pwmTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &PWMTiva_fxnTable, //********************** JJM converted - added this entry for PB4
        .object = &pwmTivaObjects[1],
        .hwAttrs = &pwmTivaHWAttrs[1]
    },
    {
        .fxnTablePtr = &PWMTiva_fxnTable, //********************** JJM converted - added this entry for PB5
        .object = &pwmTivaObjects[2],
        .hwAttrs = &pwmTivaHWAttrs[2]
    },
    {NULL, NULL, NULL}
};

/*
 *  ======== EK_TM4C123GXL_initPWM ========
 */
void EK_TM4C123GXL_initPWM(void)
{
    /* Enable PWM peripherals */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);//********************** JJM converted
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

    /*
     * Enable PWM output on GPIO pins.
     */
    GPIOPinConfigure(GPIO_PB6_M0PWM0);              // PB6
    GPIOPinTypePWM(GPIO_PORTB_BASE, GPIO_PIN_6);    // PB6
    GPIOPinConfigure(GPIO_PB4_M0PWM2);              // PB4
    GPIOPinTypePWM(GPIO_PORTB_BASE, GPIO_PIN_4);    // PB4
    GPIOPinConfigure(GPIO_PB5_M0PWM3);              // PB5
    GPIOPinTypePWM(GPIO_PORTB_BASE, GPIO_PIN_5);    // PB5

    PWM_init();
}

