/*
 *  ======== werewolf.c ========
 *  the full werewolf logic is in this file
 *
 *  this file uses abstraction references from Board.h to attempt to be board-agnostic.
 *  In theory one could swap out Board.h and the underlying board-specific files with ones
 *  for another board and the application should be possible to recompile and run.
 *
 *  Board.h is currently tied to EK_TM4C123GXL.h - which provides pin names and other reference values.
 *  EK_TM4C123GXL.h is tied to EK_TM4C123GXL.c - which sets up the hardware in a way consistent with the
 *  EK_TM4C123GXL.h configuration (e.g., which pins are GPIO input vs. output, which pins are PWM...
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>

#include <stdbool.h>

/* the Board.h file is what helps isolate the code from the board being used
 * names/references used in this application that refer to board settings, such as pin indexes,
 * come from Board.h.
 * Board.h is mostly a pass-through file.  The names/references it defines are actually references
 * to board-specific files (e.g., EK_TM4C123GXL.h)
 */
#include "Board.h"

#define TASKSTACKSIZE   512

Task_Struct headSideToSide_Struct;
UInt8 headSideToSide_Stack[TASKSTACKSIZE];
Task_Handle headSideToSideTask;

Task_Struct headUpAndDown_Struct;
UInt8 headUpAndDown_Stack[TASKSTACKSIZE];
Task_Handle headUpAndDownTask;

Task_Struct mouthOpenClose_Struct;
UInt8 mouthOpenClose_Stack[TASKSTACKSIZE];
Task_Handle mouthOpenCloseTask;

Task_Struct distSensorTask_Struct;
UInt8 distSensorTask_Stack[TASKSTACKSIZE];
Task_Handle distSensorTask;

/*
 * Constants - to be adjusted to control behavior
 */

typedef enum state_values {
    PanningMode = 0,
    RisingMode,
    HowlingMode,
    LoweringMode
} state_values;

int state = PanningMode;

const int minTriggerDistance = 10;      // minimum distance, inches,  object must be away in order to trigger
const int maxTriggerDistance = 72;      // maximum distance, inches, object must be away in order to trigger

const bool distSensorActive = true;
const bool headturnActive = false;
const bool headliftActive = false;
const bool mouthActive    = false;
const bool breathingActive    = true;

const int headTurnMillisForPanningMode   = 5000; //time to go from left-to-right or right-to-left
const int headLiftMillisForRisingMode    = 3000;  //time to get head looking down while rising
const int headTurnMillisForRisingingMode = 3000;  //time to get head pointed forward
const int lengthOfRisingMode             = 6000;  //time spent in rising mode - start to finish
const int headLiftMillisForHowlingMode   = 3000;  //time to raise head to howl
const int lengthOfHowlMillis             = 6000;  //length of howl sound
const int lengthOfHowlingMode            = 6000;  //time spent in howling mode - start to finish
const int headLiftMillisForLoweringMode  = 5000;  //time to lower head while lowering
const int lengthOfLoweringingMode        = 5000;  //time spent in lowering mode - start to finish
const int requiredHitCount               = 2;     //number of matching hits from distance sensor to trigger rise
const int resetMillis                    = 5000;  //time before allowed to re-trigger

const int minDutyToLeftShoulder = 750;
const int maxDutyToRightShoulder = 2000;
const int centeredDuty = 1500;
      int headTurnServoDutyInc = 10;
const int headTurnDurationOfInc = 40;

const int headLiftDutyUp = 1700;    //1700 seems right for panning mode and howl
const int headLiftDutyDown = 700;   //700 seems right for rising mode (looking down)
      int headLiftServoDutyInc = 10;
const int headLiftDurationOfInc = 40;

const int minDutyMouthOpen = 750;
const int maxDutyMouthClose = 2000;
      int mouthOpenCloseServoDutyInc = 10;
const int mouthOpenCloseDurationOfInc = 40;

// logging flags
const bool logHeadTurn = false;
const bool logHeadLift = false;
const bool logMouthOpenClose = true;
const bool logDistSensor = true;


/*
 *  ======== headSideToSideFxn ========
 *  Task periodically increments the PWM duty for the on board LED.
 */
