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
        // --- PC 写入 (Write) ---
        static const int PC_CONTROL_BITS = 128; // 40129 (位控制: 启动,预约,复位等)
        static const int PC_GLOBAL_SPEED = 134; // 40135 (设置机器人全局速度)
        static const int PC_RESERVE_PROG = 138; // 40139 (设置预约程序号)
        static const int PC_PROCESS_ID   = 140; // 40141 (工艺序号)
        static const int PC_CMD          = 141; // 40142 (命令字)
        static const int PC_WELD_ACK     = 142; // 40143 (焊接完成响应)

        static const int PC_DATA_X       = 148; // 40149 (坐标 X)
        static const int PC_DATA_Y       = 150; // 40151 (坐标 Y)
        static const int PC_DATA_Z       = 152; // 40153 (坐标 Z)
        static const int PC_DATA_R       = 154; // 40155 (半径)

        // --- 机器人 反馈 (Read) ---
        static const int ROBOT_STATUS_BITS = 0;  // 40001 (状态位)
        static const int ROBOT_CMD_RESP    = 13; // 40014 (命令响应)
        static const int ROBOT_WELD_DONE   = 14; // 40015 (焊接完成信号)
    };

    // 40129 寄存器的位定义
    struct ControlBits {
        static const int SERVO_ON        = 0; // 40129.0
        static const int RESERVE_EN      = 1; // 40129.1
        static const int PAUSE           = 2; // 40129.2
        static const int RESET_ALARM     = 3; // 40129.3
        static const int CONFIRM_RESERVE = 9; // 40129.9 (启动主程序触发)
    };

    explicit ModbusManager(QObject *parent = nullptr);
    ~ModbusManager();

    void connectToRobot(const QString &ip, int port = 502);

    // 核心功能接口
    void triggerRobotStart();                                               // 触发 40129.9
    void executeCommand(RobotCmd cmd, const WeldingData *data = nullptr);
    //void sendWeldingJob(const WeldingData &data, int cmdType = 51);

signals:
    void connectionStateChanged(bool connected);
    void errorOccurred(QString msg);
    void logMessage(QString msg);

    void jobSentSuccess();                                                  // 任务发送并握手成功
    void robotWeldCompleted();                                              // 机器人焊接完成信号

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onReadReady();                                                     // 读取回调
    void monitorRobotStatus();                                              // 定时轮询机器人状态

private:
    QModbusTcpClient *m_modbus;
    QTimer *m_monitorTimer;                                                 // 监控定时器
    bool m_hasValidData = false;

    // 发送流程状态机
    enum SendState {
        Idle,
        CheckingIdle,                                                       // 检查机器人是否空闲(CMD_RESP=0?)
        WritingData,                                                        // 写入坐标和参数
        WritingCmd,                                                         // 写入 CMD=51
        WaitingCmdAck,                                                      // 等待 CMD_RESP=1
        ClearingCmd,                                                        // 清除 CMD=0
        WaitingCmdReset                                                     // 等待 CMD_RESP=0
    };
    SendState m_sendState = Idle;
    WeldingData m_currentData;
    int m_currentCmdType;

    // 焊接完成握手状态机
    enum CompleteState {
        Monitoring,                                                         // 监控 40015
        SendingAck,                                                         // 发送 40143=1
        WaitingRobotClear,                                                  // 等待 40015=0
        ClearingAck                                                         // 发送 40143=0
    };
    CompleteState m_compState = Monitoring;

    // 40129 寄存器当前值缓存 (用于位操作)
    quint16 m_controlRegisterValue = 0;

    // 辅助函数
    void writeRegister(int address, quint16 value);
    void writeRegisters(int address, const QVector<quint16> &values);
    void readRegisters(int address, int count);
    void setControlBit(int bitIndex, bool value);                           // 位操作辅助
    QVector<quint16> floatToRegisters(float val);
};

#endif // MODBUSMANAGER_H
