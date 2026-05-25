/*******************************************************************
   ABSTRACT: This file is for efort sdk constants define

   AUTHOR: jidongbao
   E-MAIL: jidongbao@efort.com.cn

   HISTORY: 2022-10-13, created

   Copyright (c) Efort Intelligent Equipment Ltd. Corp.
   2022 - 2030 All rights reserved

*******************************************************************/

#pragma once

#define ERROR_OK                        0       // 操作成功
#define ERROR_CONNECT_FAILED            1       // 与机器人连接失败
#define ERROR_NO_CONNECTION             2       // 尚未与机器人连接
#define ERROR_CONNECTION_BROKEN         3       // 与机器人连接中断
#define ERROR_ACCESS_REJECTED           4       // 访问拒绝，机器人设置为不允许 API 访问
#define ERROR_CUR_MODE_NOT_SUPPORT      5       // 当前模式不支持该操作
#define ERROR_SERVO_ON_FAILED           6       // 上电失败
#define ERROR_POWER_OFF_FAILED          7       // 下电失败
#define ERROR_SET_MODE_FAILED           8       // 设置控制模式失败
#define ERROR_SET_SPEED_RATIO_FAILED    9       // 设置速度倍率失败，请检查设置的倍率值是否有效
#define ERROR_SWITCH_CHANNEL_FAILED     10      // 切换通道失败，请检查设置的通道号是否有效
#define ERROR_START_PROGRAM_FAILED      11      // 启动程序失败，只有在没有告警、已成功加载程序且系统状态处于暂停或停止的情况下才可以启动程序
#define ERROR_RESET_PROGRAM_FAILED      12      // 复位程序失败，只有系统状态处于暂停或停止时才可以复位程序
#define ERROR_LOAD_PROGRAM_FAILED       13      // 加载程序失败，请获取告警列表查看具体失败信息
#define ERROR_PARA_INVALID              14      // 输入参数范围有误
#define ERROR_STILL_RUNNING             15      // 系统仍处于运行中，无法执行运动指令
#define ERROR_SET_PARA_FAILED           16      // 设置变量失败
#define ERROR_GET_PARA_FAILED           17      // 查询变量失败
#define ERROR_MOVE_FAILED               18      // 控制机器人移动失败
#define ERROR_ENABLE_API_CONTROL_FAILED 19      // 设置外部 API 控制失败
#define ERROR_THIRD_PARTY               10000   // 第三方库抛出的异常码起始值
#define ERROR_THIRD_OPERATION_FAILED    10001   // 操作失败
#define ERROR_THIRD_GET_CLIENT_FAILED   10002   // 获取客户端失败
#define ERROR_THIRD_GET_MON_FAILED      10003   // 创建monitor失败
#define ERROR_THIRD_START_MON_FAILED    10004   // 启动monitor失败
#define ERROR_THIRD_FLASH_FAILED        10005   // 获取文件操作接口失败
#define ERROR_THIRD_SAVE_FILE_FAILED    10006   // 从控制器下载文件失败
#define ERROR_THIRD_LOAD_FILE_FAILED    10007   // 往控制器上传XPL文件失败
#define ERROR_THIRD_FILE_EXIST          10008   // XPL文件存在
#define ERROR_THIRD_FILE_NOTEXIST       10009   // XPL文件不存在
#define ERROR_THIRD_FILE_ISEXIST_FAILED 10010   // XPL文件查询失败
#define ERROR_THIRD_GET_ROBOTTYPE_FAILED 10011   // 获取机型名失败
#define ERROR_THIRD_GET_MOVEDATA_FAILED 10012   // 获取运动数据失败
#define ERROR_THIRD_NOT_ATTACHED         10013   // 环境未获取
#define ERROR_THIRD_SET_WRITEMODE_FAILED 10014   // 写权限设置失败
#define ERROR_THIRD_GET_MONITORDATA_FAILED  10015   // 获取monitor中数据失败
#define ERROR_THIRD_GET_STSINFO_FAILED      10016   // 获取状态信息失败
#define ERROR_THIRD_CREATE_FOLDER_FAILED    10017   // 创建文件夹失败
#define ERROR_THIRD_DELETE_FOLDER_FAILED    10018   // 删除文件夹失败
#define ERROR_THIRD_PATH_INVALID            10019   // 文件夹路径无效
#define ERROR_THIRD_DELETE_SYSTEM_FOLDER    10020   // 禁止删除系统文件夹
#define ERROR_DEVICE_EXIT                   10021   // 设备已存在
#define ERROR_DEVICE_NOT_EXIST              10022   // 设备不存在
#define ERROR_THIRD_SAVE_TO_FILE_FAILED     10023   // 保存到文件失败
#define ERROR_THIRD_OPERATION_TIME_OUT      10024   // 操作超时
#define ERROR_THIRD_FK_SOLVER_FAILED        10025   // 正运动学计算失败
#define ERROR_THIRD_IK_SOLVER_FAILED        10026   // 逆运动学计算失败
#define ERROR_THIRD_MULTI_MOVE_INVALID      10027   // 组合运动错误状态
#define ERROR_THIRD_MULTI_MOVE_SIZE_INVALID 10028   // 组合运动数量超限
#define ERROR_THIRD_CONFIG_INVALID          10029   // 设置参数错误
#define ERROR_THIRD_RECEIVE_NO_DATA         10030   // 没有数据
#define ERROR_THIRD_RECEIVE_LOSS_DATA       10031   // 接受数据丢包
#define ERROR_ROBOT_NOT_IN_POSITION         10032   // 机器人不处于指定位置
#define ERROR_TARGET_INVALID                10033   // 目标点位不可达
#define ERROR_MEMORY_LACK                   10034   // 内存不足

