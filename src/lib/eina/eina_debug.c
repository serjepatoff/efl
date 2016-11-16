/* EINA - EFL data type library
 * Copyright (C) 2015 Carsten Haitzler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
# endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <libunwind.h>

#include "eina_debug.h"
#include "eina_types.h"
#include "eina_list.h"
#include "eina_mempool.h"
#include "eina_util.h"
#include "eina_evlog.h"

#ifdef __CYGWIN__
# define LIBEXT ".dll"
#else
# define LIBEXT ".so"
#endif

#define DEBUGON 1
#ifdef DEBUGON
# define e_debug(fmt, args...) fprintf(stderr, "%d:"__FILE__":%s/%d : " fmt "\n", getpid(), __FUNCTION__, __LINE__, ##args)
# define e_debug_begin(fmt, args...) fprintf(stderr, "%d:"__FILE__":%s/%d : " fmt "", getpid(), __FUNCTION__, __LINE__, ##args)
# define e_debug_continue(fmt, args...) fprintf(stderr, fmt, ##args)
# define e_debug_end() fprintf(stderr, "\n")
#else
# define e_debug(x...) do { } while (0)
# define e_debug_begin(x...) do { } while (0)
# define e_debug_continue(x...) do { } while (0)
# define e_debug_end(x...) do { } while (0)
#endif

// yes - a global debug spinlock. i expect contention to be low for now, and
// when needed we can split this up into mroe locks to reduce contention when
// and if that day comes
Eina_Spinlock _eina_debug_lock;

// only init once
static Eina_Bool _inited = EINA_FALSE;
static char *_my_app_name = NULL;

extern Eina_Bool eina_module_init(void);
extern Eina_Bool eina_mempool_init(void);
extern Eina_Bool eina_list_init(void);

#define MAX_EVENTS   16

extern Eina_Spinlock      _eina_debug_thread_lock;
volatile int           _eina_debug_sysmon_reset = 0;
volatile int           _eina_debug_evlog_active = 0;

Eina_Semaphore _eina_debug_monitor_return_sem;
Eina_Lock              _eina_debug_sysmon_lock;

static Eina_Bool       _monitor_thread_runs = EINA_FALSE;
static pthread_t       _monitor_thread;
static pthread_t       _sysmon_thread;

// _bt_buf[0] is always for mainloop, 1 + is for extra threads
static void             ***_bt_buf;
static int                *_bt_buf_len;
static struct timespec    *_bt_ts;
static int                *_bt_cpu;

/* Local session */
static Eina_Debug_Session *_default_session = NULL;
static Eina_Bool _default_connection_disabled = EINA_FALSE;
/* List of existing sessions */
static Eina_List *sessions = NULL;
/* epoll stuff */
#ifndef _WIN32
static int _epfd = -1, _listening_master_fd = -1, _listening_slave_fd = -1;
#endif

/* Used by the daemon to be notified about connections changes */
static Eina_Debug_Connect_Cb _server_conn_cb = NULL;
static Eina_Debug_Disconnect_Cb _server_disc_cb = NULL;

/* Opcode used to load a module
 * needed by the daemon to notify loading success */
static int _module_init_opcode = EINA_DEBUG_OPCODE_INVALID;

/* Used by trace timer */
static double _trace_t0 = 0.0;

// some state for debugging
static unsigned int poll_time = 0;
static Eina_Debug_Timer_Cb poll_timer_cb = NULL;

