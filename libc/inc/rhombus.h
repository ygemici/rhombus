/*
 * Copyright (C) 2011 Nick Johnson <nickbjohnson4224 at gmail.com>
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __RLIBC_RHOMBUS_H
#define __RLIBC_RHOMBUS_H

#include <stdint.h>
#include <proc.h>

/*****************************************************************************
 * Resource pointers
 *
 * This stuff right here, this is important. Resource pointers are the way
 * that Rhombus indentifies pretty much everything on the system. Every file,
 * directory, link, thread, process, etc. can be uniquely identified by a 
 * resource pointer, which, because it is an integer type, is easy to pass to 
 * and from functions. Resource pointers also have canonical string 
 * representations which are used in the rcall and event interfaces, and are 
 * human-readable.
 *
 * The string representations have the following format:
 *
 *   @p:i
 *
 * Where p is the PID of the pointer and i is the index, both in decimal.
 * For example, if the PID is 42 and the index is 123, the string 
 * representation would be "@42:123".
 *
 * There are also some macros used to portably interface with robject
 * pointer contents:
 *
 *   RP_CONS(pid, index) - construct a rp with the given PID and index.
 *   RP_PID(rp)          - evaluate to the PID of the rp.
 *   RP_INDEX(rp)        - evaluate to the index of the rp.
 *   RP_HEAD(rp)         - evaluate to the "head" of a pointer's process
 *   RP_NULL             - universally represents no robject; false-like
 *
 *   RP_CURRENT_THREAD   - the resource pointer of the current thread
 *   RP_CURRENT_PROC     - the resource pointer of the whole process
 *
 * The way that the bits are arranged in a resource pointer makes it so that
 * if an rp_t is cast to a PID, it evaluates to the PID of the process owning
 * the resource, and so if a PID is cast to an rp_t, it evaluates to the
 * resource pointer pointing to that process's head.
 *
 * The "head" of a process is a resource that represents both the main thread 
 * of that process and the process itself. It may be used as both a resource
 * or a thread.
 */

// guaranteed to be an integer type
typedef uint64_t rp_t;

#define RP_CONS(pid, idx) ((((uint64_t) (idx)) << 32) | (uint64_t) (pid))
#define RP_INDEX(rp)      ((uint32_t) ((rp) >> 32))
#define RP_PID(rp)        ((uint32_t) ((rp) & 0xFFFFFFFF))
#define RP_HEAD(rp)       ((uint64_t) ((rp) & 0xFFFFFFFF))
#define RP_NULL           ((uint64_t) 0)

#define RP_CURRENT_THREAD RP_CONS(getpid(), -gettid())
#define RP_CURRENT_PROC   RP_CONS(getpid(), 0)

// convert resource pointer to string
char *rtoa(rp_t rp);

// convert string to resource pointer
rp_t ator(const char *str);

/*****************************************************************************
 * Connections
 *
 * For most operations (like I/O), a connection must be established to a 
 * resource R. This connection has some status, which indicates what sort of 
 * operations are going to be performed with it. 
 *
 * There are a small set of status flags currently standardized. All flags
 * with values below 16 bits are reserved, but all flags with values above
 * 16 bits are available for ad-hoc use.
 *
 *   STAT_OPEN - connection established; automatically set for all connections.
 *
 *   STAT_READER - A intends to and is permitted to read data
 *
 *   STAT_WRITER - A intends to and is permitted to write data
 *
 *   STAT_ADMIN - A intends to and is permitted to modify access bitmaps.
 *
 *   STAT_EVENT - A will recieve events generated by B
 *
 * The kernel also maintains a list of open connections for each process 
 * called the "rtab", for the sole purpose of sending close messages to 
 * drivers when that process exits. This table can have elements removed by 
 * the process holding the connection (this sends the message), and can have 
 * elements added by the process controlling resource R.
 */

#define STAT_OPEN	0x01
#define STAT_READER	0x02
#define STAT_WRITER	0x04
#define STAT_ADMIN	0x08
#define STAT_EVENT	0x10

int rp_setstat(rp_t rp, int status);
int rp_clrstat(rp_t rp, int status);
int rp_getstat(rp_t rp);
int __reconnect(void);

// add a connection to the rtab
int rtab_open(uint32_t pid, rp_t rp);

// remove a connection from the rtab (and implcitly send close message)
int rtab_close(rp_t rp);

/*****************************************************************************
 * UNIX-style File Descriptors
 */

int  fd_alloc(void);
int  fd_set  (int fd, rp_t rp, int mode);
int  fd_mode (int fd);
rp_t fd_rp   (int fd);
int  fd_free (int fd);