// 机器人运行模式
enum RobotMode
{
    ROBOT_MODE_MANUAL = 1,  // 手动低速模式
    ROBOT_MODE_AUTO,        // 自动模式
    ROBOT_MODE_MANUFAST     // 手动高速模式
};

// 程序状态
enum ProgramState
{
    PROG_STATE_NOT_LOAD = 0,  // 程序未加载
    PROG_STATE_RUNNING,		  // 程序运行中
    PROG_STATE_PAUSE,		  // 程序暂停
    PROG_STATE_STOP			  // 程序停止
};

// ROBOX 运行模式
enum RoboxKeyMode
{
    ROBOX_MODE_MANUAL = 0,  // 手动低速模式
    ROBOX_MODE_MANUFAST,    // 手动高速模式
    ROBOX_MODE_AUTO         // 自动模式
};

// ROBOX 运行步进模式
enum RoboxStartMode
{
    ROBOX_MODE_CONTINUOUS = 0,      // 连续模式
    ROBOX_MODE_STEPIN,              // 单步进入模式; 程序执行，一次执行一行，遇到子程序会进入
    ROBOX_MODE_STEPOVER,            // 单步跳过模式; 程序执行，一次执行一行，遇到子程序会不进入
    ROBOX_MODE_M_STEPIN,            // 运动单步进入模式；程序执行，直到到达下一个运动指令，进入子程序
    ROBOX_MODE_M_STEPOVER,          // 运动单步跳过模式；程序执行，直到到达下一个运动指令，不进入子程序
    ROBOX_MODE_ANTIDROMIC           // 回退模式
};

// ROBOX Jog模式
enum RoboxJogMode
{
    ROBOX_MODE_JOG = 0, // 关节坐标模式
    ROBOX_MODE_ROBOT,   // 机器人坐标模式
    ROBOX_MODE_TOOL,    // 工具坐标模式
    ROBOX_MODE_UFRAME,  // 用户坐标模式
    ROBOX_MODE_AUX      // 附加轴模式
};

enum RoboxProgramState
{
    RPL_INIT = 1,   // 程序初始化
    RPL_RUN,	    // 程序运行中
    RPL_PAUSE,	    // 程序暂停
    RPL_UNUSED4,    // 未使用
    RPL_STOPPED,    // 程序中止
    RPL_ENDED,      // 程序结束
    RPL_ERROR,       // 程序错误
    RPL_INIT_AFTER_ERROR // 程序从错误状态初始化
};

enum MoveType
{
    PC_MJOINT = 0,   // 关节运动
    PC_MLIN,         // 直线运动
    PC_MCIRC,       // 圆弧运动
    PC_MCIRCA       // 圆弧运动（指定角度）
};

enum MoveState
{
    MOVE_IDLE = 0,
    MOVE_LANUCH = 1,
    MOVE_EXEC = 2
};

