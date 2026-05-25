/*******************************************************************
   ABSTRACT: This file is for Efort Controller SDK

   AUTHOR: jidongbao
   E-MAIL: jidongbao@efort.com.cn

   HISTORY: 2022-10-9, created

   Copyright (c) Efort Intelligent Equipment Ltd. Corp.
   2022 - 2030 All rights reserved

*******************************************************************/

#pragma once
#include <windows.h>

#include "SdkConstDef.h"
#include "SdkStructDef.h"

using namespace std;
using namespace RobotAPI;

#ifdef _MSC_VER
#define ROBOTAPI_API extern "C" __declspec(dllexport)
#else
#define ROBOTAPI_API
#endif

namespace RobotAPINet
{
    // SDK 配置
    ROBOTAPI_API int GetObVersion(unsigned int devId);
    ROBOTAPI_API int SetLogPath(char* logpath);
    ROBOTAPI_API int FindRobot(char**& robot_addr, int& ipNum);
    ROBOTAPI_API int UpdateIOConfig(unsigned int devId);

    // 机器人连接
    ROBOTAPI_API int ConnectRobot(const char* robotAddr, unsigned int& devId, const BOOL reCntFlag = false, const BOOL debugFlag = false, int reCntMaxNum = 5);
    ROBOTAPI_API void DisconnectRobot(unsigned int& devId);
    ROBOTAPI_API BOOL IsConnected(unsigned int devId = 0);
    ROBOTAPI_API int EnableApiControl(const BOOL enable, unsigned int devId = 0);
    ROBOTAPI_API BOOL IsApiControl(unsigned int devId = 0);
    ROBOTAPI_API int SelectRobot(unsigned int devId);

    // 机器人控制
    ROBOTAPI_API int PowerOn(unsigned int devId = 0);
    ROBOTAPI_API int PowerOff(unsigned int devId = 0);
    ROBOTAPI_API int PowerOnManual(unsigned int devId = 0);
    ROBOTAPI_API int PowerOffManual(unsigned int devId = 0);

