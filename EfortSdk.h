/*******************************************************************
   ABSTRACT: This file is for Efort Controller SDK

   AUTHOR: jidongbao
   E-MAIL: jidongbao@efort.com.cn

   HISTORY: 2022-10-9, created

   VERSION: 2.5.0

   Copyright (c) Efort Intelligent Equipment Ltd. Corp.
   2022 - 2030 All rights reserved

*******************************************************************/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>
#include "SdkConstDef.h"
#include "SdkStructDef.h"

using namespace std;

#ifdef _MSC_VER
#define ROBOTAPI_API __declspec(dllexport)
#else
#define ROBOTAPI_API
#endif

namespace RobotAPI
{
    // SDK 配置
    ROBOTAPI_API int GetObVersion(unsigned int devId = 0);
    ROBOTAPI_API int SetLogPath(char* logpath);
    ROBOTAPI_API int FindRobot(vector<string>& robot_addr);
    ROBOTAPI_API int UpdateIOConfig(unsigned int devId = 0);

    // 机器人连接
    ROBOTAPI_API int ConnectRobot(const string robotAddr, unsigned int& devId, const bool reCntFlag = false, const bool debugFlag = false, int reCntMaxNum = 5);
    ROBOTAPI_API void DisconnectRobot(unsigned int& devId);
    ROBOTAPI_API bool IsConnected(unsigned int devId = 0);
    ROBOTAPI_API int EnableApiControl(const bool enable, unsigned int devId = 0);
    ROBOTAPI_API bool IsApiControl(unsigned int devId = 0);
    ROBOTAPI_API int SelectRobot(unsigned int devId);

    // RoboxCmd
    ROBOTAPI_API int PowerOn(unsigned int devId = 0);
    ROBOTAPI_API int PowerOff(unsigned int devId = 0);
    ROBOTAPI_API int PowerOnManual(unsigned int devId = 0);
    ROBOTAPI_API int PowerOffManual(unsigned int devId = 0);

