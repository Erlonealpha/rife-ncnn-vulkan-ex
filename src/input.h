#define KEY_Q 1
#define KEY_P 2
#define KEY_X 3

#ifdef _WIN32
#include <windows.h>

void console_init(HANDLE hStdin)
{
    DWORD mode;
    if (!GetConsoleMode(hStdin, &mode)) return;
    if (!SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT))) return;
}

int key_press(HANDLE hStdin)
{
    INPUT_RECORD ir[16];
    DWORD eventsRead;
    DWORD i;
    if (!ReadConsoleInput(
        hStdin,
        ir,
        16,
        &eventsRead
    )) return 0;
    for (i = 0; i < eventsRead; i++)
    {
        if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown)
        {
            if (ir[i].Event.KeyEvent.wVirtualKeyCode == 0x58) // X key
            {
                return KEY_X;
            }
            else if (ir[i].Event.KeyEvent.wVirtualKeyCode == 0x50) // P key
            {
                return KEY_P;
            }
            else if (ir[i].Event.KeyEvent.wVirtualKeyCode == 0x51) // Q key
            {
                return KEY_Q;
            }
            return 0x00;
        }
    }
    return 0;
}

#else
#include <termios.h>
#include <unistd.h>

void console_init()
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

int key_press()
{
    int ch;

    ch = getchar();

    if (ch != EOF)
    {
        if (ch == 'x' || ch == 'X')
            return KEY_X;
        else if (ch == 'p' || ch == 'P')
            return KEY_P;
        else if (ch == 'q' || ch == 'Q')
            return KEY_Q;
        else
            return 0x00;
    }

    return 0;
}
#endif