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
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "log.h"
#include "backtrace.h"

static int glib_debug_level = INT_MAX;
static int abort_level = -1;

#ifndef SPICE_ABORT_LEVEL_DEFAULT
#ifdef SPICE_DISABLE_ABORT
#define SPICE_ABORT_LEVEL_DEFAULT -1
#else
#define SPICE_ABORT_LEVEL_DEFAULT SPICE_LOG_LEVEL_CRITICAL
#endif
#endif

static GLogLevelFlags spice_log_level_to_glib(SpiceLogLevel level)
{
    static const GLogLevelFlags glib_levels[] = {
        [ SPICE_LOG_LEVEL_ERROR ] = G_LOG_LEVEL_ERROR,
        [ SPICE_LOG_LEVEL_CRITICAL ] = G_LOG_LEVEL_CRITICAL,
        [ SPICE_LOG_LEVEL_WARNING ] = G_LOG_LEVEL_WARNING,
        [ SPICE_LOG_LEVEL_INFO ] = G_LOG_LEVEL_INFO,
        [ SPICE_LOG_LEVEL_DEBUG ] = G_LOG_LEVEL_DEBUG,
        [ SPICE_LOG_LEVEL_TRACE ] = G_LOG_LEVEL_DEBUG,
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
            const char *debug_log_domain =
#define SPICE_TRACE(name, value, info)  SPICE_LOG_DOMAIN "[" #name "] "
#include "spice-traces.def"
                SPICE_LOG_DOMAIN;
            if (debug_env == NULL) {
                g_setenv("G_MESSAGES_DEBUG", debug_log_domain, FALSE);
            } else {
                debug_env = g_strconcat(debug_env, " ", debug_log_domain, NULL);
                g_setenv("G_MESSAGES_DEBUG", debug_env, FALSE);
                g_free(debug_env);
            }
        }
    }
}

