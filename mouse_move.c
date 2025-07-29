#include<windows.h>

int main(){
    INPUT move = {0};      //初始化
    move.type = 0;        //选择结构体中联合体类型
    move.mi.dx = ((2074+80.5) * 65535) /2240;    //使用结构体的类
    move.mi.dy = ((867+82.5 )* 65535) /1400;
    move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;       //  这个是负责移动绝对坐标
    SendInput(1, &move, sizeof(INPUT));      //1- 一个动作 ，@move-动作类型 ， sizeof(INPUT)执行方式

    return 0;

}

