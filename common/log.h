/*
   Copyright (C) 2012 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef H_SPICE_LOG
#define H_SPICE_LOG

#include <stdarg.h>
#include <stdio.h>
#include <glib.h>
#include <spice/macros.h>

#include "macros.h"
#include "recorder/recorder.h"

SPICE_BEGIN_DECLS

#ifdef SPICE_LOG_DOMAIN
#error Do not use obsolete SPICE_LOG_DOMAIN macro, is currently unused
#endif

#ifndef SPICE_RECORD_DOMAIN
#define SPICE_RECORD_DOMAIN     spice
#endif

#ifndef SPICE_INFO_RECORDER
#define SPICE_INFO_RECORDER      G_PASTE(SPICE_RECORD_DOMAIN,_info)
#endif // SPICE_INFO_RECORDER

#ifndef SPICE_DEBUG_RECORDER
#define SPICE_DEBUG_RECORDER     G_PASTE(SPICE_RECORD_DOMAIN,_debug)
#endif // SPICE_DEBUG_RECORDER

#ifndef SPICE_WARNING_RECORDER
#define SPICE_WARNING_RECORDER   G_PASTE(SPICE_RECORD_DOMAIN,_warning)
#endif // SPICE_WARNING_RECORDER

#ifndef SPICE_ERROR_RECORDER
#define SPICE_ERROR_RECORDER     G_PASTE(SPICE_RECORD_DOMAIN,_error)
#endif // SPICE_ERROR_RECORDER

#ifndef SPICE_CRITICAL_RECORDER
#define SPICE_CRITICAL_RECORDER  G_PASTE(SPICE_RECORD_DOMAIN,_critical)
#endif // SPICE_CRITICAL_RECORDER

RECORDER_DECLARE(spice_info);
RECORDER_DECLARE(spice_debug);
RECORDER_DECLARE(spice_warning);
RECORDER_DECLARE(spice_error);
RECORDER_DECLARE(spice_critical);

#define SPICE_STRLOC  __FILE__ ":" G_STRINGIFY (__LINE__)

void spice_log(GLogLevelFlags log_level,
               const char *strloc,
               const char *function,
               const char *format,
               ...) SPICE_ATTR_PRINTF(4, 5);

#define spice_printerr(format, ...) G_STMT_START {                      \
        spice_error(format, ## __VA_ARGS__);                            \
    } G_STMT_END

#define spice_info(format, ...) G_STMT_START {                          \
        RECORD(SPICE_INFO_RECORDER, format, ## __VA_ARGS__);            \
    } G_STMT_END

#define spice_debug(format, ...) G_STMT_START {                         \
        RECORD(SPICE_DEBUG_RECORDER, format, ## __VA_ARGS__);           \
    } G_STMT_END

#define spice_warning(format, ...) G_STMT_START {                       \
 fprintf(stderr, "spice_warning (format=%s)\n", format);         \
        RECORD(SPICE_WARNING_RECORDER, format, ## __VA_ARGS__);         \
    } G_STMT_END

#define spice_critical(format, ...) G_STMT_START {                      \
        RECORD(SPICE_CRITICAL_RECORDER, format, ## __VA_ARGS__);        \
    } G_STMT_END

#define spice_error(format, ...) G_STMT_START {                         \
        RECORD(SPICE_ERROR_RECORDER, format, ## __VA_ARGS__);           \
    } G_STMT_END

#define spice_return_if_fail(x) G_STMT_START {                          \
        if G_LIKELY(x) { } else {                                       \
            spice_critical("condition `%s' failed", #x);                \
            return;                                                     \
        }                                                               \
    } G_STMT_END

#define spice_return_val_if_fail(x, val) G_STMT_START {                 \
        if G_LIKELY(x) { } else {                                       \
            spice_critical("condition `%s' failed", #x);                \
            return (val);                                               \
        }                                                               \
    } G_STMT_END

#define spice_warn_if_reached() G_STMT_START {                          \
        spice_warning("should not be reached");                         \
    } G_STMT_END

#define spice_warn_if_fail(x) G_STMT_START {            \
        if G_LIKELY(x) { } else {                       \
            spice_warning("condition `%s' failed", #x); \
        }                                               \
    } G_STMT_END

#define spice_assert(x) G_STMT_START {                  \
        if G_LIKELY(x) { } else {                       \
            spice_error("assertion `%s' failed", #x);   \
        }                                               \
    } G_STMT_END

#if ENABLE_EXTRA_CHECKS
enum { spice_extra_checks = 1 };
#else
enum { spice_extra_checks = 0 };
#endif

SPICE_END_DECLS

#endif /* H_SPICE_LOG */
