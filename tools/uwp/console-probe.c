/* Console frontend and VT output regression probe. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
static FILE *report;
static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(report,"FAIL %u: %s error=%lu\n",__LINE__,#x,GetLastError()); ++failures; } } while (0)
static void write_text(HANDLE output,const WCHAR *text)
{
    DWORD count;
    CHECK(WriteConsoleW(output,text,wcslen(text),&count,NULL));
    CHECK(count==wcslen(text));
}
static void test_controls(HANDLE output)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    DWORD mode, count;
    WCHAR text[32];
    WORD attr;
    COORD pos = {0, 5};
    CHECK(GetConsoleMode(output, &mode));
    CHECK(SetConsoleCursorPosition(output, pos));
    write_text(output, L"abc\rX\nA\tB\bC\r\n");
    CHECK(ReadConsoleOutputCharacterW(output, text, 3, pos, &count));
    CHECK(!memcmp(text, L"Xbc", 3 * sizeof(WCHAR)));
    pos.Y++;
    CHECK(ReadConsoleOutputCharacterW(output, text, 9, pos, &count));
    CHECK(!memcmp(text, L"A       C", 9 * sizeof(WCHAR)));
    CHECK(GetConsoleScreenBufferInfo(output, &info));
    CHECK(info.dwCursorPosition.X == 0 && info.dwCursorPosition.Y == 7);

    /* A newline inside a split CSI still executes, without discarding the CSI. */
    write_text(output, L"\x1b[31\r\n");
    write_text(output, L"mR\x1b[0m");
    pos.X = 0; pos.Y = 8;
    CHECK(ReadConsoleOutputCharacterW(output, text, 1, pos, &count));
    CHECK(text[0] == 'R');
    CHECK(ReadConsoleOutputAttribute(output, &attr, 1, pos, &count));
    CHECK((attr & 15) == FOREGROUND_RED);

    /* Disabling VT must discard an unfinished sequence. */
    write_text(output, L"\x1b[");
    CHECK(SetConsoleMode(output, mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    CHECK(SetConsoleMode(output, mode));
    write_text(output, L"31m");
    pos.X = 1;
    CHECK(ReadConsoleOutputCharacterW(output, text, 3, pos, &count));
    CHECK(!memcmp(text, L"31m", 3 * sizeof(WCHAR)));

    /* Raw output must continue to store control characters literally. */
    pos.X = 0; pos.Y = 9;
    CHECK(SetConsoleCursorPosition(output, pos));
    CHECK(SetConsoleMode(output, ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    write_text(output, L"X\r\nY");
    CHECK(ReadConsoleOutputCharacterW(output, text, 4, pos, &count));
    CHECK(!memcmp(text, L"X\r\nY", 4 * sizeof(WCHAR)));
    CHECK(SetConsoleMode(output, mode));
}
int main(int argc,char **argv)
{
    HANDLE output,input;
    DWORD mode,n;
    WCHAR text[128]={0};
    CONSOLE_SCREEN_BUFFER_INFO info;
    WORD attrs[8];
    COORD home={0,0};
    BOOL native;
    if(argc!=4)return 2;
    if(!(report=fopen(argv[1],"w")))return 3;
    native=!strcmp(argv[2],"native");
    FreeConsole();
    CHECK(AllocConsole());
    /* Match a GUI application's CRT redirection before enabling ANSI output. */
    CHECK(freopen("CONOUT$", "w+", stdout) != NULL);
    CHECK(freopen("CONOUT$", "w+", stderr) != NULL);
    output=GetStdHandle(STD_OUTPUT_HANDLE);input=GetStdHandle(STD_INPUT_HANDLE);
    CHECK(GetConsoleMode(output,&mode));
    CHECK(GetConsoleScreenBufferInfo(output,&info));
    CHECK(FillConsoleOutputCharacterW(output,L' ',info.dwSize.X*info.dwSize.Y,home,&n));
    CHECK(SetConsoleCursorPosition(output,home));
    CHECK(SetConsoleMode(output,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    CHECK(SetConsoleTitleW(L"WineGDK terminal test"));
    write_text(output,L"\x1b[1;");
    write_text(output,L"31mRED\x1b[0m plain\r\n");
    write_text(output,L"\x1b[38;5;245m[ proxy] [AmethystProxy] Using 'AmethystProxy@1.2.0'\x1b[0m\r\n");
    CHECK(ReadConsoleOutputCharacterW(output,text,9,home,&n));
    CHECK(n==9 && !memcmp(text,L"RED plain",9*sizeof(WCHAR)));
    CHECK(ReadConsoleOutputAttribute(output,attrs,8,home,&n));
    CHECK((attrs[0]&0x0f)==(FOREGROUND_RED|FOREGROUND_INTENSITY));
    CHECK((attrs[4]&0x0f)==7);
    CHECK(GetConsoleScreenBufferInfo(output,&info));
    CHECK(info.dwCursorPosition.X==0 && info.dwCursorPosition.Y==2);
    CHECK((!IsWindowVisible(GetConsoleWindow()))==native);
    write_text(output,L"\x1b[2;3HX\x1b[2DX\x1b[0K");
    home.X=0;home.Y=1;
    CHECK(ReadConsoleOutputCharacterW(output,text,4,home,&n));
    CHECK(!memcmp(text,L"[X  ",4*sizeof(WCHAR)));
    test_controls(output);
    write_text(output,L"\x1b[3;1H");
    if(!strcmp(argv[3],"resize"))
    {
        CHECK(info.dwSize.X==120 && info.dwSize.Y==30);
        write_text(output,L"WineGDK terminal resize ready\r\n");
        Sleep(900);
        CHECK(GetConsoleScreenBufferInfo(output,&info));
        CHECK(info.dwSize.X==92 && info.dwSize.Y==28);
    }
    write_text(output,L"WineGDK terminal input ready\r\n");
    if(!strcmp(argv[3],"input") || !strcmp(argv[3],"resize"))
    {
        CHECK(ReadConsoleW(input,text,127,&n,NULL));
        CHECK(n==16 && !memcmp(text,L"terminal-input\r\n",16*sizeof(WCHAR)));
    }
    else Sleep(500);
    fprintf(report,"console: %u failures\n",failures);
    FreeConsole();fclose(report);return !!failures;
}