    ROBOTAPI_API int SetGlobalSpeed(unsigned int ratio, unsigned int devId = 0);
    ROBOTAPI_API int ClearAlarm(unsigned int devId = 0);
    ROBOTAPI_API int IncreaseVelocity(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int DecreaseVelocity(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int TpuBtn2nd(unsigned int devId = 0);
    ROBOTAPI_API int RotateSetJogMode(unsigned int devId = 0);
    ROBOTAPI_API int SetJogMode(int jogmode, unsigned int devId = 0);

    ROBOTAPI_API int SetStartMode(unsigned int devId = 0);
    ROBOTAPI_API int IsProgramFileExist(char* filename, unsigned int devId = 0);
    ROBOTAPI_API int LoadProgram(const char* progName, unsigned int devId = 0, const bool waitFlag = false);
    ROBOTAPI_API int LoadWithStartPro(char* name, unsigned int devId = 0);
    ROBOTAPI_API int StartProgram(unsigned int devId = 0);
    ROBOTAPI_API int StopProgram(unsigned int devId = 0);
    ROBOTAPI_API int RestartProgram(unsigned int devId = 0);
    ROBOTAPI_API int TerminateProgram(unsigned int devId = 0);
    ROBOTAPI_API int StartSubProgram(char* name, unsigned int devId = 0);

    ROBOTAPI_API int Jog1Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog1Minus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog2Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog2Minus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog3Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog3Minus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog4Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog4Minus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog5Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog5Minus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog6Plus(BOOL flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog6Minus(BOOL flag, unsigned int devId = 0);

    ROBOTAPI_API int MOVEHOLD(unsigned int devId = 0);
    ROBOTAPI_API int MOVERESUME(unsigned int devId = 0);
    ROBOTAPI_API int MOVECLEAR(unsigned int devId = 0);
    ROBOTAPI_API int MOVEJUMP(MoveJumpTarget target, double lim_z, double arch_d1, double arch_d2, double upspeed, double jspeed, double downspeed, char* tool, int zone, unsigned int devId = 0);
    ROBOTAPI_API int SETWRISTMODE(WristMode mode, unsigned devId = 0);

    ROBOTAPI_API int MJOINT(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_ZONE(double pos[], int speed, double zone = -1, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);
    ROBOTAPI_API int RMJOINT(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT1(double pos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMJOINT(double pos[], double apos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG_TWS(double pos[], int cfg[4], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG_ZONE(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, double zone = -1, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);

    ROBOTAPI_API int MLIN(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN_ZONE(double pos[], int speed, double zone, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);
    ROBOTAPI_API int RMLIN(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN1(double pos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMLIN(double pos[], double apos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMLIN1(double pos[], double apos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN_CFG(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, unsigned int devId = 0);
    ROBOTAPI_API int MLIN_CFG_TWS(double pos[], int cfg[4], char* toolname, char* wobjname, int speed, unsigned int devId = 0);

    ROBOTAPI_API int Goto(char* varName, MoveType moveType, int speed, unsigned int devId = 0);

    ROBOTAPI_API int SingleAxisSpeed(int axis, double speed, unsigned int devId = 0);
    ROBOTAPI_API int MoveSingleAxis(int axis, double angle, unsigned int devId = 0);

    ROBOTAPI_API int WaitJointPosition(RobotJoint targetJoint, double accuracy, unsigned int devId = 0);
    ROBOTAPI_API int WaitCartPosition(RobotPos targetCartPos, double accuracy, unsigned int devId = 0);
    ROBOTAPI_API int WaitToolPosition(RobotPos targetToolPos, double accuracy, unsigned int devId = 0);

    ROBOTAPI_API int BlockMultiMove(MultiMoveInfo2* vec, unsigned int len, unsigned int devId = 0);
    ROBOTAPI_API int BlockMultiMoveStop(unsigned int devId = 0);
    ROBOTAPI_API int BlockMultiMoveReset(unsigned int devId = 0);

    ROBOTAPI_API int MultiMoveStart(MultiMovePos* vec, int len, unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveHold(unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveResume(unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveReset(unsigned int devId = 0);
    ROBOTAPI_API int GetMultiMoveState(int& value, unsigned int devId = 0);

    ROBOTAPI_API int MultiMove2Start(MultiMoveInfo2* vec, int len, unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Hold(unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Resume(unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Reset(unsigned int devId = 0);
    ROBOTAPI_API int GetMultiMove2State(int& value, unsigned int devId = 0);

    ROBOTAPI_API int GetMoveState(BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int GetMoveState2(bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetSystermStatus(BOOL& value, unsigned int devId = 0);

    ROBOTAPI_API int FkSolver(RobotJoint joint, RobotPos& pos, char* toolName, char* wobjName, unsigned int devId = 0);
    ROBOTAPI_API int IkSolver(RobotPos pos, RobotJoint& joint, char* toolName, char* wobjName, unsigned int devId = 0);

    ROBOTAPI_API int GetBoolVariable(unsigned int index, BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int GetBoolVariable2(unsigned int startIndex, unsigned int len, BOOL value[], unsigned int devId = 0);
    ROBOTAPI_API int GetIntVariable(unsigned int index, int& value, unsigned int devId = 0);
    ROBOTAPI_API int GetIntVariable2(unsigned int startIndex, unsigned int len, int value[], unsigned int devId = 0);
    ROBOTAPI_API int GetDoubleVariable(unsigned int index, double& value, unsigned int devId = 0);
    ROBOTAPI_API int GetDoubleVariable2(unsigned int startIndex, unsigned int len, double value[], unsigned int devId = 0);
    ROBOTAPI_API int GetPointJVariable(unsigned int index, PointJ& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetPointCVariable(unsigned int index, PointC& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetJointVariable(unsigned int index, RobotJoint& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetPosVariable(unsigned int index, RobotPos& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetBoolVariable(unsigned int index, BOOL value, unsigned int devId = 0);
    ROBOTAPI_API int SetBoolVariable2(unsigned int startIndex, unsigned int len, BOOL value[], unsigned int devId = 0);
    ROBOTAPI_API int SetIntVariable(unsigned int index, int value, unsigned int devId = 0);
    ROBOTAPI_API int SetIntVariable2(unsigned int startIndex, unsigned int len, int value[], unsigned int devId = 0);
    ROBOTAPI_API int SetDoubleVariable(unsigned int index, double value, unsigned int devId = 0);
    ROBOTAPI_API int SetDoubleVariable2(unsigned int startIndex, unsigned int len, double value[], unsigned int devId = 0);
    ROBOTAPI_API int SetPointJVariable(unsigned int index, PointJ value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointJVector(PointJ* pjvec, unsigned int len, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointCVariable(unsigned int index, PointC value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointCVector(PointC* pcvec, unsigned int len, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetJointVariable(unsigned int index, RobotJoint value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetJointVector(RobotJoint* pjvec, unsigned int len, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPosVariable(unsigned int index, RobotPos value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPosVector(RobotPos* pcvec, unsigned int len, unsigned int devId = 0, const bool global = false);

    ROBOTAPI_API int ReadEPointJ(int index, RobotJoint& val, unsigned int devId = 0);
    ROBOTAPI_API int ReadEPointC(int index, RobotPos& val, unsigned int devId = 0);
    ROBOTAPI_API int WriteEPointJ(int index, RobotJoint value, unsigned int devId = 0);
    ROBOTAPI_API int WriteEPointC(int index, RobotPos value, unsigned int devId = 0);

    ROBOTAPI_API int PushPointQueue(RobotJoint pos, unsigned int devId = 0);
    ROBOTAPI_API int PushPointQueue2(RobotPos pos, unsigned int devId = 0);
    ROBOTAPI_API int ShellCmdSend(char* command, char*& answer, unsigned int devId = 0);

    // 机器人配置状态
    ROBOTAPI_API int GetDeviceData(DeviceData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetTimeData(TimeData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetMotorData(MotorData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentAlarmStatus(BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int GetAlarmList(unsigned int*& list, int& len, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentRobotType(char*& name, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentEmgStatus(BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentServoStatus(BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentProStatus(RoboxProgramState& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentStepMode(RoboxStartMode& value, unsigned int devId = 0);
    ROBOTAPI_API int SetCurrentStepMode(RoboxStartMode mode, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentKeyMode(RoboxKeyMode& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentJogMode(RoboxJogMode& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentSpeedRatio(unsigned int& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentSpeed(Robot& value, unsigned int devId = 0);

    ROBOTAPI_API int GetJointPos(RobotJoint& value, unsigned int devId = 0);
    ROBOTAPI_API int BlockGetJointPos(RobotJoint& value, unsigned int devId = 0);
    ROBOTAPI_API int GetBaseCoordinatePos(RobotPos& value, unsigned int devId = 0);
    ROBOTAPI_API int BlockGetBaseCoordinatePos(RobotPos& value, unsigned int devId = 0);
    ROBOTAPI_API int GetBaseCoordinatePos2(RobotPos& value, unsigned int devId = 0);
    ROBOTAPI_API int GetUserCoordinatePos(RobotPos& value, unsigned int devId = 0);
    ROBOTAPI_API int GetUserCoordinatePos2(RobotPos& value, unsigned int devId = 0);
    ROBOTAPI_API int GetPositionData(PosData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetRobotStatusData(RunInfo& value, unsigned int devId = 0);

    ROBOTAPI_API int GetCurrentToolName(char*& name, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentUframeName(char*& name, unsigned int devId = 0);
    ROBOTAPI_API int SetCurrentToolByName(char* name, unsigned int devId = 0);
    ROBOTAPI_API int SetCurrentUframeByName(char* name, unsigned int devId = 0);
    ROBOTAPI_API int GetToolCoordinate(char* name, RobotTool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetUserCoordinate(char* name, RobotWorkpiece& value, unsigned int devId = 0);
    ROBOTAPI_API int SetToolCoordinate(char* name, RobotTool value, unsigned int devId = 0);
    ROBOTAPI_API int SetUserCoordinate(char* name, RobotWorkpiece value, unsigned int devId = 0);
    ROBOTAPI_API int GetToolNameList(char**& nameList, int& len, unsigned int devId = 0);
    ROBOTAPI_API int GetUserNameList(char**& nameList, int& len, unsigned int devId = 0);

    ROBOTAPI_API void AddUserVariableList(char** vec, int len, unsigned int devId = 0);
    ROBOTAPI_API BOOL GetUserMonBoolByName(char* name, unsigned int devId = 0);
    ROBOTAPI_API int GetUserMonIntByName(char* name, unsigned int devId = 0);
    ROBOTAPI_API double GetUserMonDoubleByName(char* name, unsigned int devId = 0);
    ROBOTAPI_API int GetMonVariable(MonVariable& MonVarList, unsigned int devId = 0);

    // 机器人IO
    ROBOTAPI_API int ReadDIn(unsigned int index, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadDOut(unsigned int index, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadIOs(IOType ioType, int value[], unsigned int devId = 0);
    ROBOTAPI_API int WriteDOut(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int WriteIOs(IOType ioType, int value[], unsigned int devId = 0);
    ROBOTAPI_API int ForceDIn(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int ForceDOut(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int CancelForceDIn(int index, unsigned int devId = 0);
    ROBOTAPI_API int CancelForceDOut(int index, unsigned int devId = 0);

    // 机器人文件操作
    ROBOTAPI_API int LoadFromFile(char* name, unsigned int devId = 0, const bool waitFlag = false);
    ROBOTAPI_API int ExportFile(char* targetFile, char* sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int SaveFile(char* targetFile, char* sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int LoadFile(char* targetFile, char* sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int EnumerateFile(char* pathName, char**& vec, int& len, unsigned int devId = 0);
    ROBOTAPI_API int EnumerateFolder(char* pathName, char**& vec, int& len, unsigned int devId = 0);
    ROBOTAPI_API int XplFileIsExist(char* filename, unsigned int devId = 0);
    ROBOTAPI_API int DeleteFile1(char* targetFile, unsigned int devId = 0);
    ROBOTAPI_API int CreateFolder(char* path, unsigned int devId = 0);
    ROBOTAPI_API int DeleteFolder(char* path, unsigned int devId = 0);
    ROBOTAPI_API int TftpSendFile(const char* robot_addr, char* remoteTargetDir, char** localFilePathList, int len, unsigned int devId = 0);
    ROBOTAPI_API int TftpSaveFile(const char* robot_addr, char* localTargetDir, char** remoteFilePathList, int len, unsigned int devId = 0);

    // 机器人轴操作
    // 本体轴
    ROBOTAPI_API int SetAxesPar(unsigned int axesId, AxesPar axesPar, unsigned int devId = 0);
    // 附加轴
    ROBOTAPI_API int AuxjStepMove1(unsigned int j, unsigned int devId = 0);
    ROBOTAPI_API int AuxjStepMove2(unsigned int j, unsigned int devId = 0);
    ROBOTAPI_API int AuxjStoped(unsigned int j, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int SetAuxjSystemType(unsigned int auxjCount, unsigned int interpCount, unsigned int systemType, unsigned int devId = 0);
    ROBOTAPI_API int SetAuxjSystemType2(GantryInfo gantryInfo, unsigned int auxjCount, unsigned int systemType, unsigned int devId = 0);

    // Robox弧焊
    //ROBOTAPI_API int ArcWeldingSetTxVoltage(double value, unsigned int devId = 0);
    //ROBOTAPI_API int ArcWeldingSetTxCurrent(double value, unsigned int devId = 0);
    //ROBOTAPI_API int ArcWeldingSetWeldingSpeed(double value, unsigned int devId = 0);
    //ROBOTAPI_API int ArcWeldingRxVoltage(double& value, unsigned int devId = 0);
    //ROBOTAPI_API int ArcWeldingRxCurrent(double& value, unsigned int devId = 0);

    // Robox操作模式切换
    ROBOTAPI_API int GetPermission(unsigned int devId = 0);
    ROBOTAPI_API int ReleasePermission(unsigned int devId = 0);
    ROBOTAPI_API int SwitchRoboxMode(RoboxKeyMode mode, unsigned int devId = 0);
    ROBOTAPI_API int SoftwareReset(int flags = 0, unsigned int devId = 0);
    ROBOTAPI_API int HardwareReset(int flags = 0, unsigned int devId = 0);

    // OBS变量读写
    ROBOTAPI_API int ReadBool(const char* name, BOOL& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadInt(const char* name, int& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadDouble(const char* name, double& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadString(const char* name, char*& value, unsigned int devId = 0);
    ROBOTAPI_API int WriteBool(const char* name, BOOL value, unsigned int devId = 0);
    ROBOTAPI_API int WriteInt(const char* name, int value, unsigned int devId = 0);
    ROBOTAPI_API int WriteDouble(const char* name, double value, unsigned int devId = 0);
    ROBOTAPI_API int WriteString(const char* name, char* value, unsigned int devId = 0);

    ROBOTAPI_API int GetArcWeldingData(ArcWeldingData& value, unsigned int devId = 0);

    ROBOTAPI_API int CheckTarget_PC(PointC robot_pos, char* tool_name, char* wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget_PJ(PointJ robot_pos, char* tool_name, char* wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget_EPC(RobotPos robot_pos, char* tool_name, char* wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget_EPJ(RobotJoint robot_pos, char* tool_name, char* wobj_name, unsigned int devId = 0);

    ROBOTAPI_API int SetCurGTool(GVarIndex indexType, char* name, int id, unsigned int devId = 0);
    ROBOTAPI_API int SetCurGWobj(GVarIndex indexType, char* name, int id, unsigned int devId = 0);
    ROBOTAPI_API int GetGVarPointJ(GVarIndex indexType, char*& name, int& id, PointJ& value, char*& comment, unsigned int devId = 0);
    ROBOTAPI_API int GetGVarPointC(GVarIndex indexType, char*& name, int& id, G_PointC& value, char*& comment, unsigned int devId = 0);
    ROBOTAPI_API int GetGVarRobotTool(GVarIndex indexType, char*& name, int& id, RobotTool& value, char*& comment, unsigned int devId = 0);
    ROBOTAPI_API int GetGVarRobotWorkpiece(GVarIndex indexType, char*& name, int& id, RobotWorkpiece& value, char*& comment, unsigned int devId = 0);
    ROBOTAPI_API int SetGVarPointJ(GVarSet varSet, GVarIndex indexType, char* name, int id, const PointJ& value, char* comment, unsigned int devId = 0);
    ROBOTAPI_API int SetGVarPointC(GVarSet varSet, GVarIndex indexType, char* name, int id, const G_PointC& value, char* comment, unsigned int devId = 0);

    // 以下接口，为c#独有
    //ROBOTAPI_API int MJOINT_T(double pos[], int speed, unsigned int devId = 0);
    //ROBOTAPI_API int MJOINT_T1(double pos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);

    //ROBOTAPI_API int MLIN_T(double pos[], int speed, unsigned int devId = 0);
    //ROBOTAPI_API int EMLIN_T(double pos[], double apos[], int speed, unsigned int devId = 0);
    //ROBOTAPI_API int EMJOINT_T(double pos[], double apos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    //ROBOTAPI_API int MLIN_T1(double pos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);
    //ROBOTAPI_API int EMLIN_T1(double pos[], double apos[], char* toolname, char* wobjname, int speed, unsigned int devId = 0);

    //ROBOTAPI_API int MoveDoor(DoorMoveInfo* vec, unsigned int devId = 0);
    //ROBOTAPI_API int EnableCheckMoveData(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int ExecTermination(bool& endFlag, unsigned int devId = 0);
    //
    //ROBOTAPI_API int SetRealTimeDataClient(const unsigned int port, const char* ip, unsigned int devId = 0);
    //ROBOTAPI_API int SetRealTimeDataServer(const unsigned int port, const char* ip, unsigned int devId = 0);
    //ROBOTAPI_API int EnableRealTimeDataServer(bool enable, unsigned int devId = 0);
    //ROBOTAPI_API int RealTimeDataClientStart(unsigned int devId = 0);
    //ROBOTAPI_API int RealTimeDataClientStop(unsigned int devId = 0);
    //ROBOTAPI_API int RealTimeDataClientStatus(unsigned int devId = 0);
    //ROBOTAPI_API int GetRealTimeData(RealTimeData& data, unsigned int devId = 0);
};