int      fd_seek(int fd, uint64_t pos);
uint64_t fd_pos (int fd);

int ropen(int fd, rp_t rp, int mode);
int close(int fd);
int dup  (int fd);
int dup2 (int fd, int newfd);

/*****************************************************************************
 * Resource Access Control Lists
 *
 * Every resource has a set of access bitmaps that determine which processes
 * and users can perform which functions on that resource. 
 *
 * The following flags correspond to bits that may be set in the access
 * bitmap. Permissions can be assigned on a per-user basis.
 *
 * ACCS_READ  - 0x2
 *
 * This flag allows read access. For directories and links, this means 
 * finding and listing. For files, this means reading file contents.
 *
 * ACCS_WRITE - 0x4
 *
 * This flag allows write access. For directories, this means the creation and
 * deletion of hard links. For files, this means writing, clearing, and 
 * deleting files/file contents, as well as requesting file synchronization.
 * 
 * ACCS_ALTER - 0x8
 *
 * This flag allows the access bitmap to be modified. Some drivers simply
 * do not allow certain operations (usually writing, if the filesystem is
 * read-only) and this does not ensure that the permission bitmap will 
 * actually be modified as specified.
 *
 * ACCS_EVENT - 0x10
 *
 * This flag allows events to be listened to. Some resources simply do not
 * emit events.
 */

#define ACCS_READ	STAT_READER
#define ACCS_WRITE	STAT_WRITER
#define ACCS_ALTER	STAT_ADMIN
#define ACCS_EVENT	STAT_EVENT

int rp_access(rp_t rp, uint32_t user);
int rp_admin (rp_t rp, uint32_t user, int access);

/*****************************************************************************
 * High Level Message Passing (rcall)
 *
 * Resources can accept a bunch of I/O specific messages (see natio.h), but
 * more commonly use a generic text-based message protocol called rcall.
 *
 * This protocol is very simple. The only argument is an ASCII string, and the
 * only return value is an ASCII string. Within the argument, there is some
 * structure, however. The argument string is interpreted as a sequence of
 * space-separated tokens (much like the arguments to a command line utility),
 * with the first token being the name of the function to be called.
 */

// perform an rcall
char *rcall(rp_t rp, const char *fmt, ...);

// root rcall hook format
typedef char *(*rcall_hook_t)(rp_t src, int argc, char **argv);

// set the rcall hook for a given rcall function
int    rcall_hook(const char *func, rcall_hook_t hook);
char  *rcall_call(rp_t source, const char *args);
void __rcall_init(void);

/**************************************************************************** 
 * High Level Event System (event)
 *
 * The event protocol is an asyncronous, broadcasting parallel of the rcall 
 * protocol. Only a single ASCII string is sent as event data, and events are 
 * sent from robjects to processes. Each resource maintains a list of "event 
 * subscribers", to which messages are sent if an event is to be sent from 
 * that resource.
 *
 * Instead of a method name like in rcall, event uses the first token of the
 * argument string as an event type, which is used to route it. Event types
 * should be used to group similar events together (like keypress events, or
 * mouse movement events, or window events.)
 */

// manage event sources
int event_subscribe  (rp_t event_source);
int event_unsubscribe(rp_t event_source);

// event hook format
typedef void (*event_t)(rp_t src, int argc, char **argv);

// set the event hook for the given event type
int event_hook(const char *type, event_t hook);

// send an event
int event(rp_t rp, const char *value);

/*****************************************************************************
 * Resource Type System
 *
 * Different resources implement represent different types of things and
 * implement different rcall interfaces, and therefore it is convenient to
 * be able to determine whether a resource is of a certain type. Resource
 * types are represented as space-separaed string lists of type names, 
 * effectively sets.
 *
 * To determine whether a resource <R> implements a type <type>, use 
 * rp_type(R, type).
 *
 * The standard types are as follows:
 *
 *   "basic" - implements the basic resource interface:
 *     type
 *     ping
 *     name
 *     open
 *     stat
 *     find
 *     get-access
 *     set-access
 *
 *   "event" - is capable of (but not guaranteed to be) emitting events.
 *
 *   "file" - implements I/O handlers and the basic file interface:
 *     reset
 *     size
 *
 *   "dir" - implements the basic directory interface:
 *     list
 *     link
 *     unlink
 *
 *   "link" - implements the basic symbolic link interface:
 *     set-link
 *     get-link
 */

int rp_type(rp_t rp, const char *type);

#endif/*__RLIBC_RHOMBUS_H*/
