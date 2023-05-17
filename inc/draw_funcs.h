#ifndef DRAW_FUNCS_H
#define DRAW_FUNCS_H

#define ctrl(x) ((x) & 0x1f)

void win_reset (WINDOW * win);
void print_msg (WINDOW * win, char message[], bool sender);

#endif
