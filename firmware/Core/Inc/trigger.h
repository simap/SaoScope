/*
 * trigger.h
 *
 *  Created on: Jan 16, 2024
 *      Author: benh
 */

#ifndef INC_TRIGGER_H_
#define INC_TRIGGER_H_

#include "main.h"


void disarmTrigger();
void armTrigger(uint32_t holdoff, uint16_t level, uint32_t callbackDelay);


#endif /* INC_TRIGGER_H_ */