Void headSideToSideFxn(UArg arg0, UArg arg1)
{
    PWM_Handle headSideToSideServo; //********************** JJM
    PWM_Params servoPwmParams;
    uint16_t   pwmServoPeriod = 3000;      // Period in microseconds 20,000 <=> 50Hz

    PWM_Params_init(&servoPwmParams);
    servoPwmParams.period = pwmServoPeriod;
    headSideToSideServo = PWM_open(Board_HeadSideToSide_servo, &servoPwmParams); //************ JJM "Board_PWM2_servo" comes from Board.h but points to EK_TM4C123GXL.h PWM enum
    if (headSideToSideServo == NULL) {
        System_abort("headSideToSideServo did not open");
    }

    uint16_t   servoDuty = centeredDuty; //starting duty (servo position)

    System_printf("servoDuty: %i\nservoDutyInc: %i\ndurationOfInc= %i\n", servoDuty, headTurnServoDutyInc,headTurnDurationOfInc);
    System_flush();

    /* Loop forever incrementing the PWM duty */
    while (headturnActive) {

        if(headturnActive) {
          if(state == PanningMode) {
              if(logHeadTurn) {
                  System_printf("setting headTurn to duty: %i\n", servoDuty);
                  System_flush();
              }
              PWM_setDuty(headSideToSideServo, servoDuty);
              if(headTurnServoDutyInc > 0) {
                  servoDuty += headTurnServoDutyInc;
                  if(servoDuty > maxDutyToRightShoulder) {
                      headTurnServoDutyInc = -1 * headTurnServoDutyInc;
                      servoDuty += headTurnServoDutyInc;
                  }
              }
              if(headTurnServoDutyInc < 0) {
                  servoDuty += headTurnServoDutyInc;
                  if(servoDuty < minDutyToLeftShoulder) {
                      headTurnServoDutyInc = -1 * headTurnServoDutyInc;
                      servoDuty += headTurnServoDutyInc;
                  }
              }

              Task_sleep(headTurnDurationOfInc);
          }

          if(state == RisingMode) {
              ;
          }
        }

        Task_sleep(headTurnDurationOfInc);
    }
}


/*
 *  ======== headUpAndDownFxn ========
 *  Task periodically increments the PWM duty for the on board LED.
 */
Void headUpAndDownFxn(UArg arg0, UArg arg1)
{
    PWM_Handle headUpAndDownServo; //********************** JJM
    PWM_Params headUpAndDownServoPwmParams;
    uint16_t   headUpAndDownServoPeriod = 3000;      // Period in microseconds 20,000 <=> 50Hz

    PWM_Params_init(&headUpAndDownServoPwmParams);
    headUpAndDownServoPwmParams.period = headUpAndDownServoPeriod;
    headUpAndDownServo = PWM_open(Board_HeadUpDown_servo, &headUpAndDownServoPwmParams);
    if (headUpAndDownServo == NULL) {
        System_abort("Board_HeadUpDown_servo did not open");
    }

    int headLiftDuty = headLiftDutyUp;
    int currentHeadLiftDuty = -1;

    while (headliftActive) {


        if(state == PanningMode) {
            headLiftDuty = headLiftDutyUp;

            if(logHeadLift) {
                System_printf("setting headLift to duty: %i\n", headLiftDuty);
                System_flush();
            }
            if(headLiftDuty != currentHeadLiftDuty) {
                PWM_setDuty(headUpAndDownServo, headLiftDuty);
                currentHeadLiftDuty = headLiftDuty;
            }

            Task_sleep(headLiftDurationOfInc);
        }

        if(state == RisingMode) {
            while (state == RisingMode && headLiftDuty < headLiftDutyDown) {
                if(logHeadLift) {
                    System_printf("setting headLift to duty: %i\n", headLiftDuty);
                    System_flush();
                }
                PWM_setDuty(headUpAndDownServo, headLiftDuty);

                headLiftDuty += headLiftServoDutyInc;

                Task_sleep(headLiftDurationOfInc);
            }
        }
    }
}


/*
 *  ======== mouthOpenCloseFxn ========
 *  Task periodically increments the PWM duty for the on board LED.
 */
