/*
A software button debouncing library.
2019-2021 Ben Hencke

This library debounces a periodically sampled button, providing
easy to use state flags and events. It handles click as well as
hold events, with configurable timing.

Initialize the button state and settings with buttonInit().

Button states are sampled periodically and fed to buttonProcess()
which handles debounce and hold timers, and sets event and state flags.

The period between samples should be roughly the same, and are
the same time unit used for debounce and hold time settings.

The buttonProcess() call will let you know if anything changed, and the
down, up, click, hold, and release events can be polled for and consumed
with the respective poll functions.


Timing Diagram
                                    Hold Time
                                    <------------->
              <--->            <--->
              Debounce Time    Debounce Time
Input    +-+  +-------+        +----------------------+
       +-+ +--+       +--------+                      +-----------+
                      Debounce Time                   Debounce Time
                      <--->                           <--->

isDown            +-------+        +----------------------+
       +----------+       +--------+                      +-------+

isHeld                                            +-------+
       +------------------------------------------+       +-------+

downEvent         +                +
       +----------+----------------+------------------------------+

upEvent                   +                               +
       +------------------+-------------------------------+-------+

clickEvent                +
       +------------------+---------------------------------------+

holdEvent                                         +
       +------------------------------------------+---------------+

releaseEvent                                              +
       +--------------------------------------------------+-------+

 */

#ifndef __BUTTON_H
#define __BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct ButtonState {
	uint16_t holdCountSetting;
	uint16_t holdCounter;
	uint8_t debounceCountSetting;
	uint8_t debounceCounter;

	//current state
	uint8_t isDown:1;
	uint8_t isHeld:1;

	//consumable events
	uint8_t downEvent:1;
	uint8_t upEvent:1;
	uint8_t clickEvent:1;
	uint8_t holdEvent:1;
	uint8_t releaseEvent:1;
};

typedef volatile struct ButtonState ButtonState;


void buttonInit(ButtonState *state, uint8_t debounceCountSetting, uint16_t holdCountSetting);

//update button state given the current sample, returns nonzero if a new state transition occurred
bool buttonProcess(ButtonState *state, bool sample);

//These helper poll functions read and consumes an event flag
bool buttonPollDownEvent(ButtonState *state);
bool buttonPollUpEvent(ButtonState *state);
bool buttonPollClickEvent(ButtonState *state);
bool buttonPollHoldEvent(ButtonState *state);
bool buttonPollReleaseEvent(ButtonState *state);


#ifdef __cplusplus
}
#endif

#endif //__BUTTON_H
