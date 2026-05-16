# zmk_config_zitaotech_sofle

ZMK firmware configuration for ZitaoTech Sofle split keyboard (nRF52840).

## 硬件特性 (Hardware)

| 组件     | 左手 (Central)                    | 右手 (Peripheral)     |
| -------- | --------------------------------- | --------------------- |
| MCU      | nRF52840                          | nRF52840              |
| 编码器   | EC11 (左旋钮)                     | EC11 (右旋钮)         |
| 指点设备 | BB Trackball / BB Trackpad (A320) | Trackpoint (I2C 0x15) |
| 显示屏   | LPM View (Memory LCD)             | LPM View (Memory LCD) |
| 背光     | PWM 键盘背光 + 轨迹球背光         | PWM 键盘背光          |
| 灯效     | WS2812 ×1                         | WS2812 ×1             |
| 电池     | 电压分压监测                      | 电压分压监测          |

## 键盘布局 (Keymap)

<img src="keymap-drawer/zitaotech_sofle.svg" >

## 层设计 (Layers)

### Layer 0 — Nexys (默认层)

标准 QWERTY 布局，设计特点：

- **ESC 替代 CapsLock**：ESC 放在传统 CapsLock 位置（Vim/终端友好）
- **数字行 0–9 从左到右排列**：N0 在左手食指位，编程高频数字触手可及，与 Symbol 层 hex 区 0 起点对齐
- **对称拇指区**：左手 Win / Alt / Ctrl / Layer1 / Space；右手 Enter / Layer2 / RCtrl / RAlt / RWin — 两侧均有完整修饰键
- **左旋钮按下 = 静音**；**右旋钮按下 = Insert**（IBM CUA 复制黏贴的搭档键）
- **轨迹球/指点杆按下 = 鼠标左键**
- **额外按键**：左手额外键为 PG_DN / PG_UP（快速翻页），右手额外键为鼠标左键 / 右键
- **左旋钮**：上下方向键；**右旋钮**：左右方向键

### Layer 1 — Neural (功能层)

桌面管理 + 终端通用快捷键 + 外设控制。左半区集中修饰键组合，右半区为导航集群。

#### 标签页与窗口 (QWERTY 行)

```
W: 桌面左移     E: 反向窗口切换   R: 上一标签页   T: 关闭窗口/标签
S: 桌面右移     D: 窗口切换器     F: 下一标签页
```

| 键位 | 绑定 | 功能 |
|---|---|---|
| W | `LC(LG(LEFT))` | niri 工作区左移 / Windows 虚拟桌面左 |
| E | `LA(LS(TAB))` | 反向窗口切换器 |
| R | `LC(LS(TAB))` | 上一标签页（终端/浏览器通用） |
| T | `LC(W)` | 关闭标签页/窗口 |
| S | `LC(LG(RIGHT))` | niri 工作区右移 / Windows 虚拟桌面右 |
| D | `LA(TAB)` | 窗口切换器 |
| F | `LC(TAB)` | 下一标签页 |

#### 通用复制黏贴 (底行 X/C/V/B)

使用 **IBM CUA 快捷键**，在终端和 GUI 中均生效（Ctrl+C 在终端中是 SIGINT，无法复制）：

| 键位 | 绑定 | 功能 |
|---|---|---|
| X | `LS(DELETE)` | 剪切 (Shift+Delete) |
| C | `LC(INSERT)` | 复制 (Ctrl+Insert) |
| V | `LS(INSERT)` | 黏贴 (Shift+Insert) |
| B | `LC(W)` | 关闭窗口/标签页 |

#### WezTerm 分屏 (左额外按键)

| 键位 | 绑定 | 功能 |
|---|---|---|
| 左额外 1 | `LC(LS(LA(DQT)))` | 垂直分屏 |
| 左额外 2 | `LC(LS(LA(PERCENT)))` | 水平分屏 |

#### 右手导航集群

```
HOME    UP      END     PGUP    DEL
LEFT    DOWN    RIGHT   PGDN
```

完整方向键 + 六键导航，覆盖文本编辑和页面浏览。

#### 外设控制

| 键位 | 绑定 | 功能 |
|---|---|---|
| 左上角 | `RGB_TOG` | RGB 灯开关 |
| 左下角 | `OUT_TOG` | USB/BLE 输出切换 |
| 旋钮按下 | `BT_CLR` / `C_MUTE` | 蓝牙清空 / 静音 |
| 右手方向键下 | `BL_DEC` / `BL_INC` | 键盘背光增减 |

- **F1–F12**：完整功能键行排列在顶行
- 指点设备在**此层自动切换为滚轮模式**
- **左旋钮**：音量 +/-；**右旋钮**：Backspace / Delete

### Layer 2 — Symbol (符号层)