/*
Void mouthOpenCloseFxn(UArg arg0, UArg arg1)
{
    PWM_Handle mouthOpenCloseServo; //********************** JJM
    PWM_Params mouthOpenCloseServoParams;
    uint16_t   mouthOpenCloseServoPeriod = 3000;      // Period in microseconds 20,000 <=> 50Hz

    PWM_Params_init(&mouthOpenCloseServoParams);
    mouthOpenCloseServoParams.period = mouthOpenCloseServoPeriod;
    mouthOpenCloseServo = PWM_open(Board_MouthOpenClose_servo, &mouthOpenCloseServoParams); //************ JJM "Board_PWM2_servo" comes from Board.h but points to EK_TM4C123GXL.h PWM enum
    if (mouthOpenCloseServo == NULL) {
        System_abort("mouthOpenCloseServo did not open");
    }

    uint16_t   mouthOpenCloseServoDuty = minDutyMouthOpen; //starting duty (servo position)

    System_printf("mouthOpenCloseServoDuty: %i\nmouthOpenCloseServoDutyInc: %i\nmouthOpenCloseDurationOfInc= %i\n", mouthOpenCloseServoDuty, mouthOpenCloseServoDutyInc,mouthOpenCloseDurationOfInc);
    System_flush();

    while (mouthActive) {

        if(mouthActive) {
          if(state == PanningMode) {
              if(logMouthOpenClose) {
                  System_printf("setting mouth to duty: %i\n", mouthOpenCloseServoDuty);
                  System_flush();
              }
              PWM_setDuty(mouthOpenCloseServo, mouthOpenCloseServoDuty);
              if(mouthOpenCloseServoDutyInc > 0) {
                  mouthOpenCloseServoDuty += mouthOpenCloseServoDutyInc;
                  if(mouthOpenCloseServoDuty > maxDutyMouthClose) {
                      mouthOpenCloseServoDutyInc = -1 * mouthOpenCloseServoDutyInc;
                      mouthOpenCloseServoDuty += mouthOpenCloseServoDutyInc;
                  }
              }
              if(mouthOpenCloseServoDutyInc < 0) {
                  mouthOpenCloseServoDuty += mouthOpenCloseServoDutyInc;
                  if(mouthOpenCloseServoDuty < minDutyMouthOpen) {
                      mouthOpenCloseServoDutyInc = -1 * mouthOpenCloseServoDutyInc;
                      mouthOpenCloseServoDuty += mouthOpenCloseServoDutyInc;
                  }
              }

              Task_sleep(mouthOpenCloseDurationOfInc);
          }

          if(state == RisingMode) {
              ;
          }
        }
    }
}
*/

/*
 *  ======== distSensorTaskFxn ========
 *  Task monitors distances and changes state accordingly
 */
Void distSensorFxn(UArg arg0, UArg arg1)
{
    uint16_t   duration = 0;
    uint16_t   distance = 0;

    /* Loop forever incrementing the PWM duty */
    while (distSensorActive) {
        duration = 0;


        // CHECK DISTANCE
        // Send signal to Trigger pin on distance sensor
        // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
        GPIO_write(Dist_Sensor_Trigger, 0);     // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
        GPIO_write(Board_LED0, Board_LED_OFF);  // turn off blue LED
        Task_sleep(5*1/Clock_tickPeriod);       // sleep a bit while pin is low
        GPIO_write(Dist_Sensor_Trigger, 1);     // set pin high to signal sensor to check distance
        GPIO_write(Board_LED0, Board_LED_ON);   // turn on blue LED to show sensor is checking
        Task_sleep(1);                          // need at least 10 microseconds of signal being on trigger pin to invoke sensor to read
        GPIO_write(Dist_Sensor_Trigger, 0);     // set pulse back to low

        duration = pulseDuration(Dist_Sensor_Echo); // measure pulse duration on echo pin - needs very low latency
        distance = duration/74/2;

        GPIO_write(Board_LED0, Board_LED_OFF);  // turn off blue LED to show no more measuring

        if(logDistSensor) {
            System_printf("duration: %i  distance: %i\n", duration, distance);
            System_flush();
        }

        if(distance >= minTriggerDistance && distance <= maxTriggerDistance) {
            //something is in range - let's move!!!

            state = RisingMode;

            //raise body
            if(logDistSensor) {
                System_printf("Raising body...\n");
                System_flush();
            }
            GPIO_write(transistorGatePin, 1);     // powers transistor (and an inline LED so we can see it happen)

            //stop breathing
            GPIO_write(breathingPin, 1);

            Task_sleep(lengthOfRisingMode);               // wait for length of mode

            state = HowlingMode;
            Task_sleep(headLiftMillisForHowlingMode);
            //howl();
            GPIO_write(howlingPin, 1);                  //high turns it off
            GPIO_write(howlingPin, 0);                  //low turns it on
            Task_sleep(lengthOfHowlMillis);
            GPIO_write(howlingPin, 1);                  //high turns it off
            Task_sleep(lengthOfHowlingMode);            // wait for length of mode


            state = LoweringMode;
            //lower body
            if(logDistSensor) {
                System_printf("Lowering body...\n");
                System_flush();
            }
            GPIO_write(transistorGatePin, 0);      // power off transistor (and thus solenoid)
            Task_sleep(lengthOfLoweringingMode);               // wait for length of mode

            state = PanningMode;
            //start breathing
            GPIO_write(breathingPin, 1);                  //high turns it off
            GPIO_write(breathingPin, 0);                  //low turns it on

            // delay long enough to allow body to lower and ready for next go...
            Task_sleep(resetMillis);
        }

        //delay before repeating sensing
        Task_sleep(500);
    }
}