    ROBOTAPI_API int SetGlobalSpeed(unsigned int ratio, unsigned int devId = 0);
    ROBOTAPI_API int ClearAlarm(unsigned int devId = 0);
    ROBOTAPI_API int IncreaseVelocity(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int DecreaseVelocity(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int TpuBtn2nd(unsigned int devId = 0);
    ROBOTAPI_API int RotateSetJogMode(unsigned int devId = 0);
    ROBOTAPI_API int SetJogMode(int jogmode, unsigned int devId = 0);

    ROBOTAPI_API int SetStartMode(unsigned int devId = 0);
    ROBOTAPI_API int IsProgramFileExist(string filename, unsigned int devId = 0);
    /// <summary>
    /// 获取当前加载程序中的变量
    /// </summary>
    /// <param name="localMap 传出参数类型为map，键：程序变量名；值：int数组，第一个元素为变量id，第二个元素为类型id"></param>
    /// <param name="globMap 传出参数类型为map，键：全局变量名；值：int数组，第一个元素为变量id，第二个元素为类型id"></param>
    /// <returns></returns>
    /// <example>
    /// LoadFromFile("test1.xpl", dev1);
    /// </example>
    ROBOTAPI_API int GetProgramVar(map<string, vector<int>>& localMap, map<string, vector<int>>& globMap, unsigned int devId = 0);
    ROBOTAPI_API int EnumModuleInfo(vector<string> &moduleNameList, unsigned int devId = 0);
    ROBOTAPI_API int EnumModuleVar(string moduleName, map<string, vector<int>>& moduleMap, unsigned int devId = 0);
    ROBOTAPI_API int EnumVarClasses(vector<VarClassInfo>& classInfoList, unsigned int devId = 0);
    //获取当前加载程序中的变量(单个、1维、2维、3维)
    //单个: index[3] = { 0, 0, 0 }
    //1维: index[3] = { 索引值, 0, 0 }
    //2维: index[3] = { 索引值, 索引值, 0 }
    //3维: index[3] = { 索引值, 索引值, 索引值 }
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], int& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], unsigned int& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], double& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], string& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], PointJ& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], PointC& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], EPointJ& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], EPointC& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], ARPointC& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], RobotTool2& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], RobotWorkpiece2& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Robot& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Tracking& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Speed& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Zone& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Clock& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Timer& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Intr& value, unsigned int devId = 0);
    ROBOTAPI_API int GetProVarValue(VariableType varType, int varId, int index[], Vect3& value, unsigned int devId = 0);
    ROBOTAPI_API int SetProVarValue(VariableType varType, ValueType valType, int varId, int index[], const RplVar& rplVar, unsigned int devId = 0);
    ROBOTAPI_API int GetProStepNum(string& proName, int& stepNum, string& moveProName, int& moveStepNum, unsigned int devId = 0);
    ROBOTAPI_API int setPCByStepNum(string proName, int stepNum, unsigned int devId = 0);
    ROBOTAPI_API int enableAllBreakPoints(unsigned int devId = 0); // 启用断点调试功能，每次点击连续时，调用该函数
    ROBOTAPI_API int disableAllBreakPoints(unsigned int devId = 0); // 禁用断点调试功能，该函数一般情况外部可以不调用
    ROBOTAPI_API int removeAllBreakPoints(unsigned int devId = 0); // 移除所有断点
    ROBOTAPI_API int setSingleBreakPoint(string routineName, int stepNum, unsigned int devId = 0); //设置单个断点
    ROBOTAPI_API int removeSingleBreakPoint(string routineName, int stepNum, unsigned int devId = 0); //移除单个断点
    ROBOTAPI_API int setBreakPoints(map<string, list<int>>& breakPointsMap, unsigned int devId = 0); //批量设置断点
    ROBOTAPI_API int GetCurProName(string& proName, unsigned int devId = 0);    // 2.2
    ROBOTAPI_API int EnableProExec(unsigned int devId = 0);
    ROBOTAPI_API int GetXplLog(vector<xplRuntimeLogData>& logList, unsigned int devId = 0);
    ROBOTAPI_API int GetXplPosition(XPLPosition& xplPos, unsigned int devId = 0);

    ROBOTAPI_API int LoadProgram(string name, unsigned int devId = 0, const bool waitFlag = false);
    ROBOTAPI_API int LoadWithStartPro(string name, unsigned int devId = 0);
    ROBOTAPI_API int StartProgram(unsigned int devId = 0);
    ROBOTAPI_API int StopProgram(unsigned int devId = 0);
    ROBOTAPI_API int RestartProgram(unsigned int devId = 0);
    ROBOTAPI_API int TerminateProgram(unsigned int devId = 0);
    ROBOTAPI_API int StartSubProgram(string name, unsigned int devId = 0);
    ROBOTAPI_API int DebugProgram(unsigned int devId = 0);
    ROBOTAPI_API int Jog1Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog1Minus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog2Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog2Minus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog3Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog3Minus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog4Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog4Minus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog5Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog5Minus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog6Plus(bool flag, unsigned int devId = 0);
    ROBOTAPI_API int Jog6Minus(bool flag, unsigned int devId = 0);

    ROBOTAPI_API int MOVEHOLD(unsigned int devId = 0);
    ROBOTAPI_API int MOVERESUME(unsigned int devId = 0);
    ROBOTAPI_API int MOVECLEAR(unsigned int devId = 0);
    ROBOTAPI_API int MOVEJUMP(MoveJumpTarget target, double lim_z, double arch_d1, double arch_d2, double upspeed, double jspeed, double downspeed, string tool, int zone, unsigned int devId = 0);
    ROBOTAPI_API int SETWRISTMODE(WristMode mode, unsigned devId = 0);

    ROBOTAPI_API int MJOINT(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT(double pos[], int speed, double zone = -1, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);
    ROBOTAPI_API int RMJOINT(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT(double pos[], string toolname, string wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMJOINT(double pos[], double apos[], string toolname, string wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG_TWS(double pos[], int cfg[4], string toolname, string wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MJOINT_CFG(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, double zone = -1, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);

    ROBOTAPI_API int MLIN(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN(double pos[], int speed, double zone = -1, unsigned int devId = 0, int printfTime = 10, PrintFlag flag = PRINT_NONE);
    ROBOTAPI_API int RMLIN(double pos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN(double pos[], string toolname, string wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMLIN(double pos[], double apos[], int speed, unsigned int devId = 0);
    ROBOTAPI_API int EMLIN(double pos[], double apos[], string toolname, string wobjname, int speed, unsigned int devId = 0);
    ROBOTAPI_API int MLIN_CFG(double pos[], int speed, int cfgx, int cfg1 = 0, int cfg4 = 0, int cfg6 = 0, unsigned int devId = 0);
    ROBOTAPI_API int MLIN_CFG_TWS(double pos[], int cfg[4], string toolname, string wobjname, int speed, unsigned int devId = 0);

    ROBOTAPI_API int Goto(string varName, MoveType moveType, int speed, unsigned int devId = 0);

    ROBOTAPI_API int SingleAxisSpeed(int axis, double speed, unsigned int devId = 0);
    ROBOTAPI_API int MoveSingleAxis(int axis, double angle, unsigned int devId = 0);

    ROBOTAPI_API int WaitJointPosition(RobotJoint targetJoint, double accuracy, unsigned int devId = 0);
    ROBOTAPI_API int WaitCartPosition(RobotPos targetCartPos, double accuracy, unsigned int devId = 0);
    ROBOTAPI_API int WaitToolPosition(RobotPos targetToolPos, double accuracy, unsigned int devId = 0);

    ROBOTAPI_API int BlockMultiMove(vector<MultiMoveInfo2> list, unsigned int devId = 0);
    ROBOTAPI_API int BlockMultiMoveStop(unsigned int devId = 0);
    ROBOTAPI_API int BlockMultiMoveReset(unsigned int devId = 0);

    ROBOTAPI_API int MultiMoveStart(vector<MultiMovePos> list, unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveHold(unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveResume(unsigned int devId = 0);
    ROBOTAPI_API int MultiMoveReset(unsigned int devId = 0);
    ROBOTAPI_API int GetMultiMoveState(int& value, unsigned int devId = 0);

    ROBOTAPI_API int MultiMove2Start(vector<MultiMoveInfo2> list, unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Hold(unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Resume(unsigned int devId = 0);
    ROBOTAPI_API int MultiMove2Reset(unsigned int devId = 0);
    ROBOTAPI_API int GetMultiMove2State(int& value, unsigned int devId = 0);

    ROBOTAPI_API int GetMoveState(bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetMoveState2(bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetSystermStatus(bool& value, unsigned int devId = 0);

    ROBOTAPI_API int FkSolver(RobotJoint joint, RobotPos& pos, string toolName = "", string wobjName = "", unsigned int devId = 0);
    ROBOTAPI_API int FkSolverPC(RobotJoint joint, RobotPos& pos, string toolName = "", string wobjName = "", unsigned int devId = 0);
    ROBOTAPI_API int IkSolver(RobotPos pos, RobotJoint& joint, string toolName = "", string wobjName = "", unsigned int devId = 0);
    ROBOTAPI_API int IkSolverPC(RobotPos pos, RobotJoint& joint, string toolName = "", string wobjName = "", unsigned int devId = 0);

    ROBOTAPI_API int GetBoolVariable(unsigned int index, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetBoolVariable(unsigned int startIndex, unsigned int len, bool value[], unsigned int devId = 0);
    ROBOTAPI_API int GetIntVariable(unsigned int index, int& value, unsigned int devId = 0);
    ROBOTAPI_API int GetIntVariable(unsigned int startIndex, unsigned int len, int value[], unsigned int devId = 0);
    ROBOTAPI_API int GetDoubleVariable(unsigned int index, double& value, unsigned int devId = 0);
    ROBOTAPI_API int GetDoubleVariable(unsigned int startIndex, unsigned int len, double value[], unsigned int devId = 0);
    ROBOTAPI_API int GetPointJVariable(unsigned int index, PointJ& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetPointCVariable(unsigned int index, PointC& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetJointVariable(unsigned int index, RobotJoint& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int GetPosVariable(unsigned int index, RobotPos& value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetBoolVariable(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int SetBoolVariable(unsigned int startIndex, unsigned int len, bool value[], unsigned int devId = 0);
    ROBOTAPI_API int SetIntVariable(unsigned int index, int value, unsigned int devId = 0);
    ROBOTAPI_API int SetIntVariable(unsigned int startIndex, unsigned int len, int value[], unsigned int devId = 0);
    ROBOTAPI_API int SetDoubleVariable(unsigned int index, double value, unsigned int devId = 0);
    ROBOTAPI_API int SetDoubleVariable(unsigned int startIndex, unsigned int len, double value[], unsigned int devId = 0);
    ROBOTAPI_API int SetPointJVariable(unsigned int index, PointJ value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointJVector(vector<PointJ> pjvec, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointCVariable(unsigned int index, PointC value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPointCVector(vector<PointC> pcvec, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetJointVariable(unsigned int index, RobotJoint value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetJointVector(vector<RobotJoint> pjvec, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPosVariable(unsigned int index, RobotPos value, unsigned int devId = 0, const bool global = false);
    ROBOTAPI_API int SetPosVector(vector<RobotPos> pcvec, unsigned int devId = 0, const bool global = false);

    ROBOTAPI_API int ReadEPointJ(int index, RobotJoint& val, unsigned int devId = 0);
    ROBOTAPI_API int ReadEPointC(int index, RobotPos& val, unsigned int devId = 0);
    ROBOTAPI_API int WriteEPointJ(int index, string pos, unsigned int devId = 0);
    ROBOTAPI_API int WriteEPointC(int index, string pos, unsigned int devId = 0);

    ROBOTAPI_API int PushPointQueue(RobotJoint pos, unsigned int devId = 0);
    ROBOTAPI_API int PushPointQueue(RobotPos pos, unsigned int devId = 0);
    ROBOTAPI_API int ShellCmdSend(string command, string& answer, unsigned int devId = 0);

    // 机器人配置状态
    ROBOTAPI_API int GetDeviceData(DeviceData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetTimeData(TimeData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetMotorData(MotorData& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentAlarmStatus(bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetAlarmList(vector<unsigned int>& list, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentRobotType(string& name, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentEmgStatus(bool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentServoStatus(bool& value, unsigned int devId = 0);
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

    ROBOTAPI_API int GetCurrentToolName(string& name, unsigned int devId = 0);
    ROBOTAPI_API int GetCurrentUframeName(string& name, unsigned int devId = 0);
    ROBOTAPI_API int SetCurrentToolByName(string name, unsigned int devId = 0);
    ROBOTAPI_API int SetCurrentUframeByName(string name, unsigned int devId = 0);
    ROBOTAPI_API int GetToolCoordinate(string name, RobotTool& value, unsigned int devId = 0);
    ROBOTAPI_API int GetUserCoordinate(string name, RobotWorkpiece& value, unsigned int devId = 0);
    ROBOTAPI_API int SetToolCoordinate(string name, RobotTool value, unsigned int devId = 0);
    ROBOTAPI_API int SetUserCoordinate(string name, RobotWorkpiece value, unsigned int devId = 0);
    ROBOTAPI_API int GetToolNameList(vector<string>& nameList, unsigned int devId = 0);
    ROBOTAPI_API int GetUserNameList(vector<string>& nameList, unsigned int devId = 0);

    ROBOTAPI_API void AddUserVariableList(const vector<string> monVarsVector, unsigned int devId = 0);
    ROBOTAPI_API int GetUserMonBoolByName(bool& value, string name, unsigned int devId = 0);
    ROBOTAPI_API int GetUserMonIntByName(int& value, string name, unsigned int devId = 0);
    ROBOTAPI_API int GetUserMonDoubleByName(double& value, string name, unsigned int devId = 0);
    ROBOTAPI_API int GetMonVariable(MonVariable& MonVarList, unsigned int devId = 0);
    ROBOTAPI_API int GetMonVariable_IDE(MonVariableIDE& MonVarList, unsigned int devId = 0);

    // 机器人IO
    ROBOTAPI_API int ReadDIn(unsigned int index, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadDOut(unsigned int index, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadIOs(IOType ioType, int value[], unsigned int devId = 0);
    ROBOTAPI_API int WriteDOut(unsigned int index, bool value, unsigned int devId = 0, bool flag = false);
    ROBOTAPI_API int WriteIOs(IOType ioType, int value[], unsigned int devId = 0);
    ROBOTAPI_API int AutoForceDIn(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int ForceDIn(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int ForceDOut(unsigned int index, bool value, unsigned int devId = 0);
    ROBOTAPI_API int CancelForceDIn(int index, unsigned int devId = 0);
    ROBOTAPI_API int CancelForceDOut(int index, unsigned int devId = 0);

    // 机器人文件操作
    ROBOTAPI_API int LoadFromFile(string name, unsigned int devId = 0, const bool waitFlag = false);
    ROBOTAPI_API int ExportFile(string targetFile, string sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int SaveFile(string targetFile, string sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int LoadFile(string targetFile, string sourceFile, unsigned int devId = 0);
    ROBOTAPI_API int EnumerateFile(string pathName, vector<string>& list, unsigned int devId = 0);
    ROBOTAPI_API int EnumerateFolder(string pathName, vector<string>& list, unsigned int devId = 0);
    ROBOTAPI_API int XplFileIsExist(string filename, unsigned int devId = 0);
    ROBOTAPI_API int DeleteControllerFile(string targetFile, unsigned int devId = 0);
    ROBOTAPI_API int CreateFolder(string path, unsigned int devId = 0);
    ROBOTAPI_API int DeleteFolder(string path, unsigned int devId = 0);
    ROBOTAPI_API int TftpSendFile(const string robot_addr, string remoteTargetDir, vector<string> localFilePathList, unsigned int devId = 0);
    ROBOTAPI_API int TftpSaveFile(const string robot_addr, string localTargetDir, vector<string> remoteFilePathList, unsigned int devId = 0);
    ROBOTAPI_API int ImportFolder(const string localSourceDirPath, const string remoteTargetPath, unsigned int devId = 0, const int flag = 0, double* progress = nullptr);
    ROBOTAPI_API int ExportFolder(const string remoteSourceDirPath, const string localTargetPath, unsigned int devId = 0, const int flag = 0, double* progress = nullptr);

    // 机器人轴操作
    // 本体轴
    ROBOTAPI_API int SetAxesPar(unsigned int axesId, AxesPar axesPar, unsigned int devId = 0);
    // 附加轴
    ROBOTAPI_API int AuxjStepMove1(unsigned int j, unsigned int devId = 0);
    ROBOTAPI_API int AuxjStepMove2(unsigned int j, unsigned int devId = 0);
    ROBOTAPI_API int AuxjStoped(unsigned int j, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int SetAuxjSystemType(unsigned int auxjCount, unsigned int interpCount, unsigned int systemType, unsigned int devId = 0);
    ROBOTAPI_API int SetAuxjSystemType2(GantryInfo gantryInfo, unsigned int auxjCount, unsigned int systemType, unsigned int devId = 0);    // 用于340及以后版本

    // Robox操作模式切换
    ROBOTAPI_API int GetPermission(unsigned int devId = 0);
    ROBOTAPI_API int ReleasePermission(unsigned int devId = 0);
    ROBOTAPI_API int SwitchRoboxMode(RoboxKeyMode mode, unsigned int devId = 0);
    ROBOTAPI_API int SoftwareReset(int flags = 0, unsigned int devId = 0);
    ROBOTAPI_API int HardwareReset(int flags = 0, unsigned int devId = 0);

    // OBS变量读写
    ROBOTAPI_API int ReadBool(string name, bool& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadInt(string name, int& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadDouble(string name, double& value, unsigned int devId = 0);
    ROBOTAPI_API int ReadString(string name, string& value, unsigned int devId = 0);
    ROBOTAPI_API int WriteBool(string name, bool value, unsigned int devId = 0);
    ROBOTAPI_API int WriteInt(string name, int value, unsigned int devId = 0);
    ROBOTAPI_API int WriteDouble(string name, double value, unsigned int devId = 0);
    ROBOTAPI_API int WriteString(string name, string value, unsigned int devId = 0);

    ROBOTAPI_API int GetArcWeldingData(ArcWeldingData& value, unsigned int devId = 0);

    // 系统变量
    ROBOTAPI_API int SetCurGTool(GVarIndex indexType, string name, int id, unsigned int devId = 0);
    ROBOTAPI_API int SetCurGWobj(GVarIndex indexType, string name, int id, unsigned int devId = 0);
    ROBOTAPI_API int GetGVar(GVarIndex indexType, string& name, int& id, GVar& value, string& comment, unsigned int devId = 0);
    ROBOTAPI_API int SetGVar(GVarSet varSet, GVarIndex indexType, string name, int id, const GVar& value, string comment, unsigned int devId = 0);

    // 检测点位可达
    ROBOTAPI_API int CheckTarget(PointC robot_pos, string tool_name, string wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget(PointJ robot_pos, string tool_name, string wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget(RobotPos robot_pos, string tool_name, string wobj_name, unsigned int devId = 0);
    ROBOTAPI_API int CheckTarget(RobotJoint robot_pos, string tool_name, string wobj_name, unsigned int devId = 0);

    // visiontrack
    ROBOTAPI_API int SetPosCToWeldBuffer(vector<VisionTrackInfo> trackList, double speed, unsigned int devId = 0);
    ROBOTAPI_API int ClearWeldBuffer(unsigned int devId = 0);
    ROBOTAPI_API int SetAuxLinSpeed(double speed, unsigned int devId = 0);

    //ROBOTAPI_API int BccShellCmdSend(string command, vector<string>& answer, unsigned int devId = 0);
};