/* Magic number used in special cases for reliability */
#ifndef _WIN32
static char magic[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
#endif

typedef struct
{
   const Eina_Debug_Opcode *ops;
   Eina_Debug_Opcode_Status_Cb status_cb;
} _opcode_reply_info;

struct _Eina_Debug_Session
{
   Eina_Debug_Cb *cbs; /* Table of callbacks indexed by opcode id */
   Eina_List *opcode_reply_infos;
   Eina_List *script; /* Remaining list of script lines to apply */
   Eina_Debug_Dispatch_Cb dispatch_cb; /* Session dispatcher */
   Eina_Debug_Encode_Cb encode_cb; /* Packet encoder */
   Eina_Debug_Decode_Cb decode_cb; /* Packet decoder */
   double encoding_ratio; /* Encoding ratio */
   int cbs_length; /* cbs table size */
   int fd_in; /* File descriptor to read */
   int fd_out; /* File descriptor to write */
   int max_packet_size; /* Max packet size, useful for shell */
   short segment_sync; /* Data to send between packet segments or at the end */
   Eina_Debug_Session_Type type; /* Session is Master or Slave. Only used by daemon. */
   Eina_Bool magic_on_send : 1; /* Indicator to send magic on send */
   Eina_Bool magic_on_recv : 1; /* Indicator to expect magic on recv */
   Eina_Bool wait_for_input : 1; /* Indicator to wait for input before continuing sending */
};

static void _opcodes_register_all(Eina_Debug_Session *session);
static void _thread_start();

/* Global session used by daemon to store in a common place
 * the information that is common to all the opened sessions*/
Eina_Debug_Session *_global_session = NULL;

EAPI int
eina_debug_session_send(Eina_Debug_Session *session, int dest, int op, void *data, int size)
{
   int total_size = 0;
   unsigned char *buf = NULL, *data_buf = NULL;
   int nb = -1;

   if(!session) session = _default_session;

   if (!session) return -1;
   // send protocol packet. all protocol is an int for size of packet then
   // included in that size (so a minimum size of 4) is a 4 byte opcode
   // (all opcodes are 4 bytes as a string of 4 chars), then the real
   // message payload as a data blob after that
   total_size = size + sizeof(Eina_Debug_Packet_Header);
   buf = alloca(total_size);
   Eina_Debug_Packet_Header *hdr = (Eina_Debug_Packet_Header *)buf;
   hdr->size = total_size - sizeof(int);
   hdr->opcode = op;
   hdr->cid = dest;
   if (size > 0) memcpy(buf + sizeof(Eina_Debug_Packet_Header), data, size);
   if (session->encode_cb)
     {
        int new_size = 0;
        void *new_buf = session->encode_cb(buf, total_size, &new_size);
        buf = new_buf;
        total_size = new_size;
     }
#ifndef _WIN32
   if (session->magic_on_send) write(session->fd_out, magic, 4);
   e_debug("socket: %d / opcode %X / packet size %ld / bytes to send: %d",
         session->fd_out, op, hdr->size + sizeof(int), total_size);
   data_buf = buf;
   while (total_size > 0)
     {
        nb = write(session->fd_out, data_buf,
              session->max_packet_size && total_size > session->max_packet_size ?
              session->max_packet_size : total_size);
        data_buf += nb;
        total_size -= nb;
        if (session->segment_sync)
           write(session->fd_out, &(session->segment_sync), sizeof(session->segment_sync));
     }
   if (session->encode_cb) free(buf);
#endif
   return size;
}

static void
_eina_debug_monitor_service_greet(Eina_Debug_Session *session)
{
   // say hello to our debug daemon - tell them our PID and protocol
   // version we speak
   /* Version + Pid + App name */
   int size = 8 + (_my_app_name ? strlen(_my_app_name) : 0) + 1;
   unsigned char *buf = alloca(size);
   int version = 1; // version of protocol we speak
   int pid = getpid();
   memcpy(buf + 0, &version, 4);
   memcpy(buf + 4, &pid, 4);
   if (_my_app_name)
      memcpy(buf + 8, _my_app_name, strlen(_my_app_name) + 1);
   else
      buf[8] = '\0';
   eina_debug_session_send(session, 0, EINA_DEBUG_OPCODE_HELLO, buf, size);
}

#ifndef _WIN32
/*
 * Used to consume a script line.
 * WAIT token is used to wait for input after a script line has been
 * applied.
 */
static void
_script_consume(Eina_Debug_Session *session)
{
   const char *line = NULL;
   do {
        line = eina_list_data_get(session->script);
        session->script = eina_list_remove_list(session->script, session->script);
        if (line)
          {
             if (!strncmp(line, "WAIT", 4))
               {
                  e_debug("Wait for input");
                  session->wait_for_input = EINA_TRUE;
                  return;
               }
             else if (!strncmp(line, "SLEEP_1", 7))
               {
                  e_debug("Sleep 1s");
                  sleep(1);
               }
             else
               {
                  e_debug("Apply script line: %s", line);
                  write(session->fd_out, line, strlen(line));
                  write(session->fd_out, "\n", 1);
               }
          }
   }
   while (line);
   /* When all the script has been applied, we can begin to send debug packets */
   _eina_debug_monitor_service_greet(session);
   _opcodes_register_all(session);
}

static int
_eina_debug_session_receive(Eina_Debug_Session *session, unsigned char **buffer)
{
   double ratio = 1.0;
   int size_sz = sizeof(int);
   unsigned char *recv_buf = NULL;
   int rret;
   int recv_size = 0;

   if (!session) return -1;

   if (session->wait_for_input)
     {
        /* Wait for input */
        char c;
        e_debug_begin("Characters received: ");
        while (read(session->fd_in, &c, 1) == 1) e_debug_continue("%c", c);
        e_debug_end();
        session->wait_for_input = EINA_FALSE;
        _script_consume(session);
        return 0;
     }

   if (session->magic_on_recv)
     {
        /* All the bytes before the magic field have to be dropped. */
        char c;
        int ret, i = 0;
        do
          {
             ret = read(session->fd_in, &c, 1);
             if (ret != 1) return 0;
             if (c == magic[i]) i++;
             else i = 0;
          }
        while (i < 4);
        e_debug("Magic found");
     }
   ratio = session->decode_cb && session->encoding_ratio ? session->encoding_ratio : 1.0;
   size_sz *= ratio;
   recv_buf = malloc(size_sz);
   /*
    * Retrieve packet size
    * We need to take into account that the size could arrive in more than one chunk
    */
   do
     {
        while ((rret = read(session->fd_in, recv_buf + recv_size, size_sz - recv_size)) == -1 &&
              errno == EAGAIN);
        if (rret <= 0) goto error;
        recv_size += rret;
     }
   while (recv_size != size_sz);

   e_debug("%d - %d", session->fd_in, rret);
   if (rret == size_sz)
     {
        int size = 0;
        if (session->decode_cb)
          {
             /* Decode the size if needed */
             void *size_buf = session->decode_cb(recv_buf, size_sz, NULL);
             size = *(int *)size_buf;
             free(size_buf);
          }
        else size = *(int *)recv_buf;
        e_debug("%d/%d - %d", getpid(), session->fd_in, size);
        // allocate a buffer for the next bytes to receive
        recv_buf = realloc(recv_buf, size * ratio);
        if (recv_buf)
          {
             int cur_packet_size = 0;
             unsigned char *packet_buf = malloc(size + sizeof(int));
             memcpy(packet_buf, &size, sizeof(int));
             /* Receive all the remaining packet bytes */
             while (cur_packet_size < size)
               {
                  while ((rret = read(session->fd_in, recv_buf, (size - cur_packet_size) * ratio)) == -1 &&
                        errno == EAGAIN);
                  if (rret <= 0)
                    {
                       free(recv_buf);
                       free(packet_buf);
                       return -1;
                    }
                  /* Decoding all the packets */
                  if (session->decode_cb)
                    {
                       int decoded_size = 0;
                       char *decoded_buf = session->decode_cb(recv_buf, rret, &decoded_size);
                       if (decoded_buf && decoded_size)
                         {
                            memcpy(packet_buf + cur_packet_size + sizeof(int), decoded_buf, decoded_size);
                            cur_packet_size += decoded_size;
                            free(decoded_buf);
                         }
                    }
                  else
                    {
                       memcpy(packet_buf + cur_packet_size + sizeof(int), recv_buf, rret);
                       cur_packet_size += rret;
                    }
               }
             free(recv_buf);
             if (cur_packet_size != size)
               {
                  // we didn't get payload as expected - error on comms
                  e_debug("EINA DEBUG ERROR: "
                        "Invalid payload+header size: read %i expected %i", cur_packet_size, size);
                  free(packet_buf);
                  return -1;
               }
             else
                *buffer = packet_buf;
             /*
              * In case of an interactive shell, 0x0A is needed at the end of the packet to be sent
              * So we need to retrieve it too.
              * Magic number can't help here because we don't have it in both directions.
              */
             if (session->segment_sync)
               {
                  char c[2];
                  while (read(session->fd_in, c, 2) == -1 && errno == EAGAIN);
               }
             return cur_packet_size;
          }
        else
          {
             // we couldn't allocate memory for payloa buffer
             // internal memory limit error
             e_debug("EINA DEBUG ERROR: "
                   "Cannot allocate %u bytes for op", (unsigned int)(size * ratio));
             return -1;
          }
     }
error:
   if (rret == -1) perror("Read from socket");
   if (rret)
      e_debug("EINA DEBUG ERROR: "
            "Invalid size read %i != %i", rret, size_sz);

   if (recv_buf) free(recv_buf);
   return -1;
}
#endif

EAPI void
eina_debug_default_connection_disable()
{
   _default_connection_disabled = EINA_TRUE;
}

EAPI Eina_Debug_Session *
eina_debug_session_new()
{
   Eina_Debug_Session *session = calloc(1, sizeof(*session));
   session->dispatch_cb = eina_debug_dispatch;
   eina_spinlock_take(&_eina_debug_lock);
   //check if already appended to session list
   sessions = eina_list_append(sessions, session);
   eina_spinlock_release(&_eina_debug_lock);
   return session;
}

/*
 * Used only in the daemon. when set each session will use the global
 * session opcodes.
 */
EAPI void
eina_debug_session_global_use(Eina_Debug_Dispatch_Cb disp_cb)
{
   if(!_global_session)
      _global_session = eina_debug_session_new();
   _global_session->dispatch_cb = disp_cb ? disp_cb : eina_debug_dispatch;
}

EAPI void
eina_debug_session_free(Eina_Debug_Session *session)
{
   if (!session) return;
   eina_spinlock_take(&_eina_debug_lock);
   sessions = eina_list_remove(sessions, session);
   eina_spinlock_release(&_eina_debug_lock);

   _opcode_reply_info *info = NULL;

   EINA_LIST_FREE(session->opcode_reply_infos, info)
      free(info);

   free(session->cbs);
   free(session);
}

EAPI void
eina_debug_session_dispatch_override(Eina_Debug_Session *session, Eina_Debug_Dispatch_Cb disp_cb)
{
   if (!session) session = _default_session;
   if (!session) return;
   if (!disp_cb) disp_cb = eina_debug_dispatch;
   session->dispatch_cb = disp_cb;
}

static void
_sessions_free(void)
{
   while (sessions)
     {
        Eina_Debug_Session *session = eina_list_data_get(sessions);
#ifndef _WIN32
        if (session->fd_in != session->fd_out) close(session->fd_out);
        close(session->fd_in);
#endif
        eina_debug_session_free(session);
     }
}

static Eina_Debug_Session *
_session_find_by_fd(int fd)
{
   Eina_List *l;
   Eina_Debug_Session *session;

   EINA_LIST_FOREACH(sessions, l, session)
      if(session->fd_in == fd)
         return session;

   return NULL;
}

EAPI void
eina_debug_session_fd_attach(Eina_Debug_Session *session, int fd)
{
#ifndef _WIN32
   struct epoll_event event;
   int ret;
#endif
   if (!session) return;
   session->fd_out = session->fd_in = fd;
#ifndef _WIN32
   event.data.fd = fd;
   event.events = EPOLLIN ;
   ret = epoll_ctl (_epfd, EPOLL_CTL_ADD, fd, &event);
   if (ret) perror ("epoll_ctl/add");
#endif
}

EAPI void
eina_debug_session_fd_out_set(Eina_Debug_Session *session, int fd)
{
   if (!session) return;
   session->fd_out = fd;
}

static void
_session_fd_unattach(Eina_Debug_Session *session)
{
#ifndef _WIN32
   int ret = epoll_ctl (_epfd, EPOLL_CTL_DEL, session->fd_in, NULL);
   if (ret) perror ("epoll_ctl/del");
   close(session->fd_in);
   if (session->fd_in != session->fd_out) close(session->fd_out);
#else
   (void) session;
#endif
}

// a backtracer that uses libunwind to do the job
static inline int
_eina_debug_unwind_bt(void **bt, int max)
{
   unw_cursor_t cursor;
   unw_context_t uc;
   unw_word_t p;
   int total;

   // create a context for unwinding
   unw_getcontext(&uc);
   // begin our work
   unw_init_local(&cursor, &uc);
   // walk up each stack frame until there is no more, storing it
   for (total = 0; (unw_step(&cursor) > 0) && (total < max); total++)
     {
        unw_get_reg(&cursor, UNW_REG_IP, &p);
        bt[total] = (void *)p;
     }
   // return our total backtrace stack size
   return total;
}

// this signal handler is called inside each and every thread when the
// thread gets a signal via pthread_kill(). this causes the thread to
// stop here inside this handler and "do something" then when this returns
// resume whatever it was doing like any signal handler
static void
_eina_debug_signal(int sig EINA_UNUSED,
                   siginfo_t *si EINA_UNUSED,
                   void *foo EINA_UNUSED)
{
   int i, slot = 0;
   pthread_t self = pthread_self();
   clockid_t cid;

   // find which slot in the array of threads we have so we store info
   // in the correct slot for us
   if (self != _eina_debug_thread_mainloop)
     {
        for (i = 0; i < _eina_debug_thread_active_num; i++)
          {
             if (self == _eina_debug_thread_active[i].thread)
               {
                  slot = i + 1;
                  goto found;
               }
          }
        // we couldn't find out thread reference! help!
        e_debug("EINA DEBUG ERROR: can't find thread slot!");
        eina_semaphore_release(&_eina_debug_monitor_return_sem, 1);
        return;
     }
found:
   // store thread info like what cpu core we are on now (not reliable
   // but hey - better than nothing), the amount of cpu time total
   // we have consumed (it's cumulative so subtracing deltas can give
   // you an average amount of cpu time consumed between now and the
   // previous time we looked) and also a full backtrace
   _bt_cpu[slot] = sched_getcpu();
   pthread_getcpuclockid(self, &cid);
   clock_gettime(cid, &(_bt_ts[slot]));
   _bt_buf_len[slot] = _eina_debug_unwind_bt(_bt_buf[slot], EINA_MAX_BT);
   // now wake up the monitor to let them know we are done collecting our
   // backtrace info
   eina_semaphore_release(&_eina_debug_monitor_return_sem, 1);
}

// we shall sue SIGPROF as out signal for pausing threads and having them
// dump a backtrace for polling based profiling
#define SIG SIGPROF

// a quick and dirty local time point getter func - not portable
static inline double
get_time(void)
{
#if defined(__clockid_t_defined)
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (double)t.tv_sec + (((double)t.tv_nsec) / 1000000000.0);
#else
   struct timeval timev;
   gettimeofday(&timev, NULL);
   return (double)timev.tv_sec + (((double)timev.tv_usec) / 1000000.0);
#endif
}

static void
_eina_debug_collect_bt(pthread_t pth)
{
   // this async signals the thread to switch to the deebug signal handler
   // and collect a backtrace and other info from inside the thread
   pthread_kill(pth, SIG);
}

// this is a DEDICATED thread tojust collect system info and to have the
// least impact it can on a cpu core or system. all this does right now
// is sleep and wait for a command to begin polling for the cpu state.
// right now that means iterating through cpu's and getting their cpu
// frequency to match up with event logs.
static void *
_eina_debug_sysmon(void *_data EINA_UNUSED)
{
   static int cpufreqs[64] = { 0 };
   int i, fd, freq;
   char buf[256], path[256];
   ssize_t red;
#if defined(__clockid_t_defined)
   static struct timespec t_last = { 0, 0 };
   static Eina_Debug_Thread *prev_threads = NULL;
   static int prev_threads_num = 0;
   int j, cpu;
   Eina_Bool prev_threads_redo;
   clockid_t cid;
   struct timespec t, t_now;
   unsigned long long tim_span, tim1, tim2;
#endif

   // set a name for this thread for system debugging
#ifdef EINA_HAVE_PTHREAD_SETNAME
# ifndef __linux__
   pthread_set_name_np
# else
      pthread_setname_np
# endif
      (pthread_self(), "Edbg-sys");
#endif
   for (;;)
     {
        // wait on a mutex that will be locked for as long as this
        // threead is not meant to go running
        eina_lock_take(&_eina_debug_sysmon_lock);
        // if we need to reset as we just started polling system stats...
        if (_eina_debug_sysmon_reset)
          {
             _eina_debug_sysmon_reset = 0;
             // clear out all the clocks for threads
#if defined(__clockid_t_defined)
             // reset the last clock timestamp when we checked to "now"
             clock_gettime(CLOCK_MONOTONIC, &t);
             t_last = t;
             // walk over all threads
             eina_spinlock_take(&_eina_debug_thread_lock);
             for (i = 0; i < _eina_debug_thread_active_num; i++)
               {
                  // get the correct clock id to use for the target thread
                  pthread_getcpuclockid
                     (_eina_debug_thread_active[i].thread, &cid);
                  // get the clock cpu time accumulation for that threas
                  clock_gettime(cid, &t);
                  _eina_debug_thread_active[i].clok = t;
               }
             eina_spinlock_release(&_eina_debug_thread_lock);
#endif
             // clear all the cpu freq (up to 64 cores) to 0
             for (i = 0; i < 64; i++) cpufreqs[i] = 0;
          }
        eina_lock_release(&_eina_debug_sysmon_lock);

#if defined(__clockid_t_defined)
        // get the current time
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        tim1 = (t_last.tv_sec * 1000000000LL) + t_last.tv_nsec;
        // the time span between now and last time we checked
        tim_span = ((t_now.tv_sec * 1000000000LL) + t_now.tv_nsec) - tim1;
        // if the time span is non-zero we might get sensible results
        if (tim_span > 0)
          {
             prev_threads_redo = EINA_FALSE;
             eina_spinlock_take(&_eina_debug_thread_lock);
             // figure out if the list of thread id's has changed since
             // our last poll. this is imporant as we need to set the
             // thread cpu usage to 0 for threads that have disappeared
             if (prev_threads_num != _eina_debug_thread_active_num)
               prev_threads_redo = EINA_TRUE;
             else
               {
                  // XXX: isolate this out of hot path
                  for (i = 0; i < _eina_debug_thread_active_num; i++)
                    {
                       if (_eina_debug_thread_active[i].thread !=
                           prev_threads[i].thread)
                         {
                            prev_threads_redo = EINA_TRUE;
                            break;
                         }
                    }
               }
             for (i = 0; i < _eina_debug_thread_active_num; i++)
               {
                  pthread_t thread = _eina_debug_thread_active[i].thread;
                  // get the clock for the thread and its cpu time usage
                  pthread_getcpuclockid(thread, &cid);
                  clock_gettime(cid, &t);
                  // calculate a long timestamp (64bits)
                  tim1 = (_eina_debug_thread_active[i].clok.tv_sec * 1000000000LL) +
                          _eina_debug_thread_active[i].clok.tv_nsec;
                  // and get the difference in clock time in NS
                  tim2 = ((t.tv_sec * 1000000000LL) + t.tv_nsec) - tim1;
                  // and that as percentage of the timespan
                  cpu = (int)((100 * (int)tim2) / (int)tim_span);
                  // round to the nearest 10 percent - rough anyway
                  cpu = ((cpu + 5) / 10) * 10;
                  if (cpu > 100) cpu = 100;
                  // if the usage changed since last time we checked...
                  if (cpu != _eina_debug_thread_active[i].val)
                    {
                       // log this change
                       snprintf(buf, sizeof(buf), "*CPUUSED %llu",
                                (unsigned long long)thread);
                       snprintf(path, sizeof(path), "%i", _eina_debug_thread_active[i].val);
                       eina_evlog(buf, NULL, 0.0, path);
                       snprintf(path, sizeof(path), "%i", cpu);
                       eina_evlog(buf, NULL, 0.0, path);
                       // store the clock time + cpu we got for next poll
                       _eina_debug_thread_active[i].val = cpu;
                    }
                  _eina_debug_thread_active[i].clok = t;
               }
             // so threads changed between this poll and last so we need
             // to redo our mapping/storage of them
             if (prev_threads_redo)
               {
                  prev_threads_redo = EINA_FALSE;
                  // find any threads from our last run that do not
                  // exist now in our new list of threads
                  for (j = 0; j < prev_threads_num; j++)
                    {
                       for (i = 0; i < _eina_debug_thread_active_num; i++)
                         {
                            if (prev_threads[j].thread ==
                                _eina_debug_thread_active[i].thread) break;
                         }
                       // thread was there before and not now
                       if (i == _eina_debug_thread_active_num)
                         {
                            // log it finishing - ie 0
                            snprintf(buf, sizeof(buf), "*CPUUSED %llu",
                                     (unsigned long long)
                                     prev_threads[i].thread);
                            eina_evlog(buf, NULL, 0.0, "0");
                         }
                    }
                  // if the thread count changed then allocate a new shadow
                  // buffer of thread id's etc.
                  if (prev_threads_num != _eina_debug_thread_active_num)
                    {
                       if (prev_threads) free(prev_threads);
                       prev_threads_num = _eina_debug_thread_active_num;
                       prev_threads = malloc(prev_threads_num *
                                             sizeof(Eina_Debug_Thread));
                    }
                  // store the thread handles/id's
                  for (i = 0; i < prev_threads_num; i++)
                    prev_threads[i].thread =
                    _eina_debug_thread_active[i].thread;
               }
             eina_spinlock_release(&_eina_debug_thread_lock);
             t_last = t_now;
          }
#endif
        // poll up to 64 cpu cores for cpufreq info to log alongside
        // the evlog call data
        for (i = 0; i < 64; i++)
          {
             // look at sysfs nodes per cpu
             snprintf
               (buf, sizeof(buf),
                "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_cur_freq", i);
             fd = open(buf, O_RDONLY);
             freq = 0;
             // if the node is there, read it
             if (fd >= 0)
               {
                  // really low overhead read from cpufreq node (just an int)
                  red = read(fd, buf, sizeof(buf) - 1);
                  if (red > 0)
                    {
                       // read something - it should be an int with whitespace
                       buf[red] = 0;
                       freq = atoi(buf);
                       // move to mhz
                       freq = (freq + 500) / 1000;
                       // round mhz to the nearest 100mhz - to have less noise
                       freq = ((freq + 50) / 100) * 100;
                    }
                  // close the fd so we can freshly poll next time around
                  close(fd);
               }
             // node not there - ran out of cpu's to poll?
             else break;
             // if the current frequency changed vs previous poll, then log
             if (freq != cpufreqs[i])
               {
                  snprintf(buf, sizeof(buf), "*CPUFREQ %i", i);
                  snprintf(path, sizeof(path), "%i", freq);
                  eina_evlog(buf, NULL, 0.0, path);
                  cpufreqs[i] = freq;
               }
          }
        usleep(1000); // 1ms sleep
     }
   return NULL;
}

static Eina_Bool
_trace_cb()
{
   static int bts = 0;
   int i;
   if (!_trace_t0) _trace_t0 = get_time();
   // take a lock on grabbing thread debug info like backtraces
   eina_spinlock_take(&_eina_debug_thread_lock);
   // reset our "stack" of memory se use to dump thread info into
   _eina_debug_chunk_tmp_reset();
   // get an array of pointers for the backtrace array for main + th
   _bt_buf = _eina_debug_chunk_tmp_push
      ((1 + _eina_debug_thread_active_num) * sizeof(void *));
   if (!_bt_buf) goto err;
   // get an array of pointers for the timespec array for mainloop + th
   _bt_ts = _eina_debug_chunk_tmp_push
      ((1 + _eina_debug_thread_active_num) * sizeof(struct timespec));
   if (!_bt_ts) goto err;
   // get an array of pointers for the cpuid array for mainloop + th
   _bt_cpu = _eina_debug_chunk_tmp_push
      ((1 + _eina_debug_thread_active_num) * sizeof(int));
   if (!_bt_cpu) goto err;
   // now get an array of void pts for mainloop bt
   _bt_buf[0] = _eina_debug_chunk_tmp_push(EINA_MAX_BT * sizeof(void *));
   if (!_bt_buf[0]) goto err;
   // get an array of void ptrs for each thread we know about for bt
   for (i = 0; i < _eina_debug_thread_active_num; i++)
     {
        _bt_buf[i + 1] = _eina_debug_chunk_tmp_push(EINA_MAX_BT * sizeof(void *));
        if (!_bt_buf[i + 1]) goto err;
     }
   // get an array of ints to stor the bt len for mainloop + threads
   _bt_buf_len = _eina_debug_chunk_tmp_push
      ((1 + _eina_debug_thread_active_num) * sizeof(int));
   // collect bt from the mainloop - always there
   _eina_debug_collect_bt(_eina_debug_thread_mainloop);
   // now collect per thread
   for (i = 0; i < _eina_debug_thread_active_num; i++)
      _eina_debug_collect_bt(_eina_debug_thread_active[i].thread);
   // we're done probing. now collec all the "i'm done" msgs on the
   // semaphore for every thread + mainloop
   for (i = 0; i < (_eina_debug_thread_active_num + 1); i++)
      eina_semaphore_lock(&_eina_debug_monitor_return_sem);
   // we now have gotten all the data from all threadd + mainloop.
   // we can process it now as we see fit, so release thread lock
   //// XXX: some debug so we can see the bt's we collect - will go
   //                  for (i = 0; i < (_eina_debug_thread_active_num + 1); i++)
   //                    {
   //                       _eina_debug_dump_fhandle_bt(stderr, _bt_buf[i], _bt_buf_len[i]);
   //                    }
err:
   eina_spinlock_release(&_eina_debug_thread_lock);
   //// XXX: some debug just to see how well we perform - will go
   bts++;
   if (bts >= 10000)
     {
        double t;
        t = get_time();
        e_debug("%1.5f bt's per sec", (double)bts / (t - _trace_t0));
        _trace_t0 = t;
        bts = 0;
     }
   return EINA_TRUE;
}

// profiling on with poll time gap as uint payload
static Eina_Bool
_eina_debug_prof_on_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer, int size)
{
   unsigned int time;
   if (size >= 4)
     {
        memcpy(&time, buffer, 4);
        _trace_t0 = 0.0;
        eina_debug_timer_add(time, _trace_cb);
     }
   return EINA_TRUE;
}

