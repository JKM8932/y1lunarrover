#ifndef INFRARED_H
#define INFRARED_H

void infrared_init(void);
void infrared_start_detection(void);

// Returns:
//   0   = no result yet
//   312 = detected 312 Hz
//   547 = detected 547 Hz
int infrared_poll(void);

#endif
