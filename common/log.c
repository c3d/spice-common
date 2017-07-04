/*
   Copyright (C) 2012-2015 Red Hat, Inc.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "log.h"
#include "backtrace.h"

static int glib_debug_level = INT_MAX;
static int abort_mask = 0;

#ifndef SPICE_ABORT_MASK_DEFAULT
#define SPICE_ABORT_MASK_DEFAULT (G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_ERROR)
#endif

#define G_LOG_DOMAIN "Spice"

typedef enum {
    SPICE_LOG_LEVEL_ERROR,
    SPICE_LOG_LEVEL_CRITICAL,
    SPICE_LOG_LEVEL_WARNING,
    SPICE_LOG_LEVEL_INFO,
    SPICE_LOG_LEVEL_DEBUG,
} SpiceLogLevel;

static GLogLevelFlags spice_log_level_to_glib(SpiceLogLevel level)
{
    static const GLogLevelFlags glib_levels[] = {
        [ SPICE_LOG_LEVEL_ERROR ] = G_LOG_LEVEL_ERROR,
        [ SPICE_LOG_LEVEL_CRITICAL ] = G_LOG_LEVEL_CRITICAL,
        [ SPICE_LOG_LEVEL_WARNING ] = G_LOG_LEVEL_WARNING,
        [ SPICE_LOG_LEVEL_INFO ] = G_LOG_LEVEL_INFO,
        [ SPICE_LOG_LEVEL_DEBUG ] = G_LOG_LEVEL_DEBUG,
    };
    g_return_val_if_fail (level >= 0, G_LOG_LEVEL_ERROR);
    g_return_val_if_fail (level < G_N_ELEMENTS(glib_levels), G_LOG_LEVEL_DEBUG);

    return glib_levels[level];
}

static void spice_log_set_debug_level(void)
{
    if (glib_debug_level == INT_MAX) {
        const char *debug_str = g_getenv("SPICE_DEBUG_LEVEL");
        if (debug_str != NULL) {
            int debug_level;
            char *debug_env;

            /* FIXME: To be removed after enough deprecation time */
            g_warning("Setting SPICE_DEBUG_LEVEL is deprecated, use G_MESSAGES_DEBUG instead");
            debug_level = atoi(debug_str);
            if (debug_level > SPICE_LOG_LEVEL_DEBUG) {
                debug_level = SPICE_LOG_LEVEL_DEBUG;
            }
            glib_debug_level = spice_log_level_to_glib(debug_level);

            /* If the debug level is too high, make sure we don't try to enable
             * display of glib debug logs */
            if (debug_level < SPICE_LOG_LEVEL_INFO)
                return;

            /* Make sure GLib default log handler will show the debug messages. Messing with
             * environment variables like this is ugly, but this only happens when the legacy
             * SPICE_DEBUG_LEVEL is used
             */
            debug_env = (char *)g_getenv("G_MESSAGES_DEBUG");
            if (debug_env == NULL) {
                g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, FALSE);
            } else {
                debug_env = g_strconcat(debug_env, " ", G_LOG_DOMAIN, NULL);
                g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, FALSE);
                g_free(debug_env);
            }
        }
    }
}

static void spice_log_set_abort_level(void)
{
    if (abort_mask == 0) {
        const char *abort_str = g_getenv("SPICE_ABORT_LEVEL");
        if (abort_str != NULL) {
            GLogLevelFlags glib_abort_level;

            /* FIXME: To be removed after enough deprecation time */
            g_warning("Setting SPICE_ABORT_LEVEL is deprecated, use G_DEBUG instead");
            glib_abort_level = spice_log_level_to_glib(atoi(abort_str));
            unsigned int fatal_mask = G_LOG_FATAL_MASK;
            while (glib_abort_level >= G_LOG_LEVEL_ERROR) {
                fatal_mask |= glib_abort_level;
                glib_abort_level >>= 1;
            }
            abort_mask = fatal_mask;
            g_log_set_fatal_mask(G_LOG_DOMAIN, fatal_mask);
        } else {
            abort_mask = SPICE_ABORT_MASK_DEFAULT;
        }
    }
}