双手分工明确：**左手输入完整数值字面量，右手输入编程符号**。

#### 左手 — 数值字面量构造区

```
B   0   1   2   3   U
X   4   5   6   7   -
L   8   9   A   B   .
F   C   D   E   F   ,
```

核心是 4×4 标准十六进制键位（0–9 + A–F），左右各附加一列语义修饰符：

| 左侧（前缀/类型） | 右侧（后缀/分隔） |
|---|---|
| `B` — 0b 二进制前缀 | `U` — unsigned 后缀 |
| `X` — 0x 十六进制前缀 | `-` — 负号/范围 |
| `L` — long 后缀 | `.` — 小数点/IP 地址 |
| `F` — float 后缀 | `,` — 列表分隔 |

常见数值字面量一行流水完成：

```
0xFF    →  X → F → F
0b1010  →  B → 1 → 0 → 1 → 0
1.5f    →  1 → . → 5 → F
-42U    →  - → 4 → 2 → U
192.168 →  1 → 9 → 2 → . → 1 → 6 → 8
```

#### 右手 — 成对符号分区

```
%   @   [   ]   #   ~
-   +   {   }   ^   $
_   =   (   )   :   &
/   *   <   >   !   |
```

符号的排列遵循两条核心规则：

**1. 括号按「开/关」垂直分列**

```
[   ]        ← 方括号 (square)
{   }        ← 花括号 (curly)
(   )        ← 圆括号 (paren)  ← 常驻手势排，无需移动
```

- **中指列 = 所有开括号** `[ { (`
- **无名指列 = 所有闭括号** `] } )`
- 圆括号 `( )` 放在手势排（home row），使用频率最高且无需任何手指位移
- 方括号 `[ ]` 放顶部、花括号 `{ }` 居中，按频率分层

**2. 关联符号成组**

- `+ - * / =` 数学运算符聚集在右手食指/中指区域
- `< >` 沿袭开/闭列规则，中指 `<` 无名指 `>`
- `: &` 同行相邻（C++ 引用 `:&` 常见组合）
- `! |` 同行相邻（逻辑/位运算常用组合）
- `^ $` 同行相邻（正则/字符串结尾常用）

**3. 保留 QWERTY 手指记忆的键位复用**

| 默认层按键 | 默认层 + Shift | Symbol 层 | 说明 |
|---|---|---|---|
| `;` | `:` | `:` | 同键位，零学习成本 |
| `,` | `<` | `<` | 同键位，零学习成本 |
| `.` | `>` | `>` | 同键位，零学习成本 |

#### 与默认层对比 — 设计意图

默认 QWERTY 层的符号是历史遗留布局：数字行符号横跨双手且需 Shift，括号 `( ) [ ] { }` 分散在数字行。Symbol 层针对性地做了优化：

- **从「双手 Shift 组合」到「单手直接输入」**：`( )` 直接放在右手手势排，无需 Shift
- **左右手并行流水线**：左手只管数值（hex + 修饰符），右手只管符号，互不阻塞——输入 `(0x1A + 5)` 时两手各司其职
- **拇指区全 `&none` 防误触**：进入靠按住默认层拇指 `mo 2`，松开即回，过程中不会跳到其他层
- **指点设备按键静默**：轨迹球和指点杆按压在此层设为 `&none`，打字不误触鼠标点击，移动/滚轮照常

- **左旋钮**：蓝牙配置切换；**右旋钮**：Backspace / Delete

## 指点设备行为 (Pointing Device)

- **Layer 0 / Layer 2**：正常鼠标移动
- **Layer 1 (Neural)**：自动切换为滚轮模式，无需按住任何按键
- 轨迹球 (左)：GPIO 脉冲输入，4 方向 Hall 传感器，带指数加速曲线
- 指点杆 (右)：I2C 地址 0x15，速度随 LED 亮度动态调节，滚动速度按位移分级缩放

> 驱动参数详见 `config/zitaotech_sofle_keymap_reference.conf`

## 编译 (Build)

```bash
# 左手 (轨迹球版)
west build -p -b zitaotech_sofle_left -- -DSHIELD="lpm_view;left_bbtrackball"

# 左手 (触摸板版)
west build -p -b zitaotech_sofle_left -- -DSHIELD="lpm_view;left_bbtrackpad"

# 右手
west build -p -b zitaotech_sofle_right -- -DSHIELD="lpm_view;right_trackpoint"

# 带 settings reset（清除蓝牙配对信息）
west build -p -b zitaotech_sofle_right -- -DSHIELD="lpm_view;right_trackpoint;settings_reset"
```

## 键位矩阵参考

矩阵索引及详细硬件映射见 `config/zitaotech_sofle_keymap_reference.conf`。
