/* Linux smoke-test launcher for the Xodus descriptor mapping protocol. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main( int argc, char **argv )
{
    char buffer[8192], path[8192], mapping[2 * sizeof(path) + 64];
    ssize_t count;
    int input, fd;
    size_t i;

    if (argc != 4) return 2; /* wine, plaintext test PE, absolute placeholder path */
    if (strlen(argv[3]) > sizeof(path) - 8) return 2;
    if ((input = open(argv[2], O_RDONLY)) < 0) return 2;
    if ((fd = memfd_create("winegdk-test", 0)) < 0) return 2;
    while ((count = read(input, buffer, sizeof(buffer))) > 0)
        if (write(fd, buffer, count) != count) return 2;
    close(input);
    if (count < 0) return 2;
    snprintf(path, sizeof(path), "\\??\\Z:%s", argv[3]);
    for (i = 6; path[i]; i++) if (path[i] == '/') path[i] = '\\';
    /* Invalid entries must be skipped safely, including overflowing descriptors. */
    snprintf(mapping, sizeof(mapping), "invalid|999999999999999999999999999:%s|%d:%s", path, fd, path);
    if (setenv("WINE_DLL_FILE_MAP", mapping, 1)) return 2;
    execl(argv[1], argv[1], path, "literal value;$HOME", (char *)NULL);
    return 2;
}
