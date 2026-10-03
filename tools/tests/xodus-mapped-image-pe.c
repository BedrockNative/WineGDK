#include <string.h>

int main( int argc, char **argv )
{
    return argc == 2 && !strcmp(argv[1], "literal value;$HOME") ? 73 : 74;
}
