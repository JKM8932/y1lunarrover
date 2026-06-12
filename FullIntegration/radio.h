#ifndef RADIO_H
#define RADIO_H

void radio_init(void);
void radio_start_detection(void);

// Returns NULL if no completed result yet.
// Returns pointer to detected radio message when ready.
const char *radio_poll(void);

#endif
