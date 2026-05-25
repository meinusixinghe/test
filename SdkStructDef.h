/*******************************************************************
   ABSTRACT: This file is for efort sdk structures define

   AUTHOR: jidongbao
   E-MAIL: jidongbao@efort.com.cn

   HISTORY: 2022-10-13, created

   Copyright (c) Efort Intelligent Equipment Ltd. Corp.
   2022 - 2030 All rights reserved

*******************************************************************/

#pragma once

#include "SdkConstDef.h"
#include <cstdint>
#include <string>

namespace RobotAPI {
#pragma pack(1)
#define MAXIMUM_VALUE 1e10
#define MINIMUM_VALUE 1e-10
#define INIT_ZERO_VALUE 0
#define INIT_FALSE_VALUE false
#define MAX_CHAR_LENGTH 127
#define MAX_IO_NUM 32
#define ER_JOINT_NUM 6
#define ER_AUX_NUM 12
#define ESR_JOINT_NUM 4

    // RobotPos 数据类型一共包含16个参数，其中cfgx
    struct RobotPos
    {
        int index;
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
        double ej1;
        double ej2;
        double ej3;
        double ej4;
        double ej5;
        double ej6;
        int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
        RobotPos()
        {
            index = 0;
            x = 0.0;
            y = 0.0;
            z = 0.0;
            a = 0.0;
            b = 0.0;
            c = 0.0;
            ej1 = 0.0;
            ej2 = 0.0;
            ej3 = 0.0;
            ej4 = 0.0;
            ej5 = 0.0;
            ej6 = 0.0;
            cfgx = 0;
            cfg1 = 0;
            cfg4 = 0;
            cfg6 = 0;
        }
    };

    // RobotPos2
    struct RobotPos2
    {
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
    };

    struct PointC
    {
        int index;
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
        unsigned int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
        PointC()
        {
            index = 0;
            x = 0.0;
            y = 0.0;
            z = 0.0;
            a = 0.0;
            b = 0.0;
            c = 0.0;
            cfgx = 0;
            cfg1 = 0;
            cfg4 = 0;
            cfg6 = 0;
        }
    };

    struct G_PointC
    {
        int index;
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
        char cfgx[5];
        int cfg1;
        int cfg4;
        int cfg6;
        int gtfid;
        int gufid;
        G_PointC()
        {
            index = -1;
            x = 0.0;
            y = 0.0;
            z = 0.0;
            a = 0.0;
            b = 0.0;
            c = 0.0;
#ifdef _WIN32
            strcpy_s(cfgx, sizeof(cfgx), "");
#else
            strncpy(cfgx, "", sizeof(cfgx));
            cfgx[sizeof(cfgx) - 1] = '\0';
#endif
            cfg1 = 0;
            cfg4 = 0;
            cfg6 = 0;
            gtfid = 0;
            gufid = 0;
        }
    };

    struct PointJ
    {
        int index;
        double j1;
        double j2;
        double j3;
        double j4;
        double j5;
        double j6;
        PointJ()
        {
            index = 0;
            j1 = 0.0;
            j2 = 0.0;
            j3 = 0.0;
            j4 = 0.0;
            j5 = 0.0;
            j6 = 0.0;
        }
    };

    // RobotJoint
    // 6轴机器人， j[0] ~ j[5]， 关节角度，j[6] ~ j[11], 附加轴角度
    struct RobotJoint
    {
        int index;
        double j[ER_AUX_NUM];
        RobotJoint()
        {
            index = 0;
            for (int i = 0; i < ER_AUX_NUM; i++)
            {
                j[i] = MAXIMUM_VALUE;
            }
        }
    };

    // 带附加轴的点，IDE专用
    struct EPointJ
    {
        PointJ pose;
        PointJ eaux;
    };

    struct EPointC
    {
        PointC pose;
        PointJ eaux;
    };

    struct XPLPosition
    {
        float qj[ER_JOINT_NUM];        // joint Position
        float qt[ER_JOINT_NUM];        // cartesian position referred to world refsys
        float qp[ER_JOINT_NUM];        // cartesian position referred to actual refsys
        float eauxQj[ER_JOINT_NUM];    // joint position of external auxiliary axes
    };
    // 惯量
    struct RobotInertiaMoment
    {
        double ix;
        double iy;
        double iz;
    };

    struct RobotTool
    {
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
        bool fixed;
    };

    // 工具坐标系
    struct RobotTool2
    {
        RobotPos2 dim;
        double mass;
        RobotPos2 cog;
        RobotInertiaMoment inertial;
        bool fixed;
    };