static Eina_Bool
_eina_debug_prof_off_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer EINA_UNUSED, int size EINA_UNUSED)
{
   eina_debug_timer_add(0, NULL);
   return EINA_TRUE;
}

static Eina_Bool
_eina_debug_cpufreq_on_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer EINA_UNUSED, int size EINA_UNUSED)
{
   if (!_eina_debug_evlog_active)
     {
        _eina_debug_evlog_active = 1;
        eina_evlog_start();
        _eina_debug_sysmon_reset = 1;
        eina_lock_release(&_eina_debug_sysmon_lock);
     }
   return EINA_TRUE;
}

static Eina_Bool
_eina_debug_cpufreq_off_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer EINA_UNUSED, int size EINA_UNUSED)
{
   if (_eina_debug_evlog_active)
     {
        eina_lock_take(&_eina_debug_sysmon_lock);
        eina_evlog_stop();
        _eina_debug_evlog_active = 0;
     }
   return EINA_TRUE;
}

typedef struct {
     Eina_Module *handle;
     int (*init)(void);
     int (*shutdown)(void);
} _module_info;

#define _LOAD_SYMBOL(cls_struct, pkg, sym) \
   do \
     { \
        char func_name[1024]; \
        snprintf(func_name, sizeof(func_name), "%s_debug_" #sym, pkg); \
        (cls_struct).sym = eina_module_symbol_get((cls_struct).handle, func_name); \
        if (!(cls_struct).sym) \
          { \
             e_debug("Failed loading symbol '%s' from the library.", func_name); \
             eina_module_free((cls_struct).handle); \
             (cls_struct).handle = NULL; \
             return EINA_FALSE; \
          } \
     } \
   while (0)

static Eina_Bool
_module_init_cb(Eina_Debug_Session *session, int cid, void *buffer, int size)
{
   _module_info minfo;
   char module_path[1024];
   const char *module_name = buffer;
   if (size <= 0) return EINA_FALSE;
   e_debug("Init module %s", module_name);
   snprintf(module_path, sizeof(module_path), PACKAGE_LIB_DIR "/lib%s_debug"LIBEXT, module_name);
   minfo.handle = eina_module_new(module_path);
   if (!minfo.handle || !eina_module_load(minfo.handle))
     {
        e_debug("Failed loading debug module %s.", module_name);
        if (minfo.handle) eina_module_free(minfo.handle);
        minfo.handle = NULL;
        return EINA_TRUE;
     }

   _LOAD_SYMBOL(minfo, module_name, init);
   _LOAD_SYMBOL(minfo, module_name, shutdown);

   minfo.init();

   eina_debug_session_send(session, cid, _module_init_opcode, buffer, size);
   return EINA_TRUE;
}

static Eina_Bool
_module_shutdown_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer EINA_UNUSED, int size EINA_UNUSED)
{
   return EINA_TRUE;
}

