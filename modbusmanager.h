#ifndef MODBUSMANAGER_H
#define MODBUSMANAGER_H

#include <QObject>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QTimer>
#include <QVector>
#include <QVariant>

// 焊接数据结构
struct WeldingData {
    float x;
    float y;
    float z;
    float r;
    int processId;
};

class ModbusManager : public QObject
{
    Q_OBJECT
public:
    enum RobotCmd {
        Cmd_Standby             = 0,   // 待机 / 复位
        Cmd_Start_NoLoc         = 10,  // 无定位方式启动
        Cmd_Start_MechLoc       = 20,  // 机械定位方式启动
        Cmd_Start_PointLaser    = 30,  // 点激光定位方式启动
        Cmd_Start_LineLaser     = 40,  // 线激光定位方式启动
        Cmd_Start_3DScan        = 50,  // 3D相机定位方式扫描启动
        Cmd_Start_3DWeld        = 51   // 3D相机定位方式管焊接启动
    };
    Q_ENUM(RobotCmd)

    struct Addr {
        // --- PC 写入 ---
        static const int PC_CONTROL_WORD = 100; // 40101 (位控制: 伺服/运行/加载)
        static const int PC_PROG_NUM     = 103; // 40104 (程序号)

        static const int PC_PROCESS_ID   = 139; // 40140 (工艺号)
        static const int PC_CMD          = 140; // 40141 (动作命令 CMD)
        static const int PC_WELD_ACK     = 141; // 40142 (管焊接完成响应)

        static const int PC_DATA_R       = 144; // 40145 (半径 Real)
        static const int PC_DATA_Y       = 146; // 40147 (Y坐标 Real)
        static const int PC_DATA_X       = 148; // 40149 (X坐标 Real)
        static const int PC_DATA_Z       = 150; // 40151 (Z坐标 Real)

        // --- 机器人 反馈 ---
        static const int ROBOT_STATUS    = 0;   // 40001 (状态位)
        static const int ROBOT_PROG_LOAD = 5;   // 40006 (已加载的程序号)
        static const int ROBOT_CMD_RESP  = 34;  // 40035 (命令响应)
        static const int ROBOT_WELD_DONE = 35;  // 40036 (焊接完成信号)
    };

    // 40101 控制位定义
    struct ControlBits {
        static const int RUN_PULSE       = 1;  // Bit 1: 运行脉冲
        static const int LOAD_PULSE      = 4;  // Bit 4: 加载脉冲
        static const int SERVO_ENABLE    = 12; // Bit 12: 伺服使能脉冲
    };

    explicit ModbusManager(QObject *parent = nullptr);
    ~ModbusManager();

    void connectToRobot(const QString &ip, int port = 502);                 // 连接机器人
    void disconnectFromRobot();                                             // 显式断开连接的接口（用于安全退出）

    // 核心功能接口
    void prepareAndStart();                                                 // 执行步骤1和步骤2 (系统就绪与启动)
    void executeCommand(RobotCmd cmd, const WeldingData *data = nullptr);   // 执行步骤3和4 (业务握手)

    // 预留接口，避免主窗口调用报错
    void setPause(bool paused) {}
    void resetAlarm() {}

signals:
    void connectionStateChanged(int state);                                 // 连接状态变化
    void errorOccurred(QString msg);                                        // 错误信息
    void logMessage(QString msg);                                           // 运行日志
    void jobSentSuccess();                                                  // 任务发送并握手成功
    void robotWeldCompleted();                                              // 机器人焊接完成信号

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onReadReady();                                                     // 读取回调
    void monitorRobotStatus();                                              // 定时轮询机器人状态

private:
    QModbusTcpClient *m_modbus;
    QTimer *m_monitorTimer;                                                 // 监控定时器

    // 步骤1和2：启动状态机
    enum StartupState {
        StartIdle,
        CheckSafety,                                                        // 检查报警/急停并发送伺服脉冲
        WaitServoReady,                                                     // 等待伺服就绪并发送加载脉冲
        WaitProgramLoaded,                                                  // 等待程序加载并发送运行脉冲
        WaitProgramRunning                                                  // 等待程序运行
    };
    StartupState m_startupState = StartIdle;
    int m_targetProgram = 1;

    // 步骤3和4：业务状态机
    enum JobState {
        JobIdle,
        WaitingCmdAck,                                                      // 等待机器人响应 40035=1
        WaitingCmdClear,                                                    // 等待机器人释放 40035=0
        WaitingJobDone,                                                     // 等待动作结束 40036=1
        WaitingRobotClear                                                   // 等待机器人清零 40036=0
    };
    JobState m_jobState = JobIdle;

    quint16 m_ctrlWord101 = 0;                                              // 缓存 40101 的值
    int m_currentCmdType;

    // 辅助函数
    void writeRegister(int address, quint16 value);
    void writeRegisters(int address, const QVector<quint16> &values);
    void readRegisters(int address, int count);
    void pulseControlWord101(int bitIndex, int durationMs);                 // 发送脉冲指令
    QVector<quint16> floatToRegisters(float val);
};

#endif // MODBUSMANAGER_H