    // 用户坐标系
    struct RobotWorkpiece
    {
        double x;
        double y;
        double z;
        double a;
        double b;
        double c;
        bool onrobot;
        // std::string basename;
    };

    // 用户坐标系2
    struct RobotWorkpiece2
    {
        RobotPos2 uframe;
        RobotPos2 oframe;
        bool moveable;
        bool onrobot;
        char basename[MAX_CHAR_LENGTH];
    };

    struct ARPointC
    {
        PointC pose;
        PointJ eaux;
        char toolName[MAX_CHAR_LENGTH];
        char refsysName[MAX_CHAR_LENGTH];
    };

    struct strToolVar
    {
        std::string name;
        int id;
        int classID;
        std::string initValue;
        strToolVar(){}
    };

    struct strRefVar
    {
        std::string name;
        int id;
        int classID;
        std::string initValue;
    };

    struct xplRuntimeMemoMoveData
    {
        double qj[ER_JOINT_NUM];
        double qt[ER_JOINT_NUM];
        double qp[ER_JOINT_NUM];
        double eauxQj[ER_JOINT_NUM];
        int toolVarID;
        int refsysVarID;
        int speedVarID;
        int zoneVarID;
        int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
    };

    struct xplRuntimeStatusInfo
    {
        // Environment data
        int sysLogCounter;	// XPL environment log record counter from its creation
        int envMagic;		// environment magic number (environment variables definitions changed) @@@ if changed reload definitions
        int execPouId;		// execution pou id
        int execStepId;		// execution step id !!! we use pouStepId
        int enaCmd;			// enabled command mask
        int envSts;			// environment status (RUN, PAUSE, LOADED)
        int movePouId;		// movement pou id
        int moveStepId;		// movement step id
        int usrLogCounter;	// User log record counter (messages)
        int isModified;		// actual program and/or a module is modified and need to be saved (1=Program modified)
        float qj[ER_JOINT_NUM];          // joint position
        float qt[ER_JOINT_NUM];          // cartesian position referred to world refsys
        float qp[ER_JOINT_NUM];          // cartesian position referred to actual refsys
        int toolVarID;
        int refsysVarID;
        float eauxQj[ER_JOINT_NUM];      // joint position of exeternal auxiliary axes
        // POU data (ID pou <> 0) !!! Not used data (obsolete) !!!
        int pouSts;		    // POU status (RUN, PAUSE, LOADED)
        int pouStepId;		// POU execution step ID (it can be a CALL, so the true execution step is the step of the subroutine)
    };

    struct MultiMovePos
    {
        int type;      //MultiMoveType
                       //1, move joint
                       //2, move cart
                       //3, move circle
        int cfg;       //point_c, 上下手腕模式（预留），contains cfgx, cfg1, cfg4, cfg6
        double pos[ER_AUX_NUM];//point_j: j1,j2,j3,j4,j5,j6,aux1,aux2,aux3,aux4,aux5,aux6
                       //point_c, x, y, z, a, b, c, aux1, aux2, aux3, aux4, aux5, aux6
        double speed;  //运动点速度
                       //joint, 1- 100
                       //cart mm/s
        double overlapping;//运动点圆弧过渡
        MultiMovePos()
        {
            for (int i = 0; i < ER_AUX_NUM; i++)
            {
                pos[i] = 0.0;
            }
        }
    };

    struct DoorMoveInfo
    {
        double pos[ESR_JOINT_NUM];
        int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
        double speed;
        double overlapping;
        int flag;
    };

    struct MoveJumpTarget
    {
        double pos[ESR_JOINT_NUM];
        int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
    };

    struct MultiMoveInfo2
    {
        int moveType;
        int posType;
        RobotJoint ap[4];
        RobotPos cp[4];
        double speed;   // (0,100]
        double acc;     // (0,100]
        double dec;     // [0,100]
        double jerk;    // (0,100]
        double overlapping;
        double auxOverlapping;
        int flags;

        MultiMoveInfo2()
        {
            moveType = 1;
            posType = 1;
            speed = 1;
            acc = 100;
            dec = 0;
            jerk = 100;
            overlapping = 0;
            auxOverlapping = 1e100;
            flags = 0;
        }

    };

