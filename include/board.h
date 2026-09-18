//储存棋盘相关
#ifndef BOARD_H
#define BOARD_H
#include "common.h"

extern int Point[LEN+1][LEN+1];
extern int StepCount;
extern int Side; // 0 黑 1 白
extern int X, Y;//落子坐标
int Draw(void);

void BoardReset(void);
int PutChess(int x, int y, int color);

#endif // BOARD_H
