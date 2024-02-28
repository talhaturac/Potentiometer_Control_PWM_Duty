//#############################################################################
//                                                                            #
//   FILE:   potentiometer_control_pwm_duty.c                                 #
//                                                                            #
//   CODES OF THE STUDY                                                       #
//                                                                            #
//   STUDY NAME : PWM duty cycle configuration using potentiometer            #
//   MAIN GOAL  : ADC control & PWM generation and adjustments                #
//   CONTACT    : tturac.turk@gazi.edu.tr  &  linkedin.com/in/talhaturacturk  #
//                                                                            #
//   This work is an empty project setup for Driverlib development.           #
//                                                                            #
//   Watch Expression                                                         #
//   adcAResult0 = Digital display of the voltage at pin A0                   #
//                                                                            #
//   Configure desired EPWM frequency & duty                                  #
//   ePWM2A is on GPIO2                                                       #
//   ePWM2B is on GPIO3                                                       #
//                                                                            #
//#############################################################################

// Included Files
#include "driverlib.h"
#include "device.h"

// Function Prototypes
void    ADC_init();
void    ASYSCTL_init();
void    INTERRUPT_init();
void    PinMux_init();
void    TriggerEPWM_init();
void    EPWM2_init();
void    change_duty_ratio();
extern __interrupt void adcA1ISR(void);

// GLOBALS
uint16_t   adcAResult0;
float32_t  dutyRatio;
float32_t  d_value = 0;


// Main
void main(void)
{
    Device_init();                  // Initialize device clock and peripherals
    Device_initGPIO();              // Disable pin locks and enable internal pullups.
    Interrupt_initModule();         // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    Interrupt_initVectorTable();    // Initialize the PIE vector table with pointers to the shell Interrupt
                                    // Service Routines (ISR).

    // Set up the ADCA and ADCC and initialize the ADC SOC.
    // ADC Resolution - 12-bit, signal mode - single ended
    // ADCA SOC0, SOC1, SOC2 are configured to convert A0,
    // A1 and A2 channels with EPWM1SOCA as SOC trigger.
    // ADCC SOC0, SOC1, SOC2 are configured to convert C2,
    // C3, C4 channels with EPWM1SOCA as trigger

    EALLOW;
    PinMux_init();
    ASYSCTL_init();
    ADC_init();
    INTERRUPT_init();
    EDIS;

    TriggerEPWM_init();             // Configure EPWM1 ADC SOCA trigger

    EALLOW;
    EPWM2_init();                   // For this case just init GPIO pins for ePWM1
    EDIS;

    EINT;                           // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    ERTM;

    while(1)
    {
        change_duty_ratio();
    }
}



//EPWM2 initialization
void EPWM2_init()
{
    // Globals

    // Reset PWM2 Peripheral
    SysCtl_resetPeripheral(SYSCTL_PERIPH_RES_EPWM2);

    // EPWM2 Pinmux
    GPIO_setPinConfig(GPIO_2_EPWM2_A);
    GPIO_setPinConfig(GPIO_3_EPWM2_B);

    // Disable sync(Freeze clock to PWM as well)
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // Configure phase PWM2
    EPWM_disablePhaseShiftLoad(EPWM2_BASE);
    EPWM_setPhaseShift(EPWM2_BASE, 0U);

    // ePWM1 SYNCO is generated on CTR=0
    EPWM_enableSyncOutPulseSource(EPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_CNTR_ZERO);

    // Enable sync and clock to PWM
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // Enable interrupts required for this example
    Interrupt_enable(INT_EPWM1);
}


void change_duty_ratio(void)
{
    for(;;)
    {
        float32_t one = 1;
        d_value = one*dutyRatio;

        // Configuring ePWM module for desired frequency and duty
        EPWM_SignalParams pwmSignal = {10000, d_value, d_value, true, DEVICE_SYSCLK_FREQ,
                                       EPWM_COUNTER_MODE_UP_DOWN, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1};

        EPWM_configureSignal(EPWM2_BASE, &pwmSignal);
        DEVICE_DELAY_US(1000);
    }
}


