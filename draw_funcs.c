#include <ncurses.h>
#include <wchar.h>
#include <locale.h>

#include "inc/draw_funcs.h"

/* print message in specified window */
void print_msg (WINDOW * win, char message[], bool sender) {
        switch (sender) {
            case true:
                mvwprintw(win, 39, 2, "%lc%lc%lc %s\n", (wint_t)9472, (wint_t)9472, (wint_t)10148, message);
                break;
            
            case false:
                mvwprintw(win, 39, 2, "%lc %s\n", (wint_t)10148, message);
                break;
        }
        
        wscrl(win, 1);
        box(win, 0, 0);
        wrefresh(win);
}

/* clear and redraw window */
void win_reset (WINDOW * win) {
    werase(win);
    box(win, 0, 0);
    wrefresh(win);
}