/*
 * Given a GPIO pin index, return the pulse duration in microseconds
 */
int pulseDuration(int pinIndex) {
    unsigned long   max_echo_loops = 100000;
    unsigned long   echo_loop_count = 0;
    unsigned long   duration = 0;

    // wait for any previous pulse to end
    while(GPIO_read(Dist_Sensor_Echo) == 1) {
        if(echo_loop_count++ == max_echo_loops) {
            if(logDistSensor) {
                System_printf("Pulse already in progress expired the time...\n");
                System_flush();
            }
            return 0;
        }
    }

    // wait for pulse to start
    while(GPIO_read(Dist_Sensor_Echo) == 0) {
        if(echo_loop_count++ == max_echo_loops) {
            if(logDistSensor) {
                System_printf("Pulse never started...\n");
                System_flush();
            }
            return 0;
        }
    }

    while (GPIO_read(Dist_Sensor_Echo) == 1) {
        duration++;
        if(echo_loop_count++ == max_echo_loops) {
            if(logDistSensor) {
                System_printf("Pulse maxed out...");
                System_flush();
            }
            return 0;
        }
    }

    return duration;

}

Void logHeadTurnFxn(String text) {
    if(logHeadTurn) {
        logFxn(text);
    }
}
Void logDistanceSensorFxn(String text) {
    if(logDistSensor) {
        logFxn(text);
    }
}
Void logFxn(text) {
    System_printf("logging\n");
    System_printf("%s\n",text);
    System_flush();
    System_printf("after logging\n");
    System_flush();
}

/*
 *  ======== main ========
 */
int main(void)
{
    Task_Params tskParams;
    Task_Params headUpAndDownTaskParams;
    Task_Params distSensorTaskParams;

    /* Call board init functions. */
    Board_initGeneral();
    Board_initGPIO();
    Board_initPWM();

    /* Construct headSideToSide Task thread */
    Task_Params_init(&tskParams);
    tskParams.stackSize = TASKSTACKSIZE;
    tskParams.stack = &headSideToSide_Stack;
    tskParams.arg0 = 50;
    Task_construct(&headSideToSide_Struct, (Task_FuncPtr)headSideToSideFxn, &tskParams, NULL);
    /* Obtain instance handle */
    headSideToSideTask = Task_handle(&headSideToSide_Struct);

    /* Construct headUpAndDown Task thread */
    Task_Params_init(&headUpAndDownTaskParams);
    headUpAndDownTaskParams.stackSize = TASKSTACKSIZE;
    headUpAndDownTaskParams.stack = &headUpAndDown_Stack;
    headUpAndDownTaskParams.arg0 = 50;
    Task_construct(&headUpAndDown_Struct, (Task_FuncPtr)headUpAndDownFxn, &headUpAndDownTaskParams, NULL);
    /* Obtain instance handle */
    headUpAndDownTask = Task_handle(&headUpAndDown_Struct);

    /* Construct distance sensor Task thread */
    Task_Params_init(&distSensorTaskParams);
    distSensorTaskParams.stackSize = TASKSTACKSIZE;
    distSensorTaskParams.stack = &distSensorTask_Stack;
    distSensorTaskParams.arg0 = 50;
    Task_construct(&distSensorTask_Struct, (Task_FuncPtr)distSensorFxn, &distSensorTaskParams, NULL);

    /* Obtain instance handle */
    distSensorTask = Task_handle(&distSensorTask_Struct);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    System_printf("Starting the example\nSystem provider is set to SysMin. "
                  "Halt the target to view any SysMin contents in ROV.\n");

    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
