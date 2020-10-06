#include <stdio.h>
#include "system.h"
#include "includes.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"

#define DEBUG 1

#define HW_TIMER_PERIOD 100 /* 100ms */

/* Button Patterns */

#define GAS_PEDAL_FLAG      0x08
#define BRAKE_PEDAL_FLAG    0x04
#define CRUISE_CONTROL_FLAG 0x02
/* Switch Patterns */

#define TOP_GEAR_FLAG       0x00000002
#define ENGINE_FLAG         0x00000001

/* LED Patterns */

#define LED_RED_0 0x00000001 // Engine
#define LED_RED_1 0x00000002 // Top Gear

#define LED_GREEN_0 0x0001 // Cruise Control activated
#define LED_GREEN_2 0x0002 // Cruise Control Button
#define LED_GREEN_4 0x0010 // Brake Pedal
#define LED_GREEN_6 0x0040 // Gas Pedal

#define LED_RED_12 
#define LED_RED_12 
#define LED_RED_12 
#define LED_RED_12 
#define LED_RED_12 
#define LED_RED_12 
/*
 * Definition of Tasks
 */

#define TASK_STACKSIZE 2048

OS_STK StartTask_Stack[TASK_STACKSIZE];
OS_STK ControlTask_Stack[TASK_STACKSIZE];
OS_STK VehicleTask_Stack[TASK_STACKSIZE];
OS_STK ButtonIOTask_Stack[TASK_STACKSIZE];
OS_STK SwitchIOTask_Stack[TASK_STACKSIZE];

// Task Priorities

#define STARTTASK_PRIO     5
#define VEHICLETASK_PRIO  10
#define CONTROLTASK_PRIO  12
#define BUTTONIOTASK_PRIO  8
#define SWITCHIOTASK_PRIO  7

// Task Periods

#define CONTROL_PERIOD  300 
#define VEHICLE_PERIOD  300

/*
 * Definition of Kernel Objects
 */

// Mailboxes
OS_EVENT *Mbox_Throttle;
OS_EVENT *Mbox_Velocity;
OS_EVENT *Mbox_Brake;
OS_EVENT *Mbox_Gas;
OS_EVENT *Mbox_Cruise;
OS_EVENT *Mbox_Engine;
OS_EVENT *Mbox_Gear;

// Semaphores
OS_EVENT *ControlTimerSem;
OS_EVENT *VehicleTimerSem;
OS_EVENT *ButtonTaskTimerSem;
OS_EVENT *SwitchTaskTimerSem;

// SW-Timer
OS_TMR *ControlTimer;
OS_TMR *VehicleTimer;

/*
 * Types
 */
enum active {on = 2, off = 1};

enum active gas_pedal = off;
enum active brake_pedal = off;
enum active top_gear = off;
enum active engine = off;
enum active cruise_control = off;

/*
 * Global variables
 */
int delay; // Delay of HW-timer
INT16U led_green = 0; // Green LEDs
INT32U led_red = 0;   // Red LEDs

int buttons_pressed(void)
{
  return ~IORD_ALTERA_AVALON_PIO_DATA(D2_PIO_KEYS4_BASE);
}

int switches_pressed(void)
{
  return IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_TOGGLES18_BASE);
}

void change_led_button_status(int led_values)
{
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_GREENLED9_BASE,led_values);
}

void change_led_switch_status(int mask, int led_values)
{
  int currentLedStatus = IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE);
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, ((~mask) & currentLedStatus) | led_values);
}


/*
 * ISR for HW Timer
 */
alt_u32 alarm_handler(void* context)
{
  OSTmrSignal(); /* Signals a 'tick' to the SW timers */

  return delay;
}

/* Callback Functions */
void VehicleTimerCallback (void *ptmr, void *callback_arg){
  OSSemPost(VehicleTimerSem);
  printf("OSSemPost(VehicleTimerSem);\n");
}