    struct BlockMultiMoveInfo
    {
        int moveType;
        // 1, move joint
        // 2, move line
        // 3, move circle
        // 4, move circle angle
        int posType;
        // 1, joint
        // 2, cartesian
        double pos1[ER_AUX_NUM];    // target joint /cartesian pos
        int pos1Cfg[4];     // target cartesian pos CF
        double pos2[ER_AUX_NUM];    // Intermediate point /  Center of the circle
        int pos2Cfg[4];     //
        double angle;       // for move circle angle
        int flags;          // for move circle
        double speed;       // Desired speed, with respect to the max_spe_c set for the axes group. [0-1]
        double acc;         // Desired acceleration, with respect to the max_acc_c set for the axes group.
        double dec;         // Desired deceleration, with respect to the max_acc_c set for the axes group.
                            // The 0 value will be interpreted as 'deceleration same as acceleration
        double jerk;        // Desired jerk, with respect to the max_jer_c set for the axes group. [0-1].
                            // The 0 value will be interpreted as 'no jerk' so admitting step changes of acceleration, like if the jerk parameter would be infinite
        double overlapping; // Maximum distance from the target to reach before launching any overlapping move

        BlockMultiMoveInfo()
        {
            moveType = 1;
            posType = 1;
            for (int i = 0; i < ER_AUX_NUM; i++)
            {
                pos1[i] = MAXIMUM_VALUE;
                pos2[i] = MAXIMUM_VALUE;
            }
            for (int i = 0; i < 4; i++)
            {
                pos1Cfg[i] = INIT_ZERO_VALUE;
                pos2Cfg[i] = INIT_ZERO_VALUE;
            }

            angle = 0;
            flags = 0;
            speed = 1;
            acc = 1;
            dec = 0;
            jerk = 1;
            overlapping = 0;
        }
    };

    struct AxesPar
    {
        double minStrJ;
        double maxStrJ;
        double maxSpeJ;
        double manSpeJ;
        double manAccJ;
        double manDecJ;
        double manJerJ;
        AxesPar()
        {
            minStrJ = MAXIMUM_VALUE;
            maxStrJ = MAXIMUM_VALUE;
            maxSpeJ = MAXIMUM_VALUE;
            manSpeJ = MAXIMUM_VALUE;
            manAccJ = MAXIMUM_VALUE;
            manDecJ = MAXIMUM_VALUE;
            manJerJ = MAXIMUM_VALUE;
        }
    };

    struct GantryInfo
    {
        bool enable;            // 有无双驱轴
        // unsigned int index = 0;     // 双驱轴编号，在当前版本中固定等于最后一个附加轴
        bool sameDirection;     // true双轴同向
        double maxError;        // 最大误差（0.01-100）
        GantryInfo()
        {
            enable = false;
            sameDirection = false;
            maxError = 1;
        }
    };

    struct MonVariable
    {
        uint32_t Variable_fromAlarmN[33];
        double LOV_ERAG_QJ[13];
        double LOV_ERAG_QP[7];
        double LOV_ERAG_QT[7];
        double LOV_RPL_MAN_AG_FR;
        int32_t LOV_ERAG_AUX_NUM;
        int32_t LOV_ERAG_JOINT_NUM;
        int32_t LOV_ERAG_OPE_MODE;
        int32_t LOV_RPL_MAN_HMI_STS;
        int32_t LOV_RPL_MAN_JOG_MODE;
        int32_t LOV_RPL_MAN_KEY_MODE;
        int32_t LOV_RPL_MAN_RPL_STATUS;
        int32_t LOV_RPL_MAN_START_MODE;
        bool LOV_IComUtil_PC_BControl;
        MonVariable()   // 监控列表中的变量起始编号为1。为了对应列表中的变量，数组首元素为占位元素
        {
            Variable_fromAlarmN[0] = 0;
            LOV_ERAG_QJ[0] = 0;
            LOV_ERAG_QP[0] = 0;
            LOV_ERAG_QT[0] = 0;
        }
    };

