
#ifndef __OSC_H
#define __OSC_H

#include <unistd.h>
#ifdef __WATCOMC__
#include <types.h>
#endif
#include <sys/socket.h>

#include <SDL.h>
#include <SDL_thread.h>

		/* mainly for Open Watcom C */

#ifndef HAVE_IN_ADDR_T
#define HAVE_IN_ADDR_T
typedef Uint32 in_addr_t;
#endif

#if !(defined(SHUT_RD) || defined(SHUT_WR) || defined(SHUT_RDWR))
enum {
	SHUT_RD = 0,          /* No more receptions.  */
#define SHUT_RD SHUT_RD
	SHUT_WR,              /* No more transmissions.  */
#define SHUT_WR SHUT_WR
	SHUT_RDWR             /* No more receptions or transmissions.  */
#define SHUT_RDWR SHUT_RDWR
};
#endif

int Osc_Connect(const char *hostname, int port);
static inline void Osc_Disconnect(int fd);

SDL_Thread *Osc_InitThread(int *fd);
int Osc_TerminateThread(void);

int Osc_EnqueueFloatMessage(const char *address, float value);

static inline void
Osc_Disconnect(int fd)
{
	shutdown(fd, SHUT_WR);
	close(fd);
}

enum Osc_DataType {
	OSC_INT = 0,
	OSC_FLOAT,
	OSC_DOUBLE,
	OSC_STRING,
	OSC_BOOL
};
#define OSC_DATATYPE	\
	"int\0"		\
	"float\0"	\
	"double\0"	\
	"string\0"	\
	"bool\0"

#endif
