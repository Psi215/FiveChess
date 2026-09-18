// 此头文件储存禁手、胜负相关
#ifndef RULES_H
#define RULES_H
#include "board.h"
#include <stdlib.h>

// 禁手、胜负码
#define JUDGE_NONE 0              // 无胜负/禁手
#define JUDGE_FORBID_OVERLINE 1   // 长连禁手
#define JUDGE_FORBID_FOURFOUR 2   // 四四禁手
#define JUDGE_FORBID_THREETHREE 3 // 三三禁手
#define JUDGE_WIN_BLACK 4         // 黑方五连取胜
#define JUDGE_WIN_WHITE 5         // 白方五连取胜
#define JUDGE_PEACE 6             // 和棋

int ModeSelect(void); // 模式选择
int PlayerInput(void);
int AiInput(int XValue, int YValue);
int Judge(int x, int y);                                                    // 返回上述之一，根据 Side 判断（全局 Point/Side）
int JudgeOnBoard(int Board[LEN + 1][LEN + 1], int x, int y, int SideColor); // 基于指定棋盘与执子方判定
int JudgeFurther(void);

#endif // RULES_H
