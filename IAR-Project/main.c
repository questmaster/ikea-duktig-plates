/*
 * This code rebuilds the behaveior of the IKEA toy kitchen plates. You can use 
 * the MSP430 with this code to replace the used microcontroller as a energy 
 * efficient alternative.
 *
 * Code modified by Questmaster. 
 *
 * Original author: roadrunner84
 * Original code see forum: http://forum.43oh.com/topic/3425-nano-project-fix-an-ikea-toy/ 
 */

#include <msp430.h>
#include <stdint.h>
#ifndef __cplusplus__
#include <stdbool.h>
#endif /* __cplusplus__ */

#define P2MAP (BIT6 | BIT7) /* set to 0xFF for 20-pin device */
#define BUTTON0 BIT4
#define BUTTON1 BIT5
#define PLATE0  BIT6
#define PLATE1  BIT7

const uint16_t CHECK_MSEC   =    10u;
const uint16_t PRESS_MSEC   =    40u;
const uint16_t RELEASE_MSEC =   100u;
const uint16_t TIMEOUT      = 60000u; /* in CHECK_MSEC periods */

#define TIMINGS_10MS (uint8_t)((10 * 3) - 1)
#define TIMINGS_1MS  (uint8_t)(( 1 * 3) - 1)
#define TIMINGS_4MS  (uint8_t)(( 4 * 3) - 1)
#define TIMINGS_6MS  (uint8_t)(( 6 * 3) - 1)
#define TIMINGS_9MS  (uint8_t)(( 9 * 3) - 1)

volatile bool pwm0 = false, pwm1 = false;

__interrupt void PORT1_ISR(void);
__interrupt void TIMER0_A1_ISR(void);
__interrupt void TIMER0_A0_ISR(void);
static bool debounce(bool state, bool read, uint16_t* count);

int main(void)
{
  // Stop WDT
  WDTCTL = WDTPW | WDTHOLD; // Stop WDT
  BCSCTL1 = (uint8_t) DIVA_2; // ACLK /= 4
  BCSCTL2 = (uint8_t) SELM_0; // MCLK = SMCLK = DCO
  BCSCTL3 = (uint8_t) LFXT1S_2; // LFXT1 = VLO (ACLK = 3kHz)

  // Set up I/O (buttons as interrupt, LEDs off)
  P2SEL = 0u; // Set crystal I/O to GPIO
  P1REN = (uint8_t)~(PLATE0 | PLATE1); // All pin pull up/down, except ouputs
  P2REN = P2MAP; // Also on port 2
  P1DIR = (uint8_t)~(BUTTON0 | BUTTON1); // disable all input, except inputs
  P2DIR = P2MAP; // Also on port 2
  P1OUT = BUTTON0 | BUTTON1; // pull up button inputs
  P1IES = BUTTON0 | BUTTON1; // high->low transition
  P1IFG = 0u; // clear interrupt flags
  P1IE = BUTTON0 | BUTTON1; // enable interrupts

  // Set up timer for 10ms interval and CCR1 for 30% duty cycle
  TACCR0 = TIMINGS_10MS; // 10ms
  TACCR1 = TIMINGS_1MS; //  1ms
  TACCTL1 = CCIE; // Set TA0.1 to 30% duty cycle 10ms PWM
  TACCTL0 = CCIE;
  TACTL = TASSEL_1 | ID_0 | MC_1 | TACLR;// | TAIE; // Enable 3kHz timer interrupt

  __enable_interrupt();
  LPM3;
  __no_operation();
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
  P1IFG = 0u;
  // Leave LPM4
  __bic_SR_register_on_exit(OSCOFF); // Enable ACLK after ISR (LPM4 -> LPM3)
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  TACCTL1 &= ~CCIFG; // clear interrupt flag
    
  if (pwm0 || pwm1) {
    if (TACCR1 == TIMINGS_1MS) {
      if (pwm0) {
        P1OUT |= PLATE0;
      }
      TACCR1 = TIMINGS_4MS;
        
    } else if (TACCR1 == TIMINGS_4MS) {
      P1OUT &= ~PLATE0;
      TACCR1 = TIMINGS_6MS;
        
    } else if (TACCR1 == TIMINGS_6MS) {
      if (pwm1) {
        P1OUT |= PLATE1;
      }
      TACCR1 = TIMINGS_9MS;

    } else if (TACCR1 == TIMINGS_9MS) {
      P1OUT &= ~PLATE1;
      TACCR1 = TIMINGS_1MS;

    } else {        
      TAR = 0u;
    }
  }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
  static bool state0 = false, state1 = false;
  static uint16_t count0 = 0u, count1 = 0u;
  static uint16_t timeout;

  TACCTL0 &= ~CCIFG; // clear interrupt flag
  
  // Handle button 0
  if (debounce(state0, (bool)(P1IN & BUTTON0), &count0))
  {
    state0 = !state0;
    if (!state0) {
      pwm0 = !pwm0;
    }
    timeout = TIMEOUT;
  }
  
  // Handle button 1
  if (debounce(state1, (bool)(P1IN & BUTTON1), &count1))
  {
    state1 = !state1;
    if (!state1) {
      pwm1 = !pwm1;
    }
    timeout = TIMEOUT;
  }
  
  // Handle timeout
  if (--timeout == 0u)
  {
    pwm0 = pwm1 = false;
    P1OUT &= ~(PLATE0 | PLATE1);
  }
  
  // Handle low power mode
  if ((!pwm0) && (!pwm1) 
      && state0 && state1 
      && ((P1IN & (BUTTON0 | BUTTON1)) == (BUTTON0 | BUTTON1))) {
    // if plates off and buttons up
    // Enter LPM4
    __bis_SR_register_on_exit(OSCOFF); // Disable ACLK after ISR (LPM3 -> LPM4)
  }
}

static bool debounce(bool state, bool read, uint16_t* count)
{
  bool bResult = false;
  
  if (read == state)
  {
    *count = (uint16_t)(!state?(RELEASE_MSEC / CHECK_MSEC):(PRESS_MSEC / CHECK_MSEC));
  }
  else
  {
    if ((--*count) + 1u == 0u)
    {
      *count = (uint16_t)(!read?(RELEASE_MSEC / CHECK_MSEC):(PRESS_MSEC / CHECK_MSEC));
      bResult = true;
    }
  }
  return bResult;
}