	struct MonVariableIDE
    {
        uint32_t Variable_fromAlarmN[33];   // Variable_fromAlarmN[1]即GetAlarmStatus接口内部变量，Variable_fromAlarmN[1]--Variable_fromAlarmN[32]即GetAlarmList接口内部变量
        double LOV_ERAG_QJ[13];             // LOV_ERAG_QJ[1]--LOV_ERAG_QJ[12]即GetJointPos接口内部变量
        double LOV_ERAG_QT[7];              // LOV_ERAG_QT[1]--LOV_ERAG_QT[6]GetBaseCoordinatePos接口内部变量
        int32_t LOV_RPL_MAN_RPL_STATUS;     // GetCurrentProStatus内部变量
        bool IO_MAN_DI[32];
        bool IO_MAN_DO[32];
        /*double LOV_ERAG_QP[7];
        double LOV_RPL_MAN_AG_FR;
        int32_t LOV_ERAG_AUX_NUM;
        int32_t LOV_ERAG_JOINT_NUM;
        int32_t LOV_ERAG_OPE_MODE;
        int32_t LOV_RPL_MAN_HMI_STS;
        int32_t LOV_RPL_MAN_JOG_MODE;
        int32_t LOV_RPL_MAN_KEY_MODE;
        int32_t LOV_RPL_MAN_START_MODE;
        bool LOV_IComUtil_PC_BControl;*/
        MonVariableIDE()   // 监控列表中的变量起始编号为1。为了对应列表中的变量，数组首元素为占位元素
        {
            Variable_fromAlarmN[0] = 0;
            LOV_ERAG_QJ[0] = 0;
            LOV_ERAG_QT[0] = 0;
            std::fill(IO_MAN_DI, IO_MAN_DI + 32, false);
            std::fill(IO_MAN_DO, IO_MAN_DO + 32, false);
            /*LOV_ERAG_QP[0] = 0;*/
        }
    };

    struct RealTimeData
    {
        double stime;   // 控制器开机时间，单位：秒
        double qj[ER_JOINT_NUM];   // 关节位置
        double qp[ER_JOINT_NUM];   // 用户坐标系位置
        double ej[ER_JOINT_NUM];   // 附加轴位置
        unsigned int cfgx;
        int cfg1;
        int cfg4;
        int cfg6;
        int heartBeat;
        int padding;
        RealTimeData()
        {
            stime = 0;
            for (int i = 0; i < ER_JOINT_NUM; i++)
            {
                qj[i] = MAXIMUM_VALUE;
                qp[i] = MAXIMUM_VALUE;
                ej[i] = MAXIMUM_VALUE;
            }
            cfgx = 0;
            cfg1 = 0;
            cfg4 = 0;
            cfg6 = 0;
            heartBeat = 0;
            padding = 0;
        }
    };

    struct VisionTrackInfo
    {
        int flag;
        double pos[ER_AUX_NUM];
        VisionTrackInfo()
        {
            flag = 0;
            pos[0] = 0;
            pos[1] = 0;
            pos[2] = 0;
            pos[3] = 0;
            pos[4] = 0;
            pos[5] = 0;
            pos[6] = 0;
            pos[7] = 0;
            pos[8] = 0;
            pos[9] = 0;
            pos[10] = 0;
            pos[11] = 0;
        }
    };

    struct VarClassInfo
    {
        int varClassId;
        std::string varClassName;
        VarClassInfo()
        {
            varClassId = 0;
            varClassName = "";
        }
    };

    struct Robot
    {
        struct abst
        {
            double speed;
            double acc;
            double dec;
            double jerk;
        };
        struct absw
        {
            double speed;
            double acc;
            double dec;
            double jerk;
        };
        struct ovrj
        {
            double speed;
            double acc;
            double dec;
            double jerk;
        };
        char name[MAX_CHAR_LENGTH];
        abst abst;
        absw absw;
        ovrj ovrj;
    };

    struct Tracking
    {
        double maxspace;
        double startspace;
        double maxspe;
        double maxacc;
        char type[MAX_CHAR_LENGTH];
        double Parl;
    };

    struct Speed
    {
        double v_Tcp;
        double v_Ori;
        double v_ext_lin;
        double v_ext_rot;
        Speed()
        {
            v_Tcp = 0;
            v_Ori = 0;
            v_ext_lin = 0;
            v_ext_rot = 0;
        }
    };

    struct Zone
    {
        double mov_Tcp;
        double reori_Ori;
        Zone()
        {
            mov_Tcp = 0;
            reori_Ori = 0;
        }
    };

    struct Clock
    {
        double clk;
        Clock()
        {
            clk = 0;
        }
    };

    struct Timer
    {
        double tmrR;
        bool tmrQ;
        Timer()
        {
            tmrR = 0;
            tmrQ = false;
        }
    };

    struct Trigger
    {
        int type;
        double parType;
        Trigger()
        {
            type = 0;
            parType = 0;
        }
    };

    struct Intr
    {
        char subRoutine[MAX_CHAR_LENGTH];
        int num;
        Intr()
        {
            num = 0;
        }
    };