void ControlTimerCallback (void *ptmr, void *callback_arg){
  OSSemPost(ControlTimerSem);
  OSSemPost(ButtonTaskTimerSem);
  OSSemPost(SwitchTaskTimerSem);
  printf("OSSemPost(Control Timers);\n");
}

static int b2sLUT[] = {0x40, //0
		       0x79, //1
		       0x24, //2
		       0x30, //3
		       0x19, //4
		       0x12, //5
		       0x02, //6
		       0x78, //7
		       0x00, //8
		       0x18, //9
		       0x3F, //-
};

/*
 * convert int to seven segment display format
 */
int int2seven(int inval){
  return b2sLUT[inval];
}

/*
 * output current velocity on the seven segement display
 */
void show_velocity_on_sevenseg(INT8S velocity){
  int tmp = velocity;
  int out;
  INT8U out_high = 0;
  INT8U out_low = 0;
  INT8U out_sign = 0;

  if(velocity < 0){
    out_sign = int2seven(10);
    tmp *= -1;
  }else{
    out_sign = int2seven(0);
  }

  out_high = int2seven(tmp / 10);
  out_low = int2seven(tmp - (tmp/10) * 10);

  out = int2seven(0) << 21 |
    out_sign << 14 |
    out_high << 7  |
    out_low;
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_LOW28_BASE,out);
}

/*
 * shows the target velocity on the seven segment display (HEX5, HEX4)
 * when the cruise control is activated (0 otherwise)
 */
void show_target_velocity(INT8U target_vel)
{

}

/*
 * indicates the position of the vehicle on the track with the four leftmost red LEDs
 * LEDR17: [0m, 400m)
 * LEDR16: [400m, 800m)
 * LEDR15: [800m, 1200m)
 * LEDR14: [1200m, 1600m)
 * LEDR13: [1600m, 2000m)
 * LEDR12: [2000m, 2400m]
 */
void show_position(INT16U position)
{

  INT32U ledStatus=0;
  if(position<400){
    ledStatus=0x20000;
  }else if(position<800){
    ledStatus=0x10000;
  }else if(position<1200){
    ledStatus=0x8000;    
  }else if(position<1600){
    ledStatus=0x4000;    
  }else if(position<2000){
    ledStatus=0x2000;    
  }else if(position<=2400){
    ledStatus=0x1000;    
  }
  INT32U mask= 0x3F000;
  change_led_switch_status(mask,ledStatus);

}

/*
 * The function 'adjust_position()' adjusts the position depending on the
 * acceleration and velocity.
 */
INT16U adjust_position(INT16U position, INT16S velocity,
		       INT8S acceleration, INT16U time_interval)
{
  INT16S new_position = position + velocity * time_interval / 1000;

  if (new_position > 2400) {
    new_position -= 2400;
  } else if (new_position < 0){
    new_position += 2400;
  }

  show_position(new_position);
  return new_position;
}

/*
 * The function 'adjust_velocity()' adjusts the velocity depending on the
 * acceleration.
 */
INT16S adjust_velocity(INT16S velocity, INT8S acceleration,
		       enum active brake_pedal, INT16U time_interval)
{
  INT16S new_velocity;
  INT8U brake_retardation = 200;

  if (brake_pedal == off)
    new_velocity = velocity  + (float) (acceleration * time_interval) / 1000.0;
  else {
    if (brake_retardation * time_interval / 1000 > velocity)
      new_velocity = 0;
    else
      new_velocity = velocity - brake_retardation * time_interval / 1000;
  }

  return new_velocity;
}

/*
 * The task 'VehicleTask' updates the current velocity of the vehicle
 */
