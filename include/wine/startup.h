/* Optional startup milestones from the Unix side of Wine.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __WINE_STARTUP_H
#define __WINE_STARTUP_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "wine/json.h"

/* Each event uses a separate, nonblocking connection. Startup must not wait
 * for a launcher to accept or consume telemetry. No Wine services are needed. */
static inline void wine_startup_event(const char *event, const char *detail, unsigned int code)
{
    const char *destination = getenv("WINE_STARTUP_LOG");
    int saved_errno = errno, fd = -1;
    char *escaped = NULL, *prefix = NULL, *line = NULL;
    struct timespec realtime, monotonic;
    size_t size, length;
    ssize_t written = -1;
    struct stat st;

    if (!destination || !*destination) return;
    if (!(escaped = wine_json_escape(detail)) ||
        !(prefix = wine_json_escape(getenv("WINEPREFIX")))) goto done;
    size = strlen(escaped) + strlen(prefix) + strlen(event) + 320;
    if (!(line = malloc(size))) goto done;
    if (clock_gettime(CLOCK_REALTIME, &realtime) || clock_gettime(CLOCK_MONOTONIC, &monotonic)) goto done;
    length = snprintf(line, size,
                      "{\"version\":1,\"source\":\"startup\",\"event\":\"%s\","
                      "\"time_unix_ms\":%llu,\"monotonic_ms\":%llu,\"pid_unix\":%lu,"
                      "\"prefix\":\"%s\",\"detail\":\"%s\",\"code\":%u}\n",
                      event, (unsigned long long)realtime.tv_sec * 1000 + realtime.tv_nsec / 1000000,
                      (unsigned long long)monotonic.tv_sec * 1000 + monotonic.tv_nsec / 1000000,
                      (unsigned long)getpid(), prefix, escaped, code);
    if (length >= size) goto done;
    if (!strncmp(destination, "unix:", 5))
    {
        struct sockaddr_un address = { .sun_family = AF_UNIX };
        int flags = 0;

        if (strlen(destination + 5) >= sizeof(address.sun_path)) goto fallback;
        strcpy(address.sun_path, destination + 5);
        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) goto fallback;
        if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 || fcntl(fd, F_SETFL, O_NONBLOCK) == -1) goto fallback;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#elif defined(SO_NOSIGPIPE)
        {
            int enabled = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled))) goto fallback;
        }
#else
        goto fallback;
#endif
        if (connect(fd, (struct sockaddr *)&address, sizeof(address))) goto fallback;
        written = send(fd, line, length, flags);
    }
    else
    {
        fd = open(destination, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, 0600);
        if (fd == -1 || fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 || fstat(fd, &st) || !S_ISREG(st.st_mode))
            goto fallback;
        written = write(fd, line, length);
    }
fallback:
    /* A disconnected receiver must never take the game down or hold up boot.
     * A partial socket record is incomplete; the full event goes to stderr. */
    if (written != length) write(STDERR_FILENO, line, length);
done:
    if (fd != -1) close(fd);
    free(line);
    free(prefix);
    free(escaped);
    errno = saved_errno;
}
#endif