static const Eina_Debug_Opcode _EINA_DEBUG_MONITOR_OPS[] = {
       {"profiler/on", NULL, &_eina_debug_prof_on_cb},
       {"profiler/off", NULL, &_eina_debug_prof_off_cb},
       {"cpufreq/on", NULL, &_eina_debug_cpufreq_on_cb},
       {"cpufreq/off", NULL, &_eina_debug_cpufreq_off_cb},
       {"module/init", &_module_init_opcode, &_module_init_cb},
       {"module/shutdown", NULL, &_module_shutdown_cb},
       {NULL, NULL, NULL}
};

static void
_opcodes_register(void)
{
   eina_debug_opcodes_register(NULL, _EINA_DEBUG_MONITOR_OPS, NULL);
}

/*
 * Response of the daemon containing the ids of the requested opcodes.
 * PTR64 + (opcode id)*
 */
static Eina_Bool
_callbacks_register_cb(Eina_Debug_Session *session, int src_id EINA_UNUSED, void *buffer, int size)
{
   _opcode_reply_info *info = NULL;
   int *os;
   unsigned int count, i;

   uint64_t info_64;
   memcpy(&info_64, buffer, sizeof(uint64_t));
   info = (_opcode_reply_info *)info_64;

   if (!info) return EINA_FALSE;

   os = (int *)((unsigned char *)buffer + sizeof(uint64_t));
   count = (size - sizeof(uint64_t)) / sizeof(int);

   for (i = 0; i < count; i++)
     {
        if (info->ops[i].opcode_id) *(info->ops[i].opcode_id) = os[i];
        eina_debug_static_opcode_register(session, os[i], info->ops[i].cb);
        e_debug("Opcode %s -> %d", info->ops[i].opcode_name, os[i]);
     }
   if (info->status_cb) info->status_cb(EINA_TRUE);

   return EINA_TRUE;
}

