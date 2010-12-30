
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef __WATCOMC__
# include <types.h>
# include <tcpustd.h>
#else
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include <SDL.h>
#include <SDL_thread.h>

#include "controller.h"
#include "OSC-client.h"
#include "osc.h"

#define FOREACH_QUEUE(VAR) \
	for (struct Osc_QueueElement *VAR = Osc_QueueHead.next; VAR; VAR = VAR->next)

static struct Osc_QueueElement {
	struct Osc_QueueElement *next;

	OSCbuf buffer;
} Osc_QueueHead = {NULL};

static struct Osc_QueueElement *Osc_QueueTail = &Osc_QueueHead;

static SDL_mutex *Osc_QueueMutex = NULL;
static SDL_sem *Osc_QueueSemaphore = NULL;

static int SDLCALL Osc_DequeueThread(void *ud);

static inline Uint32 Osc_StrPad32(Uint32 l);
static inline int Osc_BuildFloatMessage(OSCbuf *buf, const char *address,
					float value);

int
Osc_Connect(const char *hostname, int port)
{
	struct hostent		*entry;
	struct sockaddr_in	addr;

	int			fd;

	if (!(entry = gethostbyname(hostname)) ||
	    (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = *(in_addr_t*)*entry->h_addr_list;
	addr.sin_port = htons(port);

	if (connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

SDL_Thread *
Osc_InitThread(int *fd)
{
	SDL_Thread *thread;

	if (!(Osc_QueueMutex = SDL_CreateMutex()))
		return NULL;

	if (!(Osc_QueueSemaphore = SDL_CreateSemaphore(0))) {
		SDL_DestroyMutex(Osc_QueueMutex);
		return NULL;
	}

	if (!(thread = SDL_CreateThread(Osc_DequeueThread, fd))) {
		SDL_DestroyMutex(Osc_QueueMutex);
		SDL_DestroySemaphore(Osc_QueueSemaphore);
	}

	return thread;
}

/*
 * terminate thread without killing it by freeing the remaining queue and
 * increasing the semaphore (gentle termination condition)
 */

int
Osc_TerminateThread(void)
{
	struct Osc_QueueElement *next;

	if (SDL_LockMutex(Osc_QueueMutex))
		return -1;

	for (struct Osc_QueueElement *el = Osc_QueueHead.next; el; el = next) {
		next = el->next;

		free(OSC_getPacket(&el->buffer));
		free(el);
	}

	Osc_QueueHead.next = NULL;
	Osc_QueueTail = &Osc_QueueHead;

	return SDL_UnlockMutex(Osc_QueueMutex) ||
	       SDL_SemPost(Osc_QueueSemaphore);
}

#define THREAD_ABORT() {			\
	if (SDL_PushEvent((SDL_Event*)&abort))	\
		return 1;			\
	continue;				\
}

static int SDLCALL
Osc_DequeueThread(void *ud)
{
	int fd = *(int*)ud;

	static const SDL_Event abort = {
		.type = SDL_USEREVENT,
		.user = {
			.type = SDL_USEREVENT,
			.code = CONTROLLER_ERR_THREAD
		}
	};

	for (;;) {
		struct Osc_QueueElement	*el;
		char			*data;

		fd_set		wrset;
		ssize_t		r;

		struct timeval timeout = {
			.tv_sec = 30,
			.tv_usec = 0
		};

		if (SDL_SemWait(Osc_QueueSemaphore) ||
		    SDL_LockMutex(Osc_QueueMutex))
			THREAD_ABORT();

		if (!(el = Osc_QueueHead.next)) { /* gentle thread termination */
			SDL_DestroyMutex(Osc_QueueMutex);
			SDL_DestroySemaphore(Osc_QueueSemaphore);
			Osc_QueueMutex = NULL;
			Osc_QueueSemaphore = NULL;

			return 0;
		}

		if (SDL_UnlockMutex(Osc_QueueMutex))
			THREAD_ABORT();

		FD_ZERO(&wrset);
		FD_SET(fd, &wrset);

		if (select(fd + 1, NULL, &wrset, NULL, &timeout) <= 0)
			THREAD_ABORT();

		data = OSC_getPacket(&el->buffer);

		do
			r = send(fd, data, OSC_packetSize(&el->buffer), 0);
		while (r < 0 && errno == EINTR);

		if (r <= 0 ||
		    SDL_LockMutex(Osc_QueueMutex))
			THREAD_ABORT();

		if (!(Osc_QueueHead.next = el->next))
			Osc_QueueTail = &Osc_QueueHead;

		if (SDL_UnlockMutex(Osc_QueueMutex))
			THREAD_ABORT();

		free(data);
		free(el);
	}
}

static inline Uint32
Osc_StrPad32(Uint32 l)
{
	Uint32 m = ++l % sizeof(Uint32);
	return m ? l + sizeof(Uint32) - m : l;
}

#if 0
int
Osc_BuildMessage(OSCbuf *buf, const char *address, const char *types, ...)
{
	va_list	args;

	Uint32	size = Osc_StrPad32(strlen(address)) + Osc_StrPad32(strlen(types));
	char	*data = NULL;

	va_start(args, types);

	for (const char *p = types + 1; *p; p++)
		switch (*p) {
		case 'f':
		case 'i':
			va_arg(args, Uint32);
			size += sizeof(Uint32);
			break;

		case 's':
			size += Osc_StrPad32(strlen(va_arg(args, char*)));
			break;

		default:
			goto err;
		}

	va_end(args);
	va_start(args, types);

	if (!(data = malloc(size)))
		goto err;
	OSC_initBuffer(buf, size, data);

	if (OSC_writeAddressAndTypes(buf, (char*)address, (char*)types))
		goto err;

	for (const char *p = types + 1; *p; p++)
		switch (*p) {
		case 'f':
			if (OSC_writeFloatArg(buf, va_arg(args, float)))
				goto err;
			break;

		case 'i':
			if (OSC_writeIntArg(buf, va_arg(args, int)))
				goto err;
			break;

		case 's':
			if (OSC_writeStringArg(buf, va_arg(args, char*)))
				goto err;
			break;
		}

	va_end(args);
	return 0;

err:

	va_end(args);
	free(data);
	return 1;
}
#endif

static inline int
Osc_BuildFloatMessage(OSCbuf *buf, const char *address, float value)
{
	Uint32	size = Osc_StrPad32(strlen(address)) + 4 + sizeof(float);
	char	*data;

	if (!(data = malloc(size)))
		return 1;

	OSC_initBuffer(buf, size, data);

	if (OSC_writeAddressAndTypes(buf, (char*)address, ",f") ||
	    OSC_writeFloatArg(buf, value)) {
		free(data);
		return 1;
	}

	return 0;
}

int
Osc_EnqueueFloatMessage(const char *address, float value)
{
	struct Osc_QueueElement *el;

	if (SDL_LockMutex(Osc_QueueMutex) ||
	    !(el = malloc(sizeof(struct Osc_QueueElement))))
		return 1;
	memset(el, 0, sizeof(struct Osc_QueueElement));

	if (Osc_BuildFloatMessage(&el->buffer, address, value)) {
		free(el);
		return 1;
	}

	Osc_QueueTail->next = el;
	Osc_QueueTail = el;

	return SDL_UnlockMutex(Osc_QueueMutex) ||
	       SDL_SemPost(Osc_QueueSemaphore);
}

