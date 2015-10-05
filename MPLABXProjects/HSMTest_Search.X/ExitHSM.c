/*
 * File: SearchHSM.c
 * Author: J. Edward Carryer
 * Modified: Gabriel Elkaim and Soja-Marie Morgens
 *
 * Template file to set up a Heirarchical State Machine to work with the Events and
 * Services Framework (ES_Framework) on the Uno32 for the CMPE-118/L class. Note that
 * this file will need to be modified to fit your exact needs, and most of the names
 * will have to be changed to match your code.
 *
 * There is another template file for the SubHSM's that is slightly differet, and
 * should be used for all of the subordinate state machines (flat or heirarchical)
 *
 * This is provided as an example and a good place to start.
 *
 * History
 * When           Who     What/Why
 * -------------- ---     --------
 * 09/13/13 15:17 ghe      added tattletail functionality and recursive calls
 * 01/15/12 11:12 jec      revisions for Gen2 framework
 * 11/07/11 11:26 jec      made the queue static
 * 10/30/11 17:59 jec      fixed references to CurrentEvent in RunTemplateSM()
 * 10/23/11 18:20 jec      began conversion from SMTemplate.c (02/20/07 rev)
 */


/*******************************************************************************
 * MODULE #INCLUDE                                                             *
 ******************************************************************************/

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "BOARD.h"
#include "BotConfig.h"
#include "MotorDriver.h"
#include "ExitHSM.h"
/*******************************************************************************
 * PRIVATE #DEFINES                                                            *
 ******************************************************************************/
//Include any defines you need to do
/*******************************************************************************
 * MODULE #DEFINES                                                             *
 ******************************************************************************/


#define STRING_FORM(STATE) #STATE, //Strings are stringified and comma'd
static const char *StateNames[] = {
	LIST_OF_EXIT_STATES(STRING_FORM)
};

/*******************************************************************************
 * GLOBAL VARIABLES							       *
 ******************************************************************************/


/*******************************************************************************
 * PRIVATE MODULE VARIABLES                                                            *
 ******************************************************************************/

static ExitState_t CurrentState = Exit_Init;
static uint8_t MyPriority;


/*******************************************************************************
 * PUBLIC FUNCTIONS                                                            *
 ******************************************************************************/

/**
 * @Function InitExitHSM(uint8_t Priority)
 * @param Priority - internal variable to track which event queue to use
 * @return TRUE or FALSE
 * @brief This will get called by the framework at the beginning of the code
 *        execution. It will post an ES_INIT event to the appropriate event
 *        queue, which will be handled inside RunExitFSM function. Remember
 *        to rename this to something appropriate.
 *        Returns TRUE if successful, FALSE otherwise
 * @author J. Edward Carryer, 2011.10.23 19:25 */