static void
_opcodes_registration_send(Eina_Debug_Session *session,
      _opcode_reply_info *info)
{
   unsigned char *buf;

   int count = 0;
   int size = sizeof(uint64_t);

   if(!session) session = _default_session;
   if(!session) return;

   while(info->ops[count].opcode_name)
     {
        size += strlen(info->ops[count].opcode_name) + 1;
        count++;
     }

   buf = alloca(size);

   uint64_t info_64 = (uint64_t)info;
   memcpy(buf, &info_64, sizeof(uint64_t));
   int size_curr = sizeof(uint64_t);

   count = 0;
   while(info->ops[count].opcode_name)
     {
        int len = strlen(info->ops[count].opcode_name) + 1;
        memcpy(buf + size_curr, info->ops[count].opcode_name, len);
        size_curr += len;
        count++;
     }

   eina_debug_session_send(session, 0,
         EINA_DEBUG_OPCODE_REGISTER,
         buf,
         size);
}

static void
_opcodes_register_all(Eina_Debug_Session *session)
{
   Eina_List *l;
   _opcode_reply_info *info = NULL;

   eina_debug_static_opcode_register(session,
         EINA_DEBUG_OPCODE_REGISTER, _callbacks_register_cb);
   EINA_LIST_FOREACH(session->opcode_reply_infos, l, info)
        _opcodes_registration_send(session, info);;
}