    struct Vect3
    {
        double x;
        double y;
        double z;
        Vect3()
        {
            x = 0;
            y = 0;
            z = 0;
        }
    };

    struct RplVar
    {
        RplVarType rplVarType;

        union RplVarVal
        {
            bool rplBool;
            int rplInt;
            unsigned int rplUint;
            double rplDouble;
            char rplString[MAX_CHAR_LENGTH];
            PointJ rplPointJ;
            PointC rplPointC;
            EPointJ rplEPointJ;
            EPointC rplEPointC;
            RobotTool2 rplRobotTool2;
            RobotWorkpiece2 rplRobotWorkpiece2;
            Robot rplRobot;
            Tracking rplTracking;
            Speed rplSpeed;
            Zone rplZone;
            Clock rplClock;
            Timer rplTimer;
            Intr rplIntr;
            Vect3 rplVect3;

            RplVarVal() {}
            ~RplVarVal() {}
        }rplVarVal;

        RplVar() {}
        ~RplVar() {}
    };

    struct xplRuntimeLogData
    {
        uint32_t type;  // log=1, info=2, warning=3, error=4
        std::string time;
        std::string params;
    };

    struct GVar
    {
        GVarType gVarType;

        union GVarVal
        {
            int gInt;
            double gFloat;
            bool gBool;
            char gString[MAX_CHAR_LENGTH];
            PointJ gPointJ;
            G_PointC gPointC;
            RobotTool gRobotTool;
            RobotWorkpiece gRobotWorkpiece;

            GVarVal() {}
            ~GVarVal() {}
        }gVarVal;

        GVar() {}
        ~GVar() {}
    };

    struct PosData
    {
        double acsPos[ER_JOINT_NUM];  //关节坐标
        double kcsPos[ER_JOINT_NUM];  //基座坐标
        double pcsPos[ER_JOINT_NUM];  //用户坐标
        double auxPos[ER_JOINT_NUM];  //附加轴坐标
    };

    struct RunInfo
    {
        bool connectMode;
        RoboxKeyMode keyMode;
        RoboxJogMode coordSysID;
        char toolName[MAX_CHAR_LENGTH];
        char wobjName[MAX_CHAR_LENGTH];
        unsigned int velocity;
        bool powerStatus;
        bool robotRunMode;
        RoboxProgramState programStatus;
        int programNumber;
        char programName[MAX_CHAR_LENGTH];
        double tcpSpeed;
        int grplInt[5];
    };

    struct MotorData
    {
        double current[6];
        double voltage[6];
        double loadRate[6];
    };

    struct ArcWeldingData
    {
        bool rxPowerAlarm;
        bool rxBcollision;
        bool rxArcexisted;
        int txWorkMode;
        double txCurrent;
        double txVoltage;
        double rxCurrent;
        double rxVoltage;
    };

    struct KeyLic
    {
        char fileName[MAX_CHAR_LENGTH];
        char compactFlash[MAX_CHAR_LENGTH];
        char signature[MAX_CHAR_LENGTH];
        //char qualifiedPn[MAX_CHAR_LENGTH];
        //char nAxes[MAX_CHAR_LENGTH];
        //char ladderEnable[MAX_CHAR_LENGTH];
        //char pathEnable[MAX_CHAR_LENGTH];
        //char ob[MAX_CHAR_LENGTH];
    };

    struct RobotInfo
    {
        char robotMode[MAX_CHAR_LENGTH];
        char bodyNumber[MAX_CHAR_LENGTH];
        char cabinetNumber[MAX_CHAR_LENGTH];
        char assemblyNumber[MAX_CHAR_LENGTH];
        char manufactureDate[MAX_CHAR_LENGTH];
        char softwareVersion[MAX_CHAR_LENGTH];
        char rdeVersion[MAX_CHAR_LENGTH];
        char tpuVersion[MAX_CHAR_LENGTH];
        char firmwareVersion[MAX_CHAR_LENGTH];
        char userRobotName[MAX_CHAR_LENGTH];
        char customUserRobotName[MAX_CHAR_LENGTH];
    };

    struct DeviceData
    {
        KeyLic keyLic;
        RobotInfo robotInfo;
    };

    struct TimeData
    {
        char systemTime[MAX_CHAR_LENGTH];
        char servoOnTotalTime[MAX_CHAR_LENGTH];
        char powerOnTotalTime[MAX_CHAR_LENGTH];
        char alarmTotalTime[MAX_CHAR_LENGTH];
    };

#pragma pack()
};