void VehicleTask(void* pdata)
{
  INT8U err;
  void* msg;
  INT8U* throttle;
  INT8S acceleration;
  INT8S retardation;
  INT16U position = 0;
  INT16S velocity = 0;
  INT16S wind_factor;
  enum active brake_pedal = off;

  printf("Vehicle task created!\n");

  while(1)
    {
      err = OSMboxPost(Mbox_Velocity, (void *) &velocity);

      //OSTimeDlyHMSM(0,0,0,VEHICLE_PERIOD);
      OSSemPend(VehicleTimerSem, 0, &err);

      /* Non-blocking read of mailbox:
	 - message in mailbox: update throttle
	 - no message:         use old throttle
      */
      msg = OSMboxPend(Mbox_Throttle, 1, &err);
      if (err == OS_NO_ERR)
	    throttle = (INT8U*) msg;
      msg = OSMboxPend(Mbox_Brake, 1, &err);
      if (err == OS_NO_ERR)
	    brake_pedal = (enum active) msg;

      // vehichle cannot effort more than 80 units of throttle
      if (*throttle > 80) *throttle = 80;

      // brakes + wind
      if (brake_pedal == off)
	{
	  acceleration = (*throttle) - 1*velocity;

	  if (400 <= position && position < 800)
	    acceleration -= 2; // traveling uphill
	  else if (800 <= position && position < 1200)
	    acceleration -= 4; // traveling steep uphill
	  else if (1600 <= position && position < 2000)
	    acceleration += 4; //traveling downhill
	  else if (2000 <= position)
	    acceleration += 2; // traveling steep downhill
	}
      else
	acceleration = -4*velocity;

      printf("Position: %d m\n", position);
      printf("Velocity: %d m/s\n", velocity);
      printf("Accell: %d m/s2\n", acceleration);
      printf("Throttle: %d V\n", *throttle);

      position = position + velocity * VEHICLE_PERIOD / 1000;
      velocity = velocity  + acceleration * VEHICLE_PERIOD / 1000.0;
      if(position > 2400)
	      position = 0;
      // position = adjust_position(position, velocity, acceleration, VEHICLE_PERIOD);

      show_velocity_on_sevenseg((INT8S) velocity);
      // show_target_velocity((INT8S) velocity);
    }
}

/*
 * The task 'ControlTask' is the main task of the application. It reacts
 * on sensors and generates responses.
 */
void ControlTask(void* pdata)
{
  INT8U err;
  INT8U throttle = 40; /* Value between 0 and 80, which is interpreted as between 0.0V and 8.0V */
  void* msg;
  INT16S* current_velocity;

  printf("Control Task created!\n");

  while(1)
    {
      msg = OSMboxPend(Mbox_Velocity, 0, &err);
      current_velocity = (INT16S*) msg;

      err = OSMboxPost(Mbox_Throttle, (void *) &throttle);

      //OSTimeDlyHMSM(0,0,0, CONTROL_PERIOD);
      OSSemPend(ControlTimerSem, 0, &err);
    }
}

/*
  * The task 'ButtonIOTask' is the main task of the application. It reacts
  * on sensors and generates responses.
  */
void ButtonIOTask(void* pdata)
{
  INT8U err;
  int button_register;
  printf("ButtonIO Task created!\n");

  while(1)
  {
    button_register = (~button_register) & 0xf;
    button_register = buttons_pressed();

    err = OSMboxPost(Mbox_Gas, button_register & GAS_PEDAL_FLAG);
    err = OSMboxPost(Mbox_Cruise,button_register & CRUISE_CONTROL_FLAG);
    err = OSMboxPost(Mbox_Brake, button_register & BRAKE_PEDAL_FLAG);
        
    int led_value=((button_register & CRUISE_CONTROL_FLAG)>0)*0xff &  LED_GREEN_2
                  | ((button_register & BRAKE_PEDAL_FLAG)>0)*0xff  & LED_GREEN_4
                  | ((button_register & GAS_PEDAL_FLAG)>0)*0xff  & LED_GREEN_6;
    
    change_led_button_status(led_value);
    
    OSSemPend(ButtonTaskTimerSem, 0, &err);
  }

}

/*
  * The task 'SwitchIOTask' is the main task of the application. It reacts
  * on sensors and generates responses.
  */
