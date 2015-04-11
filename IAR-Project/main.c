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

#define CHECK_MSEC    10
#define PRESS_MSEC    40
#define RELEASE_MSEC 100
#define TIMEOUT     6000 /* in CHECK_MSEC periods */

volatile bool pwm0 = false, pwm1 = false;

bool debounce(bool state, bool read, uint16_t* count);

int main(void)
{
  // Stop WDT
  WDTCTL = WDTPW | WDTHOLD; // Stop WDT
  BCSCTL1 = DIVA_2; // ACLK /= 4
  BCSCTL2 = SELM_0; // MCLK = SMCLK = DCO
  BCSCTL3 = LFXT1S_2; // LFXT1 = VLO (ACLK = 3kHz)

  // Set up I/O (buttons as interrupt, LEDs off)
  P2SEL = 0; // Set crystal I/O to GPIO
  P1REN = (unsigned char)~(PLATE0 | PLATE1); // All pin pull up/down, except ouputs
  P2REN = P2MAP; // Also on port 2
  P1DIR = (unsigned char)~(BUTTON0 | BUTTON1); // disable all input, except inputs
  P2DIR = P2MAP; // Also on port 2
  P1OUT = BUTTON0 | BUTTON1; // pull up button inputs
  P1IES = BUTTON0 | BUTTON1; // high->low transition
  P1IFG = 0; // clear interrupt flags
  P1IE = BUTTON0 | BUTTON1; // enable interrupts

  // Set up timer for 10ms interval and CCR1 for 30% duty cycle
  TACCR0 = (10 * 3) - 1; // 10ms
  TACCR1 = ( 1 * 3) - 1; //  1ms
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
  P1IFG = 0;
  // Leave LPM4
  __bic_SR_register_on_exit(OSCOFF); // Enable ACLK after ISR (LPM4 -> LPM3)
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  TACCTL1 &= ~CCIFG; // clear interrupt flag
  if (!(pwm0 || pwm1)) return;
  switch(TACCR1)
  {
    case (1 * 3) - 1:
      if (pwm0) P1OUT |= PLATE0;
      TACCR1 = (4 * 3) - 1;
      break;
    case (4 * 3) - 1:
      P1OUT &= ~PLATE0;
      TACCR1 = (6 * 3) - 1;
      break;
    case (6 * 3) - 1:
      if (pwm1) P1OUT |= PLATE1;
      TACCR1 = (9 * 3) - 1;
      break;
    case (9 * 3) - 1:
      P1OUT &= ~PLATE1;
      TACCR1 = (1 * 3) - 1;
      break;
    default:
      TAR = 0;
      break;
  }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
  static bool state0 = false, state1 = false;
  static uint16_t count0 = 0, count1 = 0;
  static uint16_t timeout;

  TACCTL0 &= ~CCIFG; // clear interrupt flag
  // Handle button 0
  if (debounce(state0, P1IN & BUTTON0, &count0))
  {
    state0 = !state0;
    if (!state0) pwm0 = !pwm0;
    timeout = TIMEOUT;
  }
  // Handle button 1
  if (debounce(state1, P1IN & BUTTON1, &count1))
  {
    state1 = !state1;
    if (!state1) pwm1 = !pwm1;
    timeout = TIMEOUT;
  }
  // Handle timeout
  if (--timeout == 0)
  {
    pwm0 = pwm1 = false;
    P1OUT &= ~(PLATE0 | PLATE1);
  }
  // Handle low power mode
  if (!pwm0 && !pwm1 && state0 && state1 && ((P1IN & (BUTTON0 | BUTTON1)) == (BUTTON0 | BUTTON1)))
  {
    // if plates off and buttons up
    // Enter LPM4
    __bis_SR_register_on_exit(OSCOFF); // Disable ACLK after ISR (LPM3 -> LPM4)
  }
}

bool debounce(bool state, bool read, uint16_t* count)
{
  if (read == state)
  {
    *count = !state?(RELEASE_MSEC / CHECK_MSEC):(PRESS_MSEC / CHECK_MSEC);
  }
  else
  {
    if ((--*count) + 1 == 0)
    {
      *count = !read?(RELEASE_MSEC / CHECK_MSEC):(PRESS_MSEC / CHECK_MSEC);
      return true;
    }
  }
  return false;
}