/* MinGW-w64 has no fork(); XPPAUT only uses it to launch an editor. */
#ifndef XPP_COMPAT_SYS_WAIT_H
#define XPP_COMPAT_SYS_WAIT_H
#define fork() (-1)
#define wait(status) (-1)
#endif
