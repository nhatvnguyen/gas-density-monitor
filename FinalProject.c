


/**
 * main.c
 */

#include <stdbool.h>
#include <stdint.h>
#include <inc/hw_memmap.h>
#include <stdio.h>
#include <driverlib/gpio.h>
#include <driverlib/sysctl.h>
#include <inc/hw_timer.h>
#include <inc/hw_gpio.h>
#include <driverlib/timer.h>
#include <driverlib/gpio.c>
#include <driverlib/systick.h>
#include <driverlib/adc.h>

#define NVIC_ST_CURRENT_R       (*((volatile uint32_t *)0xE000E018))   //Systick ccurrent value register
#define ADC0_DITHER        (*((volatile unsigned long *)0x40038038))  // For DITHER register for ADC0 is used for noise cancellation
#define ADC1_DITHER        (*((volatile unsigned long *)0x40039038))  // For DITHER register for ADC1 is used for noise cancellation

uint8_t i = 1;    // global variable used as a counter variable to switch among modes
uint8_t switch1_before; //To store the value of switch 1 before it is pressed to include it and test against it for the switch Debounce
uint8_t control = 2;  //a variable to store the control line of Port E, it will have the values 2,4 and 8 which are the values of pins 1,2 and 3 of Port E
uint32_t raw_temp;    //32 bit global variable used to store raw temperature data taken out of the ADC
uint32_t raw_pres;   //32 bit global variable used to store raw pressure data taken out of the ADC


// pre-declare some ISRs
void ModeSwitching_handler(void);


////////////////////////////////////////////// PORTS D,E,B & F Configurations//////////////////////////////////////////////

void PortD_Cnofig(void)   //Configuration of port D
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);//To enable clocking for port D
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 ); //Set pins 0 to 3 & 6 of Port D as output
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD); // 8 mA current for pins 0 to 3 of Port D
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_6, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
}

void PortE_Config(void)  //Configuration of port E
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);//To enable clocking for port E
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 ); //Set pins 1 to 3 of Port E as output
    GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD); //Configuring Port E ,not needed
    //GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_5, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_ANALOG);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);
}


void PortB_Config(void)    // Port B Configuration
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);//To enable clocking for port B
    GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_5);
    //GPIOPadConfigSet(GPIO_PORTB_BASE,GPIO_PIN_5, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_ANALOG);
}

void PortF_Config(void)    // Port F Configuration
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);//To enable clocking for port F
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4); //Configure Pin 4 as input
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU); //Configure pin4 to have a pull up resistor
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3); //Configure Pin 1,2,3 as output for the states
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_4MA,GPIO_PIN_TYPE_STD); //Configure pin1,2,3 to have drive leds
    switch1_before = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);
    GPIOIntRegister(GPIO_PORTF_BASE, (ModeSwitching_handler));  //To register the function to be call when the interrupt flag is raised (the switch has been pressed)
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_LOW_LEVEL ); //To set the type of the interrupt to be triggered with a falling edge signal
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_INT_PIN_4); //Enable the interrupt to watch for switch pressing
}
////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////// Switching between Display Modes ///////////////////////////

void ModeSwitching_handler(void)
{
    GPIOIntDisable(GPIO_PORTF_BASE, GPIO_INT_PIN_4);  //Disabling the interrupt as to let the program we are servicing that interrupt
        int switch1_now;
        switch1_now = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);
        if ((switch1_now != switch1_before) && (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4) == 0))
                            {  SysCtlDelay(1000000); //To count for switch debounce
                            i++;  //this will change the mode count and therefore change display mode accoordingly
                            if (i == 4) {i = 1;}
                            }

                            switch1_before = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_INT_PIN_4);  //RE-enabling the interrupt before leaving ISR
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






///////////////////Two functions for constant-like display where we switch between pins 1 to 3 of port E being as the control line /////////////////////////

void TimerFlashing_Handler(void)
{

    int bac; //A variable to hold 10s digit of the count
    int tr;  //A variable to hold the 100s digit of the count
    int csc; //A variable to hold the ones digits


   int actual_pres = raw_pres / 40.95;    //raw data are from 0 to 4095, this will change them from 0 to 100
   int actual_temp = raw_temp / 40.95;     //raw data are from 0 to 4095, this will change them from 0 to 100
   int temp_expected_temp = actual_temp - 40;   //To get the temperature adjusted in the range of -40C to 60C
   int expected_pres = (temp_expected_temp + 273) * 0.17065;  //To calculated the expected pressure for a given measured temperature
   int density = (actual_pres*100) / (expected_pres);   //To calculate the density
   int pres_ones, pres_tens, pres_hundred;   //variables to get the ones,tens and hundreds digits


   // Condition for the alarm
   if (density < 85) { GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_6, GPIO_PIN_6); }
   else if ((density > 90)) { GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_6, 0x0); }

    TimerIntClear(TIMER0_BASE,TIMER_TIMA_TIMEOUT);  //Clearing the interrupt before configuring it
    TimerIntDisable(TIMER0_BASE,TIMER_TIMA_TIMEOUT); //Disabling the interrupt while configuring it
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 , control);
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3 , 0);


    //Density Display Mode
    if (i == 1) {
        tr = density / 100;
        bac = (density % 100) / 10;
        csc = density % 10;
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2| GPIO_PIN_3, 0x8);
    if (control == 2){
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, csc);
        }
    else if (control == 4) {
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, bac);
        }
    else if (control == 8) {
       GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, tr);
       }
    control = control*2;
    if (control > 8) {control = 2;}
    }
    ///////////////////////


    // Pressure Display Mode
    if (i == 2) {
        pres_hundred = actual_pres / 100;
        pres_tens = (actual_pres % 100) / 10;
        pres_ones = actual_pres % 10;
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 ,0x4 );
        if (control == 2){
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, pres_ones);
                }
           else if (control == 4) {
               GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, pres_tens);
               }
           else if (control == 8) {
              GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, pres_hundred);
              }
           control = control*2;
           if (control > 8) {control = 2;}
           TimerEnable(TIMER1_BASE,TIMER_A);
            }
     /////////////////////////


    // Temperature Display Mode
    if (i == 3) {
        tr = actual_temp / 100;
        bac = (actual_temp % 100) / 10;
        csc = actual_temp % 10;
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 , 0x2);
        if (control == 2){
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, csc);
                }
           else if (control == 4) {
               GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, bac);
               }
           else if (control == 8) {
              GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, tr);
              }
           control = control*2;
           if (control > 8) {control = 2;}
           TimerEnable(TIMER1_BASE,TIMER_A);
            }
      //////////////////////////

    TimerIntEnable(TIMER0_BASE,TIMER_TIMA_TIMEOUT);
}
void TimerFlashing_Config(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); //To enable clock for Timer 1
    TimerDisable(TIMER0_BASE,TIMER_A);
    TimerConfigure(TIMER0_BASE,TIMER_CFG_A_PERIODIC); //To configure the timer mode as periodic
    TimerLoadSet(TIMER0_BASE,TIMER_A, 35000);
    TimerIntRegister(TIMER0_BASE,TIMER_A, (TimerFlashing_Handler));
    TimerIntEnable(TIMER0_BASE,TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE,TIMER_A);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////// Timer function and ISR to restore the Density Display mode after 5 seconds ///////////////////////////////////
