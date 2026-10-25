/* The local channel through which Python drives the running app
   (channel.c; the client is xpplab.app.connect()). */
#ifndef XPPS_CHANNEL_H
#define XPPS_CHANNEL_H

#include <stdbool.h>

#include "app.h"

/* Listen on 127.0.0.1 and write the port and token file. */
bool channel_start(App *a);
/* Once a frame: take new clients and answer one request of each. */
void channel_poll(App *a);
/* Close every connection and remove the file. */
void channel_stop(App *a);
int channel_clients(void);
const char *channel_path(void); /* the port and token file, once started */

#endif