enum MultiMoveState
{
    MULTI_MOVE_INIT = 0,
    MULTI_MOVE_LANUCH,
    MULTI_MOVE_EXEC,
    MULTI_MOVE_HOLD_DEC,
    MULTI_MOVE_HOLD,
    MULTI_MOVE_ERROR,
    MULTI_MOVE_ENDED
};

enum MultiMove2State
{
    MULTI_MOVE2_INIT = 0,
    MULTI_MOVE2_LANUCH,
    MULTI_MOVE2_EXEC,
    MULTI_MOVE2_HOLD,
    MULTI_MOVE2_ERROR,
    MULTI_MOVE2_ENDED
};

enum MultiMove2Type
{
    MULTI_MOVE2_MJOINT = 1,
    MULTI_MOVE2_MLIN = 2,
    MULTI_MOVE2_MCIRC = 3,
    MULTI_MOVE2_MCIRC360 = 4,
    // MULTI_MOVE_MCIRCA = 5
};

enum BlockMultiMoveState
{
    BLOCK_MULTI_MOVE_IDLE = 0,
    BLOCK_MULTI_MOVE_LANUCH = 1,
    BLOCK_MULTI_MOVE_RUN = 2,
    BLOCK_MULTI_MOVE_ABORT = 3,
    BLOCK_MULTI_MOVE_STOPPED = 4,
    BLOCK_MULTI_MOVE_ENDED = 5
};

enum MultiMoveType
{
    MULTI_MOVE_MJOINT = 1,
    MULTI_MOVE_MLIN = 2,
    MULTI_MOVE_MCIRC = 3,
    MULTI_MOVE_MCIRC360 = 4,
    // MULTI_MOVE_MCIRCA = 5
};

enum PosType
{
    TYPE_JOINT = 1,
    TYPE_CART = 2
};

enum WristMode
{
    EULERANGLES = 0,
    OBJECTFRAME = 1,
    PATHFRAME = 2,
    AUXPOINTORI = 3,
    WRISTJOINT = 4,
};

enum VariableType
{
    LOCAL = 0,
    GLOBAL,
    GLOBAL_FB,
    MODULE
};

enum ValueType
{
    INIT_VAL = 0,
    RUN_VAL
};

enum RplVarType
{
    RPL_BOOL = 0,
    RPL_INT,
    RPL_UINT,
    RPL_DOUBLE,
    RPL_STRING,
    RPL_POINTJ,
    RPL_POINTC,
    RPL_EPOINTJ,
    RPL_EPOINTC,
    RPL_ARPOINTC,
    RPL_ROBOTTOOL2,
    RPL_ROBOTWORKPIECE2,
    RPL_ROBOT,
    RPL_TRACKING,
    RPL_SPEED,
    RPL_ZONE,
    RPL_CLOCK,
    RPL_TIMER,
    RPL_INTR,
    RPL_VECT3,
};

enum GVarSet
{
    G_EDIT = 0,
    G_TEACH,
};

enum GVarIndex
{
    G_NAME = 0,
    G_ID,
};

enum GVarType
{
    G = 0,
    G_INT,
    G_FLOAT,
    G_BOOL,
    G_STRING,
    G_POINTJ,
    G_POINTC,
    G_ROBOTTOOL,
    G_ROBOTWORKPIECE,
};

enum IOType
{
    DIN,
    DOUT,
    DIN_AND_DOUT
};

enum PrintFlag
{
    PRINT_NONE = 0,
    PRINT_MOVE = 1 << 0,
    PRINT_JOINT = 1 << 1,
    PRINT_BASE = 1 << 2,
    PRINT_USER = 1 << 3,
    PRINT_ALARM = 1 << 4
};

static const unsigned int DOUT_COUNT = 176;
static const unsigned int DIN_COUNT = 176;
static const unsigned int VAR_BOOL_COUNT = 100;
static const unsigned int VAR_INT_COUNT = 100;
static const unsigned int VAR_DOUBLE_COUNT = 100;
static const unsigned int VAR_STRING_COUNT = 20;
static const unsigned int VAR_ROBOT_JOINT_COUNT = 10000;
static const unsigned int VAR_ROBOT_POS_COUNT = 10000;
static const unsigned int VAR_ROBOT_TOOL_COUNT = 200;
static const unsigned int VAR_ROBOT_WOBJ_COUNT = 200;
static const unsigned int MAX_MULTI_MOVE = 10;
static const unsigned int VAR_GET_COUNT = 100;
static const unsigned int VAR_GET_INTERVAL = 10;        // 毫秒