static void
_opcodes_unregister_all(Eina_Debug_Session *session)
{
   Eina_List *l;
   _opcode_reply_info *info = NULL;

   if (!session) return;
   session->cbs_length = 0;
   free(session->cbs);
   session->cbs = NULL;

   EINA_LIST_FOREACH(session->opcode_reply_infos, l, info)
     {
        const Eina_Debug_Opcode *op = info->ops;
        while(!op->opcode_name)
          {
             if (op->opcode_id) *(op->opcode_id) = EINA_DEBUG_OPCODE_INVALID;
             op++;
          }
        if (info->status_cb) info->status_cb(EINA_FALSE);
     }
}

static const char *
_socket_home_get()
{
   // get possible debug daemon socket directory base
   const char *dir = getenv("XDG_RUNTIME_DIR");
   if (!dir) dir = eina_environment_home_get();
   if (!dir) dir = eina_environment_tmp_get();
   return dir;
}

#define SERVER_PATH ".edebug"
#define SERVER_NAME "efl_debug"
#define SERVER_MASTER_PORT 0
#define SERVER_SLAVE_PORT 1

#ifndef _WIN32
#define LENGTH_OF_SOCKADDR_UN(s) \
   (strlen((s)->sun_path) + (size_t)(((struct sockaddr_un *)NULL)->sun_path))
#endif

EAPI Eina_Bool
eina_debug_local_connect(Eina_Debug_Session *session, Eina_Debug_Session_Type type)
{
#ifndef _WIN32
   char buf[4096];
   int fd, socket_unix_len, curstate = 0;
   struct sockaddr_un socket_unix;
#endif

   if (!session) return EINA_FALSE;
   // try this socket file - it will likely be:
   //   ~/.ecore/efl_debug/0
   // or maybe
   //   /var/run/UID/.ecore/efl_debug/0
   // either way a 4k buffer should be ebough ( if it's not we're on an
   // insane system)
#ifndef _WIN32
   snprintf(buf, sizeof(buf), "%s/%s/%s/%i", _socket_home_get(), SERVER_PATH, SERVER_NAME,
         type == EINA_DEBUG_SESSION_MASTER ? SERVER_MASTER_PORT : SERVER_SLAVE_PORT);
   // create the socket
   fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) goto err;
   // set the socket to close when we exec things so they don't inherit it
   if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) goto err;
   // set up some socket options on addr re-use
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&curstate,
                  sizeof(curstate)) < 0)
     goto err;
   // sa that it's a unix socket and where the path is
   socket_unix.sun_family = AF_UNIX;
   strncpy(socket_unix.sun_path, buf, sizeof(socket_unix.sun_path) - 1);
   socket_unix_len = LENGTH_OF_SOCKADDR_UN(&socket_unix);
   // actually conenct to efl_debugd service
   if (connect(fd, (struct sockaddr *)&socket_unix, socket_unix_len) < 0)
      goto err;
   // we succeeded
   eina_debug_session_fd_attach(session, fd);
   _eina_debug_monitor_service_greet(session);
   _opcodes_register_all(session);
   // start the monitor thread
   _thread_start();
   return EINA_TRUE;
err:
   // some kind of connection failure here, so close a valid socket and
   // get out of here
   if (fd >= 0) close(fd);
#else
   (void) session;
   (void) type;
#endif
   return EINA_FALSE;
}

EAPI Eina_Bool
eina_debug_shell_remote_connect(Eina_Debug_Session *session, const char *cmd, Eina_List *script)
{
#ifndef _WIN32
   int pipeToShell[2], pipeFromShell[2];
   int pid = -1;
   pipe(pipeToShell);
   pipe(pipeFromShell);

   pid = fork();
   if (pid == -1) return EINA_FALSE;
   if (!pid)
     {
        int i = 0;
        const char *args[16] = { 0 };
        char *tmp = strdup(cmd);
        /* Child */
        close(STDIN_FILENO);
        dup2(pipeToShell[0], STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(pipeFromShell[1], STDOUT_FILENO);
        args[i++] = tmp;
        do
          {
             tmp = strchr(tmp, ' ');
             if (tmp)
               {
                  *tmp = '\0';
                  args[i++] = ++tmp;
               }
          }
        while (tmp);
        args[i++] = 0;
        execv(args[0], (char **)args);
        perror("execv");
        _exit(-1);
     }
   else
     {
        /* Parent */
        eina_debug_session_fd_attach(session, pipeFromShell[0]);
        eina_debug_session_magic_set_on_recv(session);
        if (fcntl(session->fd_in, F_SETFL, O_NONBLOCK) == -1) perror(0);
        session->fd_out = pipeToShell[1];
        session->script = script;
        _script_consume(session);
        // start the monitor thread
        _thread_start();
     }
   return EINA_TRUE;
#else
   (void) session;
   (void) cmd;
   (void) script;
   return EINA_FALSE;
#endif
}

#ifndef _WIN32
static int
_local_listening_socket_create(const char *path)
{
   struct sockaddr_un socket_unix;
   int socket_unix_len, curstate = 0;
   // create the socket
   int fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) goto err;
   // set the socket to close when we exec things so they don't inherit it
   if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) goto err;
   // set up some socket options on addr re-use
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&curstate,
                  sizeof(curstate)) < 0)
     goto err;
   // sa that it's a unix socket and where the path is
   socket_unix.sun_family = AF_UNIX;
   strncpy(socket_unix.sun_path, path, sizeof(socket_unix.sun_path) - 1);
   socket_unix_len = LENGTH_OF_SOCKADDR_UN(&socket_unix);
   unlink(socket_unix.sun_path);
   if (bind(fd, (struct sockaddr *)&socket_unix, socket_unix_len) < 0)
     {
        perror("ERROR on binding");
        goto err;
     }
   listen(fd, 5);
   return fd;
err:
   if (fd >= 0) close(fd);
   return -1;
}
#endif

EAPI Eina_Bool
eina_debug_server_launch(Eina_Debug_Connect_Cb conn_cb, Eina_Debug_Disconnect_Cb disc_cb)
{
#ifndef _WIN32
   char buf[4096];
   struct epoll_event event = {0};
   mode_t mask = 0;
   const char *socket_home_path = _socket_home_get();
   char *socket_path = NULL;
   if (!socket_home_path) return EINA_FALSE;
   socket_path = strdup(socket_home_path);

   snprintf(buf, sizeof(buf), "%s/%s", socket_path, SERVER_PATH);
   if (mkdir(buf, S_IRWXU) < 0 && errno != EEXIST)
     {
        perror("mkdir SERVER_PATH");
        goto err;
     }
   snprintf(buf, sizeof(buf), "%s/%s/%s", socket_path, SERVER_PATH, SERVER_NAME);
   if (mkdir(buf, S_IRWXU) < 0 && errno != EEXIST)
     {
        perror("mkdir SERVER_NAME");
        goto err;
     }
   mask = umask(S_IRWXG | S_IRWXO);
   snprintf(buf, sizeof(buf), "%s/%s/%s/%i", socket_path, SERVER_PATH, SERVER_NAME, SERVER_MASTER_PORT);
   _listening_master_fd = _local_listening_socket_create(buf);
   if (_listening_master_fd <= 0) goto err;
   event.data.fd = _listening_master_fd;
   event.events = EPOLLIN;
   epoll_ctl (_epfd, EPOLL_CTL_ADD, _listening_master_fd, &event);
   snprintf(buf, sizeof(buf), "%s/%s/%s/%i", socket_path, SERVER_PATH, SERVER_NAME, SERVER_SLAVE_PORT);
   _listening_slave_fd = _local_listening_socket_create(buf);
   if (_listening_slave_fd <= 0) goto err;
   event.data.fd = _listening_slave_fd;
   event.events = EPOLLIN;
   epoll_ctl (_epfd, EPOLL_CTL_ADD, _listening_slave_fd, &event);
   umask(mask);
#endif

   _server_conn_cb = conn_cb;
   _server_disc_cb = disc_cb;
   // start the monitor thread
   _thread_start();
   return EINA_TRUE;
#ifndef _WIN32
err:
   if (mask) umask(mask);
   if (_listening_master_fd >= 0) close(_listening_master_fd);
   _listening_master_fd = -1;
   if (_listening_slave_fd >= 0) close(_listening_slave_fd);
   _listening_slave_fd = -1;
#endif
   free(socket_path);
   return EINA_FALSE;
}

