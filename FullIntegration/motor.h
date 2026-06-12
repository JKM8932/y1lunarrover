#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>

void motor_init(void);

/*
 forward:
 1  = forwards
 0  = stop
 -1  = backwards
 
 turn:
 1  = turn right
 0  = straight
 -1  = turn left
 
 speed_percentage:
  motor speed percentage, clamped to 80-100 inside motor.c
 */
void motor_write(int forward, int turn, int speed_percentage);

#endif