void SwitchIOTask(void* pdata)
{
  INT8U err;
  int switch_register;
  printf("SwitchIO Task created!\n");

  while(1)
  {
    
    switch_register = (~switch_register) & 0xf;
    switch_register = switches_pressed();
    err = OSMboxPost(Mbox_Engine, switch_register & ENGINE_FLAG);
    err = OSMboxPost(Mbox_Gear,switch_register & TOP_GEAR_FLAG);

    int led_value=((switch_register & ENGINE_FLAG)>0)*0xff &  LED_RED_0
                  | ((switch_register & TOP_GEAR_FLAG)>0)*0xff  & LED_RED_1;

    change_led_switch_status(0x3,led_value);

    OSSemPend(SwitchTaskTimerSem, 0, &err);
  }

}



/*
 * The task 'StartTask' creates all other tasks kernel objects and
 * deletes itself afterwards.
 */
void StartTask(void* pdata)
{
  INT8U err;
  void* context;

  static alt_alarm alarm;     /* Is needed for timer ISR function */

  /* Base resolution for SW timer : HW_TIMER_PERIOD ms */
  delay = alt_ticks_per_second() * HW_TIMER_PERIOD / 1000;
  printf("delay in ticks %d\n", delay);

  /*
   * Create Hardware Timer with a period of 'delay'
   */
  if (alt_alarm_start (&alarm,
		       delay,
		       alarm_handler,
		       context) < 0)
    {
      printf("No system clock available!n");
    }


  /*
   * Create and start Software Timer
   */

   //Create Control Timer
   ControlTimer = OSTmrCreate(0, //delay
                            CONTROL_PERIOD/HW_TIMER_PERIOD, //period
                            OS_TMR_OPT_PERIODIC,
                            ControlTimerCallback, //OS_TMR_CALLBACK
                            (void *)0,
                            "ControlTimer",
                            &err);

   if (DEBUG) {
    if (err == OS_ERR_NONE) { //if creation successful
      printf("ControlTimer created\n");
    }
   }

   //Create Vehicle Timer
   VehicleTimer = OSTmrCreate(0, //delay
                            VEHICLE_PERIOD/HW_TIMER_PERIOD, //period
                            OS_TMR_OPT_PERIODIC,
                            VehicleTimerCallback, //OS_TMR_CALLBACK
                            (void *)0,
                            "VehicleTimer",
                            &err);

   if (DEBUG) {
    if (err == OS_ERR_NONE) { //if creation successful
      printf("VehicleTimer created\n");
    }
   }

   /*
    * Start timers
    */

   //start Vehicle Timer
   OSTmrStart(VehicleTimer, &err);

   if (DEBUG) {
    if (err == OS_ERR_NONE) { //if start successful
      printf("VehicleTimer started\n");
    }
   }

   //start Control Timer
   OSTmrStart(ControlTimer, &err);

   if (DEBUG) {
    if (err == OS_ERR_NONE) { //if start successful
      printf("ControlTimer started\n");
    }
   }

   /*
   * Creation of Kernel Objects
   */

  VehicleTimerSem = OSSemCreate(0);
  ControlTimerSem = OSSemCreate(0);
  SwitchTaskTimerSem = OSSemCreate(0);
  ButtonTaskTimerSem = OSSemCreate(0);

  // Mailboxes
  Mbox_Throttle = OSMboxCreate((void*) 0); /* Empty Mailbox - Throttle */
  Mbox_Velocity = OSMboxCreate((void*) 0); /* Empty Mailbox - Velocity */
  Mbox_Brake = OSMboxCreate((void*) 0); /* Empty Mailbox - Brake */
  Mbox_Gas = OSMboxCreate((void*) 0); /* Empty Mailbox -  Gas */
  Mbox_Cruise = OSMboxCreate((void*) 0); /* Empty Mailbox - Cruise */
  Mbox_Gear = OSMboxCreate((void*) 0); /* Empty Mailbox -  Gear */
  Mbox_Engine = OSMboxCreate((void*) 0); /* Empty Mailbox - Engine */

  /*
   * Create statistics task
   */

  OSStatInit();

  /*
   * Creating Tasks in the system
   */


  err = OSTaskCreateExt(
			ControlTask, // Pointer to task code
			NULL,        // Pointer to argument that is
			// passed to task
			&ControlTask_Stack[TASK_STACKSIZE-1], // Pointer to top
			// of task stack
			CONTROLTASK_PRIO,
			CONTROLTASK_PRIO,
			(void *)&ControlTask_Stack[0],
			TASK_STACKSIZE,
			(void *) 0,
			OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
			VehicleTask, // Pointer to task code
			NULL,        // Pointer to argument that is
			// passed to task
			&VehicleTask_Stack[TASK_STACKSIZE-1], // Pointer to top
			// of task stack
			VEHICLETASK_PRIO,
			VEHICLETASK_PRIO,
			(void *)&VehicleTask_Stack[0],
			TASK_STACKSIZE,
			(void *) 0,
			OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
			ButtonIOTask, // Pointer to task code
			NULL,        // Pointer to argument that is
			// passed to task
			&ButtonIOTask_Stack[TASK_STACKSIZE-1], // Pointer to top
			// of task stack
			BUTTONIOTASK_PRIO,
			BUTTONIOTASK_PRIO,
			(void *)&ButtonIOTask_Stack[0],
			TASK_STACKSIZE,
			(void *) 0,
			OS_TASK_OPT_STK_CHK);

  err = OSTaskCreateExt(
			SwitchIOTask, // Pointer to task code
			NULL,        // Pointer to argument that is
			// passed to task
			&SwitchIOTask_Stack[TASK_STACKSIZE-1], // Pointer to top
			// of task stack
			SWITCHIOTASK_PRIO,
			SWITCHIOTASK_PRIO,
			(void *)&SwitchIOTask_Stack[0],
			TASK_STACKSIZE,
			(void *) 0,
			OS_TASK_OPT_STK_CHK);

  printf("All Tasks and Kernel Objects generated!\n");

  /* Task deletes itself */

  OSTaskDel(OS_PRIO_SELF);
}