EAPI Eina_Bool
eina_debug_timer_add(unsigned int timeout_ms, Eina_Debug_Timer_Cb cb)
{
   poll_time = timeout_ms;
   poll_timer_cb = cb;
   return EINA_TRUE;
}

static void
_signal_init(void)
{
   struct sigaction sa;

   // set up signal handler for our profiling signal - eevery thread should
   // obey this (this is the case on linux - other OSs may vary)
   sa.sa_sigaction = _eina_debug_signal;
   sa.sa_flags = SA_RESTART | SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIG, &sa, NULL) != 0)
     e_debug("EINA DEBUG ERROR: Can't set up sig %i handler!", SIG);

   sa.sa_sigaction = NULL;
   sa.sa_handler = SIG_IGN;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   if (sigaction(SIGPIPE, &sa, 0) == -1) perror(0);
}

static void
_session_disconnect(Eina_Debug_Session *session)
{
   _session_fd_unattach(session);
   _opcodes_unregister_all(session);
   session->fd_in = session->fd_out = -1;
   if (_server_disc_cb) _server_disc_cb(session);
}

// this is a DEDICATED debug thread to monitor the application so it works
// even if the mainloop is blocked or the app otherwise deadlocked in some
// way. this is an alternative to using external debuggers so we can get
// users or developers to get useful information about an app at all times
static void *
_monitor(void *_data EINA_UNUSED)
{
#ifndef _WIN32
   int ret;
   struct epoll_event events[MAX_EVENTS];

   // set a name for this thread for system debugging
#ifdef EINA_HAVE_PTHREAD_SETNAME
# ifndef __linux__
   pthread_set_name_np
# else
   pthread_setname_np
# endif
     (pthread_self(), "Edbg-mon");
#endif

   // set up our profile signal handler
   _signal_init();

   // sit forever processing commands or timeouts in the debug monitor
   // thread - this is separate to the rest of the app so it shouldn't
   // impact the application specifically
   for (;;)
     {
        // if we are in a polling mode then set up a timeout and wait for it
        int timeout = poll_time ? (int)poll_time : -1; //in milliseconds

        ret = epoll_wait (_epfd, events, MAX_EVENTS, timeout);

        // if the fd for debug daemon says it's alive, process it
        if (ret)
          {
             int i;
             //check which fd are set/ready for read
             for (i = 0; i < ret; i++)
               {
                  if (events[i].events & EPOLLHUP)
                    {
                       Eina_Debug_Session *session = _session_find_by_fd(events[i].data.fd);
                       _session_disconnect(session);
                    }
                  if (events[i].events & EPOLLIN)
                    {
                       int size;
                       unsigned char *buffer;

                       // A master client wants to connect
                       if(events[i].data.fd == _listening_master_fd)
                         {
                            int new_fd = accept(_listening_master_fd, NULL, NULL);
                            if (new_fd < 0) perror("Accept");
                            else
                              {
                                 Eina_Debug_Session *new_fd_session = eina_debug_session_new();
                                 eina_debug_session_fd_attach(new_fd_session, new_fd);
                                 new_fd_session->type = EINA_DEBUG_SESSION_MASTER;
                                 if (_server_conn_cb) _server_conn_cb(new_fd_session);
                              }
                            continue;
                         }

                       // A slave client wants to connect
                       if(events[i].data.fd == _listening_slave_fd)
                         {
                            int new_fd = accept(_listening_slave_fd, NULL, NULL);
                            if (new_fd < 0) perror("Accept");
                            else
                              {
                                 Eina_Debug_Session *new_fd_session = eina_debug_session_new();
                                 eina_debug_session_fd_attach(new_fd_session, new_fd);
                                 new_fd_session->type = EINA_DEBUG_SESSION_SLAVE;
                                 if (_server_conn_cb) _server_conn_cb(new_fd_session);
                              }
                            continue;
                         }

                       Eina_Debug_Session *session =
                          _session_find_by_fd(events[i].data.fd);
                       if(session)
                          {
                             size = _eina_debug_session_receive(session, &buffer);
                             // if not negative - we have a real message
                             if (size > 0)
                               {
                                  Eina_Debug_Session *_disp_session = _global_session ? _global_session : session;
                                  if(!_disp_session->dispatch_cb(session, buffer))
                                    {
                                       // something we don't understand
                                       e_debug("EINA DEBUG ERROR: Unknown command");
                                    }
                               }
                             else if (size == 0)
                               {
                                  // May be due to a response from a script line
                               }
                             else
                               {
                                  // major failure on debug daemon control fd - get out of here.
                                  //   else goto fail;
                                  _session_disconnect(session);
                                  //TODO if its not main session we will tell the main_loop
                                  //that it disconneted
                               }
                          }
                    }
               }
          }
        else
           if (poll_time && poll_timer_cb)
             {
                if (!poll_timer_cb()) poll_time = 0;
             }
     }
#endif
   // free sessions and close fd's
   _sessions_free();
   // sleep forever because there is currently no logic to join this thread
   for (;;) pause();
   return NULL;
}

// start up the debug monitor if we haven't already
static void
_thread_start()
{
   int err;
   sigset_t oldset, newset;

   // if it's already running - we're good.
   if (_monitor_thread_runs) return;
   sigemptyset(&newset);
   sigaddset(&newset, SIGPIPE);
   sigaddset(&newset, SIGALRM);
   sigaddset(&newset, SIGCHLD);
   sigaddset(&newset, SIGUSR1);
   sigaddset(&newset, SIGUSR2);
   sigaddset(&newset, SIGHUP);
   sigaddset(&newset, SIGQUIT);
   sigaddset(&newset, SIGINT);
   sigaddset(&newset, SIGTERM);
#ifdef SIGPWR
   sigaddset(&newset, SIGPWR);
#endif
   sigprocmask(SIG_BLOCK, &newset, &oldset);

   //register opcodes for monitor - should be only once
   _opcodes_register();


   eina_lock_new(&_eina_debug_sysmon_lock);
   eina_lock_take(&_eina_debug_sysmon_lock);
   err = pthread_create(&_sysmon_thread, NULL, _eina_debug_sysmon, NULL);
   if (err != 0)
     {
        e_debug("EINA DEBUG ERROR: Can't create debug thread 1!");
        abort();
     }
   err = pthread_create(&_monitor_thread, NULL, _monitor, NULL);
   sigprocmask(SIG_SETMASK, &oldset, NULL);
   if (err != 0)
     {
        e_debug("EINA DEBUG ERROR: Can't create debug thread 2!");
        abort();
     }
   _monitor_thread_runs = EINA_TRUE;
}