void Timer5sDelay_Handler(void)
{

    TimerIntClear(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
    TimerIntDisable(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
    i = 1;
    TimerIntEnable(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
}

void Timer5sDelay_Config(void)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1); //To enable clock for Timer 1
    TimerDisable(TIMER1_BASE,TIMER_A);
    TimerConfigure(TIMER1_BASE,TIMER_CFG_A_PERIODIC); //To configure the timer mode as periodic
    TimerLoadSet(TIMER1_BASE,TIMER_A, 92000000);
   // TimerPrescaleSet(TIMER1_BASE,TIMER_A, 255);
    TimerIntRegister(TIMER1_BASE,TIMER_A, (Timer5sDelay_Handler));
    //IntPrioritySet(INT_TIMER1, 0xE0);
    TimerIntEnable(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////  ADC initialization & ISR handler to handle the data after the sampling is done  /////////////////////////
///////////////  (ADC0 is used for Temperature sensor conversion & ADC1 is used for Pressure sensor conversion)  ///////////////

void Temp_handler(void)  //ISR to handle temperature data after the ADC is done sampling
{

    ADCIntClear(ADC0_BASE,3);
    ADCIntDisable(ADC0_BASE,3);
    ADCSequenceDataGet(ADC0_BASE,3, &raw_temp);
    ADCProcessorTrigger(ADC0_BASE,3);
    ADCIntEnable(ADC0_BASE,3);
}


void Pres_handler(void)   //ISR to handle pressure data after the ADC is done sampling
{
    ADCIntClear(ADC1_BASE,3);
    ADCIntDisable(ADC1_BASE,3);
    ADCSequenceDataGet(ADC1_BASE,3, &raw_pres);
    ADCIntEnable(ADC1_BASE,3);
    ADCProcessorTrigger(ADC1_BASE,3);
}

void ADC0_TempConfig(void)  //ADC0 Configuration and initialization for temperature sensor
{
// ADC
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);  //clock for ADC zero
    //ADCSequenceDisable(ADC0_BASE,3);
    ADCHardwareOversampleConfigure(ADC0_BASE, 64);  //maximum oversampling
    ADC0_DITHER |= 0x0000004; // DITHER bit
    ADCSequenceConfigure(ADC0_BASE,3,ADC_TRIGGER_PROCESSOR,1);
    ADCSequenceStepConfigure(ADC0_BASE,3,0,ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH8);
    ADCSequenceEnable(ADC0_BASE,3);
    ADCProcessorTrigger(ADC0_BASE,3);
    ADCIntRegister(ADC0_BASE,3,(Temp_handler));
    ADCIntEnable(ADC0_BASE,3);
}

void ADC1_PresConfig(void)   //ADC1 Configuration and initialization for pressure sensor
{
// ADC
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);  //clock for ADC 1
    //ADCSequenceDisable(ADC0_BASE,3);
    ADCHardwareOversampleConfigure(ADC1_BASE, 64);  //maximum oversampling
    ADC1_DITHER |= 0x0000004; // DITHER bit
    ADCSequenceConfigure(ADC1_BASE,3,ADC_TRIGGER_PROCESSOR,2);
    ADCSequenceStepConfigure(ADC1_BASE,3,0,ADC_CTL_IE| ADC_CTL_END | ADC_CTL_CH11);
    ADCSequenceEnable(ADC1_BASE,3);
    ADCProcessorTrigger(ADC1_BASE,3);
    ADCIntRegister(ADC1_BASE,3,(Pres_handler));
    ADCIntEnable(ADC1_BASE,3);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





void main(void)    {

    SysCtlClockSet(SYSCTL_USE_OSC | SYSCTL_OSC_MAIN); //changing the clock source to MOSC

    PortD_Cnofig();
    PortE_Config();
    PortB_Config();
    TimerFlashing_Config();
    Timer5sDelay_Config();
    PortF_Config();

    ADC0_TempConfig();
    ADC1_PresConfig();


   while (1)
        {

        }

    }