// Function to configure ePWM1 to generate the SOC.
void TriggerEPWM_init(void)
{
    EPWM_disableADCTrigger(EPWM1_BASE, EPWM_SOC_A);                            // Disable SOCA

    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_U_CMPA);   // Configure the SOC to occur on the first up-count event
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1);

    // Set the compare A value to 1000 and the period to 1999
    // Assuming ePWM clock is 100MHz, this would give 50kHz sampling
    // 50MHz ePWM clock would give 25kHz sampling, etc.
    // The sample rate can also be modulated by changing the ePWM period
    // directly (ensure that the compare A value is less than the period).

    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 1000);
    EPWM_setTimeBasePeriod(EPWM1_BASE, 1999);

    EPWM_setClockPrescaler(EPWM1_BASE,                                         // Set the local ePWM module clock divider to /1
                           EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);

    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);    // Freeze the counter

    // Start ePWM1, enabling SOCA and putting the counter in up-count mode
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP);
}


// ADC A Interrupt 1 ISR
__interrupt void adcA1ISR(void)
{
    // Store results
      adcAResult0 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    //adcAResult1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);   CAN BE USED IF MORE INPUT IS REQUIRED
    //adcAResult2 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2);   CAN BE USED IF MORE INPUT IS REQUIRED

    dutyRatio = adcAResult0 / 4096.0;                                         // Duty Cycle Rate

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);                     // Clear the interrupt flag

    if(true == ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))    // Check if overflow has occurred
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    }

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);     // Acknowledge the interrupt
}


void ADC_init()   // ADCA initialization
{                                                                           // ADC Initialization: Write ADC configurations and power up the ADC
    ADC_setOffsetTrimAll(ADC_REFERENCE_INTERNAL,ADC_REFERENCE_3_3V);        // Configures the ADC module's offset trim
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_2_0);                           // Configures the analog-to-digital converter module prescaler.
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);            // Sets the timing of the end-of-conversion pulse
    ADC_enableConverter(ADCA_BASE);                                         // Powers up the analog-to-digital converter core.
    DEVICE_DELAY_US(5000);                                                  // Delay for 1ms to allow ADC time to power up

                                                                            // SOC Configuration: Setup ADC EPWM channel and trigger settings
    ADC_disableBurstMode(ADCA_BASE);                                        // Disables SOC burst mode.
    ADC_setSOCPriority(ADCA_BASE, ADC_PRI_ALL_ROUND_ROBIN);                 // Sets the priority mode of the SOCs.


    // Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.     // Start of Conversion 0 Configuration
                                                                                            // SOC number      : 0
                                                                                            // Trigger         : ADC_TRIGGER_EPWM1_SOCA
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN0, 8U);    // Channel         : ADC_CH_ADCIN0
    ADC_setInterruptSOCTrigger(ADCA_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE);       // Sample Window   : 8 SYSCLK cycles
                                                                                            //  Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE

    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);                    //  ADC Interrupt 1 Configuration
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);                                        //  SOC/EOC number  : 0
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);                                   //  Interrupt Source: enabled
    ADC_disableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1);                                  //  Continuous Mode : disabled
}


void ASYSCTL_init()
{
    // asysctl initialization
    ASysCtl_disableTemperatureSensor();                                         // Disables the temperature sensor output to the ADC.
    ASysCtl_setAnalogReferenceInternal( ASYSCTL_VREFHIA | ASYSCTL_VREFHIC );    // Set the analog voltage reference selection to internal.
    ASysCtl_setAnalogReference1P65( ASYSCTL_VREFHIA | ASYSCTL_VREFHIC );        // Set the internal analog voltage reference selection to 1.65V.
}


void INTERRUPT_init()
{
    // Interrupt Settings for INT_ADCA1
    Interrupt_register(INT_ADCA1, &adcA1ISR);
    Interrupt_enable(INT_ADCA1);
}


// ANALOG -> Pinmux
void PinMux_init()
{
    // Analog PinMux for A0/C15
    GPIO_setPinConfig(GPIO_231_GPIO231);
    // AIO -> Analog mode selected
    GPIO_setAnalogMode(231, GPIO_ANALOG_ENABLED);
    // Analog PinMux for A1
    GPIO_setPinConfig(GPIO_232_GPIO232);
    // AIO -> Analog mode selected
    GPIO_setAnalogMode(232, GPIO_ANALOG_ENABLED);
}

// End of file
