/*
A software button debouncing library.
2019-2021 Ben Hencke
*/

#include "button.h"
#include "string.h"

void buttonInit(ButtonState *state, uint8_t debounceCountSetting, uint16_t holdCountSetting) {
	memset((void *)state, 0, sizeof(ButtonState));
	state->debounceCountSetting = debounceCountSetting;
	state->holdCountSetting = holdCountSetting;
}

bool buttonProcess(ButtonState *state, bool sample) {
	bool stateChanged = false;
	if (sample) {
		if (state->debounceCounter < state->debounceCountSetting) {
			state->debounceCounter++;
		}
	} else {
		if (state->debounceCounter > 0) {
			state->debounceCounter--;
		}
	}

	if (state->debounceCounter == 0 && state->isDown) {
		state->isDown = false;
		state->upEvent = stateChanged = true;
		if (state->isHeld) {
			state->isHeld = false;
			state->holdCounter = 0;
			state->releaseEvent = true;
		} else {
			state->clickEvent = true;
		}
	} else if (state->debounceCounter >= state->debounceCountSetting && !state->isDown) {
		state->isDown = state->downEvent = stateChanged = true;
	}

	if (state->isDown) {
		if (state->holdCounter < state->holdCountSetting) {
			state->holdCounter++;
		}
	} else {
		//isDown is already debounced, so we fully reset holdCounter
		state->holdCounter = 0;
	}

	if (state->holdCounter >= state->holdCountSetting && !state->isHeld) {
		state->isHeld = state->holdEvent = stateChanged = true;
	}

	return stateChanged;
}


bool buttonPollDownEvent(ButtonState *state) {
	if (state->downEvent) {
		state->downEvent = false;
		return true;
	}
	return false;
}

bool buttonPollUpEvent(ButtonState *state) {
	if (state->upEvent) {
		state->upEvent = false;
		return true;
	}
	return false;
}

bool buttonPollClickEvent(ButtonState *state) {
	if (state->clickEvent) {
		state->clickEvent = false;
		return true;
	}
	return false;
}

bool buttonPollHoldEvent(ButtonState *state) {
	if (state->holdEvent) {
		state->holdEvent = false;
		return true;
	}
	return false;
}

bool buttonPollReleaseEvent(ButtonState *state) {
	if (state->releaseEvent) {
		state->releaseEvent = false;
		return true;
	}
	return false;
}