/*static void Key_InterruptHandler(void* context, alt_u32 id)
{
  printf("Interrupt is called\n");
  /* Write to the edge capture register to reset it. */
  /*IOWR_ALTERA_AVALON_PIO_EDGE_CAP(DE2_PIO_KEYS4_BASE, 0);
  /* reset interrupt capability for the Button PIO. */
  /*IOWR_ALTERA_AVALON_PIO_IRQ_MASK(DE2_PIO_KEYS4_BASE, 0xf);
}*/

/*
 *
 * The function 'main' creates only a single task 'StartTask' and starts
 * the OS. All other tasks are started from the task 'StartTask'.
 *
 */


int main(void) {


  // set interrupt capability for the Button PIO. 
 // IOWR_ALTERA_AVALON_PIO_IRQ_MASK(DE2_PIO_KEYS4_BASE, 0xf);
   // Reset the edge capture register.
  //IOWR_ALTERA_AVALON_PIO_EDGE_CAP(DE2_PIO_KEYS4_BASE, 0x0);
  // Register the ISR for buttons
  //alt_irq_register(DE2_PIO_KEYS4_IRQ, NULL, Key_InterruptHandler); 


  printf("Lab: Cruise Control\n");

  OSTaskCreateExt(
		  StartTask, // Pointer to task code
		  NULL,      // Pointer to argument that is
		  // passed to task
		  (void *)&StartTask_Stack[TASK_STACKSIZE-1], // Pointer to top
		  // of task stack
		  STARTTASK_PRIO,
		  STARTTASK_PRIO,
		  (void *)&StartTask_Stack[0],
		  TASK_STACKSIZE,
		  (void *) 0,
		  OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

  OSStart();

  return 0;
}