uint8_t InitExitHSM(uint8_t Priority)
{
	MyPriority = Priority;
	// put us into the Initial PseudoState
	CurrentState = Exit_Init;
	// post the initial transition event
	if (ES_PostToService(MyPriority, INIT_EVENT) == TRUE) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * @Function PostExitHSM(ES_Event ThisEvent)
 * @param ThisEvent - the event (type and param) to be posted to queue
 * @return TRUE or FALSE
 * @brief This function is a wrapper to the queue posting function, and its name
 *        will be used inside ES_Configure to point to which queue events should
 *        be posted to. Remember to rename to something appropriate.
 *        Returns TRUE if successful, FALSE otherwise
 * @author J. Edward Carryer, 2011.10.23 19:25 */
uint8_t PostExitHSM(ES_Event ThisEvent)
{
	return ES_PostToService(MyPriority, ThisEvent);
}

/**
 * @Function QueryExitHSM(void)
 * @param none
 * @return Current state of the state machine
 * @brief This function is a wrapper to return the current state of the state
 *        machine. Return will match the ENUM above. Remember to rename to
 *        something appropriate, and also to rename the ExitState_t to your
 *        correct variable name.
 * @author J. Edward Carryer, 2011.10.23 19:25 */
ExitState_t QueryExitHSM(void)
{
	return (CurrentState);
}

/**
 * @Function RunExitHSM(ES_Event ThisEvent)
 * @param ThisEvent - the event (type and param) to be responded.
 * @return Event - return event (type and param), in general should be ES_NO_EVENT
 * @brief This function is where you implement the whole of the heirarchical state
 *        machine, as this is called any time a new event is passed to the event
 *        queue. This function will be called recursively to implement the correct
 *        order for a state transition to be: exit current state -> enter next state
 *        using the ES_EXIT and ES_ENTRY events.
 * @note Remember to rename to something appropriate.
 *       The lower level state machines are run first, to see if the event is dealt
 *       with there rather than at the current level. ES_EXIT and ES_ENTRY events are
 *       not consumed as these need to pass pack to the higher level state machine.
 * @author J. Edward Carryer, 2011.10.23 19:25
 * @author Gabriel H Elkaim, 2011.10.23 19:25 */
ES_Event RunExitHSM(ES_Event ThisEvent)
{
	uint8_t makeTransition = FALSE; // use to flag transition
	ExitState_t nextState;

	static uint8_t trackFlag = FALSE;	// used to check if track wire has been found
	static uint8_t turnedFlag = FALSE;  // used to check if we have flipped 180 already
	static uint8_t bounceCount = 0;		// make sure we dont get stuck in align states

	EventStorage args;

	ES_Tattle(); // trace call stack

	switch (CurrentState) {
	case Exit_Init: // If current state is initial Pseudo State
		if (ThisEvent.EventType == ES_INIT)// only respond to ES_Init
		{
			// Handle initialization of modules

			// Initialize sub HSMs

			// Move to real state
			// These are handled in main() instead
			nextState = Exit_Drive;
			makeTransition = TRUE;
			ThisEvent.EventType = ES_NO_EVENT;
			;
		}
		break;

	case Exit_Drive:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is still active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				// Drive forward
				Drive_Straight(MOTOR_SPEED_CRAWL);

				// Start timer for checking when we have driven far enough to have left the castle
				ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_OUTSIDE);
				break;

			case ES_EXIT:
				// Stop the timer while we handle turns etc, it will restart on entry
				ES_Timer_StopTimer(EXIT_HSM_TIMER);

				Drive_Stop();
				break;

			case TRACK_FOUND:
				trackFlag = TRUE;

				// Consume
				ThisEvent.EventType = ES_NO_EVENT;
				break;

			case BUMPER:
				args.val = ThisEvent.EventParam;

				// if center tripped
				if (args.bits.event  & args.bits.type & BUMP_CENTER) {
					nextState = Exit_Straighten;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;

				}
				// else if left tripped
				else if (args.bits.event & args.bits.type & BUMP_LEFT) {
					nextState = Exit_Align_Left;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				// else if right tripped
				else if (args.bits.event & args.bits.type & BUMP_RIGHT) {
					nextState = Exit_Align_Right;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					// Turn 180
					if (turnedFlag) {
						nextState = Exit_Turn90;
					} else {
						nextState = Exit_Turn180;
					}
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	case Exit_Straighten:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				// Drive straight to recheck bumps
				Drive_Straight(MOTOR_SPEED_CRAWL);

				++bounceCount;

				if (bounceCount == 8) {
					nextState = Exit_Backup;
					makeTransition = TRUE;
				}

				// Set the timeout
				ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_STRAIGHTEN);
				break;

			case ES_EXIT:
				Drive_Stop();

				// Stop timer on exit
				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case TRACK_FOUND:
				trackFlag = TRUE;
				
				// Consume
				ThisEvent.EventType = ES_NO_EVENT;
				break;

			case BUMPER:
				args.val = ThisEvent.EventParam;

				// if left bumper and NOT right bumper
				if ((args.bits.type & BUMP_LEFT) && (~args.bits.type & BUMP_RIGHT)) {
					nextState = Exit_Align_Left;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				// else if right bumper and NOT left bumper
				else if ((args.bits.type & BUMP_RIGHT) && (~args.bits.type & BUMP_LEFT)) {
					nextState = Exit_Align_Right;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				// else if its right AND left or center it will time out and move to backup
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					if (trackFlag) {
						nextState = Exit_Done_Pause;
					} else {
						nextState = Exit_Backup;
					}
					makeTransition = TRUE;

					// Reset our bounce count, it has resolved a collision
					bounceCount = 0;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	// need a timeout for when the center cant be pressed
	case Exit_Align_Left:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				// Turn into the wall to align
				Drive_Right(-MOTOR_SPEED_MEDIUM);

				ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_ALIGN);
				break;

			case ES_EXIT:
				// Stop
				Drive_Stop();

				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					nextState = Exit_Straighten;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	// need a timeout for when the center cant be pressed
	case Exit_Align_Right:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				// Turn into the wall to align
				Drive_Left(-MOTOR_SPEED_MEDIUM);

				ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_ALIGN);
				break;

			case ES_EXIT:
				// Stop
				Drive_Stop();

				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					nextState = Exit_Straighten;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	case Exit_Backup:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				Drive_Straight(-MOTOR_SPEED_CRAWL);

				ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_BACKUP);
				break;

			case ES_EXIT:
				Drive_Stop();

				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					// Turn 90
					nextState = Exit_Turn90;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	case Exit_Turn90:
		if (ThisEvent.EventType != ES_NO_EVENT) { // An event is active
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				Drive_TankRight(MOTOR_SPEED_EXPLORE);

				ES_Timer_InitTimer(EXIT_HSM_TIMER, MOTOR_TURN_EX_90);
				break;

			case ES_EXIT:
				Drive_Stop();

				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					nextState = Exit_Drive;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	case Exit_Turn180:
		if (ThisEvent.EventType != ES_NO_EVENT) {
			switch (ThisEvent.EventType) {
			case ES_ENTRY:
				Drive_TankRight(MOTOR_SPEED_EXPLORE);

				// Set 180 turn flag true, don't do this state twice
				turnedFlag = TRUE;

				ES_Timer_InitTimer(EXIT_HSM_TIMER, MOTOR_TURN_EX_180);
				break;

			case ES_EXIT:
				Drive_Stop();

				ES_Timer_StopTimer(EXIT_HSM_TIMER);
				break;

			case ES_TIMEOUT:
				if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
					nextState = Exit_Drive;
					makeTransition = TRUE;

					ThisEvent.EventType = ES_NO_EVENT;
				}
				break;

			default: // all unhandled events pass the event back up to the next level
				break;
			}
		}
		break;

	case Exit_Done_Pause:
		switch (ThisEvent.EventType) {
		case ES_ENTRY:
			// Reverse
			Drive_Straight(-MOTOR_SPEED_MEDIUM);

			ES_Timer_InitTimer(EXIT_HSM_TIMER, TIME_EXIT_BACKUP);
			break;

		case ES_EXIT:
			break;

		case ES_TIMEOUT:
			if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
				Drive_Stop();

				ES_Timer_InitTimer(STALL_TIMER, STALL_TIME_IN_MS);

				ThisEvent.EventType = ES_NO_EVENT;
			} else if (ThisEvent.EventParam == STALL_TIMER) {
				nextState = Exit_Done_State;
				makeTransition = TRUE;

				ThisEvent.EventType = ES_NO_EVENT;
			}
			break;

		default:
			break;
		}
		break;
	
	case Exit_Done_State:
		switch (ThisEvent.EventType) {
		case ES_ENTRY:
			// Turn 180
			Drive_TankRight(MOTOR_SPEED_EXPLORE);

			ES_Timer_InitTimer(EXIT_HSM_TIMER, MOTOR_TURN_EX_180 - 10);
			break;

		case ES_EXIT:
			// Do nothing
			Drive_Stop();
			break;

		case ES_TIMEOUT:
			if (ThisEvent.EventParam == EXIT_HSM_TIMER) {
				Drive_Stop();

				ThisEvent.EventType = CHILD_DONE;
			}
			break;

		default: // all unhandled events pass the event back up to the next level
			break;
		}
		break;

	default: // all unhandled states fall into here
		break;
	} // end switch on Current State

	if (makeTransition == TRUE) { // making a state transition, send EXIT and ENTRY
		// recursively call the current state with an exit event
		RunExitHSM(EXIT_EVENT);
		CurrentState = nextState;
		RunExitHSM(ENTRY_EVENT);
	}

	ES_Tail(); // trace call stack end
	return ThisEvent;
}


