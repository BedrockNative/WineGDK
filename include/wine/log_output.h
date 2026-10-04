/* Optional log sinks for Wine programs. Requires kernel32 and ws2_32. */
#ifndef __WINE_LOG_OUTPUT_H
#define __WINE_LOG_OUTPUT_H

struct wine_log_output
{
    BOOL enabled;
    HANDLE file, fallback;
    SOCKET socket;
    const WCHAR *variable;
};

BOOL wine_log_output_open(struct wine_log_output *output, const WCHAR *variable);
void wine_log_output_write(struct wine_log_output *output, const char *buffer, DWORD length);
void wine_log_output_close(struct wine_log_output *output);

void wine_log_output_event(struct wine_log_output *output, const char *source,
                           const char *event, const char *stage, DWORD code);

#endif
