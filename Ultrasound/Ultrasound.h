#ifndef ULTRASOUND_H
#define ULTRASOUND_H

void ultrasound_init(void);
void ultrasound_start_detection(void);

// Returns:
//   0 = no final result yet
//   1 = no 40kHz signal
//   2 = weak / uncertain signal
//   3 = 40kHz signal detected
int ultrasound_poll(void);

#endif