/*******************************************************************************
 * PRIVATE FUNCTIONS                                                           *
 ******************************************************************************/
/*Here's where you put the actual content of your functions.
Example:
 * char RunAway(uint_8 seconds) {
 * Lots of code here
 * } */

/*******************************************************************************
 * TEST HARNESS                                                                *
 ******************************************************************************/
/* Define SEARCHFSM_TEST to run this file as your main file (without the rest
 * of the framework)-useful for debugging */
#ifdef EXITHSM_TEST

#include <stdio.h>

void main(void)
{
	ES_Return_t ErrorType;
	BOARD_Init();
	// When doing testing, it is useful to annouce just which program
	// is running.

	printf("Starting the Exit HSM Test Harness \r\n");
	printf("using the 2nd Generation Events & Services Framework\n\r");

	// Your hardware initialization function calls go here
	Bot_Init();
	Drive_Init();

	// now initialize the Events and Services Framework and start it running
	ErrorType = ES_Initialize();

	if (ErrorType == Success) {
		ErrorType = ES_Run();
	}

	//
	//if we got to here, there was an error
	//

	switch (ErrorType) {
	case FailedPointer:
		printf("Failed on NULL pointer");
		break;
	case FailedInit:
		printf("Failed Initialization");
		break;
	default:
		printf("Other Failure");
		break;
	}

	while (1) {
		;
	}
}

#endif // SEARCHHSM_TEST
