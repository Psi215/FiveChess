#include "include/game.h"
#include <stdio.h> // 确保引入 stdio

int main(void)
{
    int choice = 1;
    while (choice)
    {
        system("chcp 65001 > nul"); // 强制使用UTF-8，防止乱码
        system("color F0");         // 设置背景和字体色 (白底黑字)
        system("cls");              // 清屏

        printf("\n\n");
        printf("\t\t   ● ○ ● ○ ● ○ ● ○ ● ○ ● ○ ●\n");
        printf("\t\t   ○                       ○\n");
        printf("\t\t   ●       五  子  棋      ●\n");
        printf("\t\t   ○                       ○\n");
        printf("\t\t   ● ○ ● ○ ● ○ ● ○ ● ○ ● ○ ●\n");
        printf("\n");
        printf("\t\t ╔═══════════════════════════╗\n");
        printf("\t\t ║                           ║\n");
        printf("\t\t ║      1. 人 人 对 战       ║\n");
        printf("\t\t ║                           ║\n");
        printf("\t\t ║      2. 人 机 对 战       ║\n");
        printf("\t\t ║                           ║\n");
        printf("\t\t ║      3. 机 机 对 战       ║\n");
        printf("\t\t ║                           ║\n");
        printf("\t\t ║      4. 退 出 游 戏       ║\n");
        printf("\t\t ║                           ║\n");
        printf("\t\t ╚═══════════════════════════╝\n");
        printf("\n\t\t      请输入选项 [1-4]: ");

        switch (ModeSelect()) // 模式选择
        {
        case 1:
            while (GamePvP())
            {
            };
            break;
        case 2:
            while (GamePvE())
            {
            };
            break;
        case 3:
            while (GameEvE())
            {
            };
            break;
        default:
            choice = 0; // 退出游戏
        }
    }
    return 0;
}