static void spice_log_set_abort_level(void)
{
    if (abort_level == -1) {
        const char *abort_str = g_getenv("SPICE_ABORT_LEVEL");
        if (abort_str != NULL) {
            GLogLevelFlags glib_abort_level;

            /* FIXME: To be removed after enough deprecation time */
            g_warning("Setting SPICE_ABORT_LEVEL is deprecated, use G_DEBUG instead");
            abort_level = atoi(abort_str);
            glib_abort_level = spice_log_level_to_glib(abort_level);
            unsigned int fatal_mask = G_LOG_FATAL_MASK;
            while (glib_abort_level >= G_LOG_LEVEL_ERROR) {
                fatal_mask |= glib_abort_level;
                glib_abort_level >>= 1;
            }
            g_log_set_fatal_mask(SPICE_LOG_DOMAIN, fatal_mask);
#define SPICE_TRACE(name, value, info)                                 \
            g_log_set_fatal_mask(SPICE_LOG_DOMAIN "[" #name "]", fatal_mask);
#include "spice-traces.def"
        } else {
            abort_level = SPICE_ABORT_LEVEL_DEFAULT;
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

SPICE_CONSTRUCTOR_FUNC(spice_log_init)
{
    char *trace_spec = getenv("SPICE_TRACES");
    if (trace_spec)
        spice_set_trace(trace_spec);

    spice_log_set_debug_level();
    spice_log_set_abort_level();
    GLogLevelFlags spice_levels = G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION;
    g_log_set_handler(SPICE_LOG_DOMAIN, spice_levels, spice_logger, NULL);
#define SPICE_TRACE(name, value, info)                  \
    g_log_set_handler(SPICE_LOG_DOMAIN "[" #name "]",   \
                      spice_levels, spice_logger, NULL);
#include "spice-traces.def"

    /* Threading is always enabled from 2.31.0 onwards */
    /* Our logging is potentially used from different threads.
     * Older glibs require that g_thread_init() is called when
     * doing that. */
#if !GLIB_CHECK_VERSION(2, 31, 0)
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif
}

static void spice_logv(const char *log_domain,
                       SpiceLogLevel log_level,
                       const char *strloc,
                       const char *function,
                       const char *format,
                       va_list args)
{
    GString *log_msg;
    GLogLevelFlags glib_level = spice_log_level_to_glib(log_level);

    if ((glib_level & G_LOG_LEVEL_MASK) > glib_debug_level) {
        return; // do not print anything
    }

    log_msg = g_string_new(NULL);
    if (strloc && function) {
        g_string_append_printf(log_msg, "%s:%s: ", strloc, function);
    }
    if (format) {
        g_string_append_vprintf(log_msg, format, args);
    }
    g_log(log_domain, glib_level, "%s", log_msg->str);
    g_string_free(log_msg, TRUE);

    if (abort_level != -1 && abort_level >= (int) log_level) {
        spice_backtrace();
        abort();
    }
}

void spice_log(const char *log_domain,
               SpiceLogLevel log_level,
               const char *strloc,
               const char *function,
               const char *format,
               ...)
{
    va_list args;

    g_return_if_fail(spice_log_level_to_glib(log_level) != 0);

    if (log_level == SPICE_LOG_LEVEL_TRACE) {
        if (TRACE(trace_stderr)) {
            va_start (args, format);
            IFTRACE(trace_timestamp) {
                static guint64 start = 0;
                guint64 now = g_get_monotonic_time() - start;
                if (start == 0)
                    start = now;
                fprintf(stderr, "%lu.%06lu:", now / 1000000, now % 1000000);
            }
            IFTRACE(trace_name)      { fprintf(stderr, "%s", log_domain + (sizeof(SPICE_LOG_DOMAIN))-1); }
            IFTRACE(trace_location)  { fprintf(stderr, "%s:", strloc); }
            IFTRACE(trace_function)  { fprintf(stderr, "(%s)", function); }
            vfprintf(stderr, format, args);
            fputs("\n", stderr);
            va_end(args);
        }
        if (!TRACE(trace_stdlog))
            return;
    }

    if ((log_level & G_LOG_LEVEL_MASK) > glib_debug_level) {
        return; // do not print anything
    }

    va_start (args, format);
    spice_logv (log_domain, log_level, strloc, function, format, args);
    va_end (args);
}


int spice_set_trace(const char *trace_spec)
{
    const enum number_of_traces {
#define SPICE_TRACE(name, value, info)   TRACE_##name,
#include "spice-traces.def"
        TRACE_COUNT
    } number_of_traces = TRACE_COUNT;
    int count = 0;

    spice_trace(trace_set, "Trace specification '%s'", trace_spec);
    gchar **traces = g_strsplit(trace_spec, ":", number_of_traces);
    for (gchar **trace = traces; *trace; trace++) {
        int set_value = 1;
        char *equal_sign = strstr(*trace, "=");
        gchar *trace_name = *trace;
        int pattern = *trace_name == '*';
        unsigned settings_applied = 0;
        if (pattern)
            trace_name++;
        if (equal_sign) {
            char *end;
            set_value = strtol(equal_sign + 1, &end, 10);
            if (end == equal_sign + 1 || *end != 0) {
                g_strfreev(traces);
                return -1;
            }
            *equal_sign = '\0';
        }
        spice_trace(trace_set, "Trace name '%s'%s, value=%d",
                    trace_name, pattern ? " (pattern)" : "", set_value);

#define SPICE_TRACE(name, init, info)                           \
        if ((pattern && strstr(#name, trace_name) ||            \
             strcmp(#name, trace_name) == 0)) {                 \
            spice_traces.name = set_value;                      \
            spice_trace(trace_set,                              \
                        "Set %s=%d", #name, set_value);         \
            settings_applied++;                                 \
        }
#define SPICE_TWEAK(name, init, info)                           \
        if (!pattern && strcmp(#name, trace_name) == 0)) {      \
            spice_traces.name = set_value;                      \
            spice_trace(trace_set,                              \
                        "Set %s=%d", #name, set_value);         \
            settings_applied++;                                 \
        }
#include "spice-traces.def"

        spice_trace(trace_set, "Set %u trace variables", settings_applied);
        if (settings_applied == 0)
        {
            g_strfreev(traces);
            return -2;
        }
        count++;
    }
    g_strfreev(traces);
    return count;
}

struct spice_traces spice_traces =
{
/* Initialize tweaks first, to match structure declaration in log.h */
#define SPICE_TWEAK(name, init, info)   init,
#define SPICE_TRACE(name, init, info)
#include "spice-traces.def"

#define SPICE_TWEAK(name, init, info)
#define SPICE_TRACE(name, init, info)   init,
#include "spice-traces.def"
};