EAPI void
eina_debug_static_opcode_register(Eina_Debug_Session *session,
      int op_id, Eina_Debug_Cb cb)
{
   if(_global_session) session = _global_session;

   if(session->cbs_length < op_id + 1)
     {
        int i = session->cbs_length;
        session->cbs_length = op_id + 16;
        session->cbs = realloc(session->cbs, session->cbs_length * sizeof(Eina_Debug_Cb));
        for(; i < session->cbs_length; i++)
           session->cbs[i] = NULL;
     }
   session->cbs[op_id] = cb;
}

/*
 * Sends to daemon:
 * - Pointer to ops: returned in the response to determine which opcodes have been added
 * - List of opcode names seperated by \0
 */
EAPI void
eina_debug_opcodes_register(Eina_Debug_Session *session, const Eina_Debug_Opcode ops[],
      Eina_Debug_Opcode_Status_Cb status_cb)
{
   Eina_Debug_Session *opcodes_session = session;
   if(!opcodes_session) opcodes_session = _default_session;
   if(_global_session) opcodes_session = _global_session;
   if(!opcodes_session) return;

   _opcode_reply_info *info = malloc(sizeof(*info));
   info->ops = ops;
   info->status_cb = status_cb;

   opcodes_session->opcode_reply_infos = eina_list_append(
         opcodes_session->opcode_reply_infos, info);

   //send only if session's fd connected, if not -  it will be sent when connected
   if(opcodes_session && opcodes_session->fd_in && !opcodes_session->script)
      _opcodes_registration_send(opcodes_session, info);
}

Eina_Bool
eina_debug_dispatch(Eina_Debug_Session *session, void *buffer)
{
   Eina_Debug_Packet_Header *hdr =  buffer;
   int opcode = hdr->opcode;
   Eina_Debug_Cb cb = NULL;
   Eina_Debug_Session *opcodes_session = _global_session ? _global_session : session;

   if(opcode < opcodes_session->cbs_length) cb = opcodes_session->cbs[opcode];

   if (cb)
     {
        cb(session, hdr->cid,
              (unsigned char *)buffer + sizeof(*hdr),
              hdr->size + sizeof(int) - sizeof(*hdr));
        free(buffer);
        return EINA_TRUE;
     }
   else e_debug("Invalid opcode %d", opcode);
   free(buffer);
   return EINA_FALSE;
}

/*
 * Encoder for shell sessions
 * Each byte is encoded in two bytes.
 * 0x0A is appended at the end of the new buffer, as it is needed by shells
 */
static void *
_shell_encode_cb(const void *data, int src_size, int *dest_size)
{
   const char *src = data;
   int new_size = src_size * 2;
   char *dest = malloc(new_size);
   int i;
   for (i = 0; i < src_size; i++)
     {
        dest[(i << 1) + 0] = ((src[i] & 0xF0) >> 4) + 0x40;
        dest[(i << 1) + 1] = ((src[i] & 0x0F) >> 0) + 0x40;
     }
   if (dest_size) *dest_size = new_size;
   return dest;
}

/*
 * Decoder for shell sessions
 * Each two bytes are merged into one byte.
 * The appended 0x0A appended during encoding cannot be handled
 * in this function, as it is not a part of the packet.
 */
static void *
_shell_decode_cb(const void *data, int src_size, int *dest_size)
{
   const char *src = data;
   int i = 0, j;
   char *dest = malloc(src_size / 2);
   if (!dest) goto error;
   for (i = 0, j = 0; j < src_size; j++)
     {
        if ((src[j] & 0xF0) == 0x40 && (src[j + 1] & 0xF0) == 0x40)
          {
             dest[i++] = ((src[j] - 0x40) << 4) | ((src[j + 1] - 0x40));
             j++;
          }
     }
   goto end;
error:
   free(dest);
   dest = NULL;
end:
   if (dest_size) *dest_size = i;
   return dest;
}

EAPI void
eina_debug_session_codec_hooks_add(Eina_Debug_Session *session,
      Eina_Debug_Encode_Cb enc_cb, Eina_Debug_Decode_Cb dec_cb, double encoding_ratio)
{
   if (!session) return;
   session->encode_cb = enc_cb;
   session->decode_cb = dec_cb;
   session->encoding_ratio = encoding_ratio;
}

EAPI void eina_debug_session_basic_codec_add(Eina_Debug_Session *session, Eina_Debug_Basic_Codec codec)
{
   switch(codec)
     {
      case EINA_DEBUG_CODEC_SHELL:
         eina_debug_session_codec_hooks_add(session, _shell_encode_cb, _shell_decode_cb, 2.0);
         session->max_packet_size = 4000;
         session->segment_sync = 0x0A0A;
         break;
      default:
         e_debug("EINA DEBUG ERROR: Bad basic encoding");
     }
}

EAPI void
eina_debug_session_magic_set_on_send(Eina_Debug_Session *session)
{
   if (!session) return;
   session->magic_on_send = EINA_TRUE;
}

EAPI void
eina_debug_session_magic_set_on_recv(Eina_Debug_Session *session)
{
   if (!session) return;
   session->magic_on_recv = EINA_TRUE;
}

EAPI Eina_Debug_Session_Type
eina_debug_session_type_get(const Eina_Debug_Session *session)
{
   return session ? session->type : EINA_DEBUG_SESSION_SLAVE;
}

#ifdef __linux__
   extern char *__progname;
#endif

Eina_Bool
eina_debug_init(void)
{
   pthread_t self;

   // if already inbitted simply release our lock that we may have locked on
   // shutdown if we are re-initted again in the same process
   if (_inited)
     {
        eina_spinlock_release(&_eina_debug_thread_lock);
        return EINA_TRUE;
     }
   // mark as initted
   _inited = EINA_TRUE;
   eina_module_init();
   eina_mempool_init();
   eina_list_init();
#ifdef __linux__
   _my_app_name = __progname;
#endif
   // For Windows support GetModuleFileName can be used
   // set up thread things
   eina_spinlock_new(&_eina_debug_lock);
   eina_spinlock_new(&_eina_debug_thread_lock);
   eina_semaphore_new(&_eina_debug_monitor_return_sem, 0);
   self = pthread_self();
   _eina_debug_thread_mainloop_set(&self);
   _eina_debug_thread_add(&self);
#ifndef _WIN32
   _epfd = epoll_create (MAX_EVENTS);
#endif
#if defined(HAVE_GETUID) && defined(HAVE_GETEUID)
   // if we are setuid - don't debug!
   if (getuid() != geteuid()) return EINA_TRUE;
#endif
   // if someone uses the EFL_NODEBUG env var - do not do debugging. handy
   // for when this debug code is buggy itself
   if (getenv("EFL_NODEBUG")) return EINA_TRUE;
   if (!_default_connection_disabled)
     {
        _default_session = eina_debug_session_new();
        eina_debug_local_connect(_default_session, EINA_DEBUG_SESSION_SLAVE);
     }
   return EINA_TRUE;
}

Eina_Bool
eina_debug_shutdown(void)
{
   eina_spinlock_take(&_eina_debug_thread_lock);
   // yes - we never free on shutdown - this is because the monitor thread
   // never exits. this is not a leak - we intend to never free up any
   // resources here because they are allocated once only ever.
   return EINA_TRUE;
}
