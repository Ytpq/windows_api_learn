```
typedef struct tagINPUT {
    DWORD type;
    union {
        MOUSEINPUT    mi;
        KEYBDINPUT    ki;
        HARDWAREINPUT hi;
    };
} INPUT;
```
# 在使用结构体时，要先初始化 ``INPUT move = {0};`` 
1. tagINPUT 中的tag是结构体的定义，实际结构体是INPUT
2. 结构体要先初始化，防止没有定义报错


--------------------------------------------------------------------------

```
#define INPUT_MOUSE     0   // 表示这是个鼠标事件
#define INPUT_KEYBOARD  1   // 表示是键盘事件
#define INPUT_HARDWARE  2   // 表示是硬件事件（很少用）
```
# 选择结构体中联合体的类型 ``move.type = 0;``

--------------------------------------------------------

```
typedef struct tagMOUSEINPUT {
  LONG      dx;
  LONG      dy;
  DWORD     mouseData;
  DWORD     dwFlags;
  DWORD     time;
  ULONG_PTR dwExtraInfo;
} MOUSEINPUT, *PMOUSEINPUT, *LPMOUSEINPUT;
```
# 在这个现在事件类型 ``move.mi.dx = ((2074+80.5) * 65535) /2240;``

------------------------------------------------------------------------

# 这个是运动指令 ``SendInput(1, &move, sizeof(INPUT));``