static void spice_logger(const gchar *log_domain,
                         GLogLevelFlags log_level,
                         const gchar *message,
                         gpointer user_data G_GNUC_UNUSED)
{
    if ((log_level & G_LOG_LEVEL_MASK) > glib_debug_level) {
        return; // do not print anything
    }
    g_log_default_handler(log_domain, log_level, message, NULL);
}

static void spice_recorder_format(recorder_show_fn show,
                                  void *output,
                                  const char *label,
                                  const char *location,
                                  uintptr_t order,
                                  uintptr_t timestamp,
                                  const char *message)
{
    GLogLevelFlags log_level = G_LOG_LEVEL_INFO;
    const char *name = strrchr(label, '_');
    if (name)
    {
        name++;
        switch(*name)
        {
        case 'e': case 'E': log_level = G_LOG_LEVEL_ERROR; break;
        case 'i': case 'I': log_level = G_LOG_LEVEL_INFO; break;
        case 'w': case 'W': log_level = G_LOG_LEVEL_WARNING; break;
        case 'c': case 'C': log_level = G_LOG_LEVEL_CRITICAL; break;
        case 'd': case 'D': log_level = G_LOG_LEVEL_DEBUG; break;
        }
    }
    g_log(G_LOG_DOMAIN, log_level, "[%lu %.6f] %s:%s: %s",
          (unsigned long) order, (double) timestamp / RECORDER_HZ,
          location, label, message);
}

SPICE_CONSTRUCTOR_FUNC(spice_log_init)
{
    spice_log_set_debug_level();
    spice_log_set_abort_level();
    if (glib_debug_level != INT_MAX) {
        /* If SPICE_DEBUG_LEVEL is set, we need a custom handler, which is
         * going to break use of g_log_set_default_handler() by apps
         */
        g_log_set_handler(G_LOG_DOMAIN,
                          G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                          spice_logger, NULL);
    }
    /* Threading is always enabled from 2.31.0 onwards */
    /* Our logging is potentially used from different threads.
     * Older glibs require that g_thread_init() is called when
     * doing that. */
#if !GLIB_CHECK_VERSION(2, 31, 0)
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    /* If SPICE_TRACES is set, use that and select default recorder output.
       Otherwise, use glib logging.
       In both cases, trace critical, error and warning messages */
    char *spice_traces = getenv("SPICE_TRACES");
    if (spice_traces) {
        recorder_trace_set(spice_traces);
    } else {
        recorder_configure_format(spice_recorder_format);

        char *debug_env = (char *)g_getenv("G_MESSAGES_DEBUG");
        if (debug_env == NULL) {
            g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, FALSE);
        } else {
            debug_env = g_strconcat(debug_env, " ", G_LOG_DOMAIN, NULL);
            g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, FALSE);
            g_free(debug_env);
        }
    }
    recorder_trace_set(".*_warning|.*_error|.*_critical");
    recorder_dump_on_common_signals(0, 0);
}

static void spice_logv(const char *log_domain,
                       GLogLevelFlags log_level,
                       const char *strloc,
                       const char *function,
                       const char *format,
                       va_list args)
{
    GString *log_msg;

    if ((log_level & G_LOG_LEVEL_MASK) > glib_debug_level) {
        return; // do not print anything
    }

    log_msg = g_string_new(NULL);
    if (strloc && function) {
        g_string_append_printf(log_msg, "%s:%s: ", strloc, function);
    }
    if (format) {
        g_string_append_vprintf(log_msg, format, args);
    }
    g_log(log_domain, log_level, "%s", log_msg->str);
    g_string_free(log_msg, TRUE);

    if ((abort_mask & log_level) != 0) {
        spice_backtrace();
        abort();
    }
}

void spice_log(GLogLevelFlags log_level,
               const char *strloc,
               const char *function,
               const char *format,
               ...)
{
    va_list args;

    va_start (args, format);
    spice_logv (G_LOG_DOMAIN, log_level, strloc, function, format, args);
    va_end (args);
}


RECORDER(spice_info,    128, "Default recorder for spice_info");
RECORDER(spice_debug,   128, "Default recorder for spice_debug");
RECORDER(spice_warning, 128, "Default recorder for spice_warning");
RECORDER(spice_error,   128, "Default recorder for spice_error");
RECORDER(spice_critical,128, "Default recorder for spice_critical");
