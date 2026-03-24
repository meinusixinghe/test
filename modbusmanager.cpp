#include "modbusmanager.h"
#include <QDebug>
#include <QtEndian>

ModbusManager::ModbusManager(QObject *parent) : QObject(parent)
{
    m_modbus = new QModbusTcpClient(this);
    m_modbus->setTimeout(1000);
    m_modbus->setNumberOfRetries(2);

    connect(m_modbus, &QModbusClient::stateChanged, this, &ModbusManager::onStateChanged);
    connect(m_modbus, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error){
        emit errorOccurred(m_modbus->errorString());
    });

    // 循环读取定时器 (200ms)
    m_monitorTimer = new QTimer(this);
    connect(m_monitorTimer, &QTimer::timeout, this, &ModbusManager::monitorRobotStatus);
}

ModbusManager::~ModbusManager()
{
    disconnectFromRobot();
}

void ModbusManager::connectToRobot(const QString &ip, int port)
{
    if (m_modbus->state() != QModbusDevice::UnconnectedState)
        m_modbus->disconnectDevice();

    m_modbus->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbus->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);

    if (m_modbus->connectDevice()) {
        emit logMessage(QString("正在连接 %1:%2 ...").arg(ip).arg(port));
    } else {
        emit errorOccurred("连接失败: " + m_modbus->errorString());
    }
}

void ModbusManager::disconnectFromRobot()
{
    if (m_modbus && m_modbus->state() != QModbusDevice::UnconnectedState) {
        m_modbus->disconnectDevice();
    }
}

void ModbusManager::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        emit connectionStateChanged(2);
        emit logMessage("已连接到机器人");
        m_monitorTimer->start(200);
    } else if (state == QModbusDevice::UnconnectedState) {
        emit connectionStateChanged(0);
        m_monitorTimer->stop();
        m_startupState = StartIdle;
        m_jobState = JobIdle;
    } else if (state == QModbusDevice::ConnectingState) {
        emit connectionStateChanged(1);
    }
}

// ==========================================================
// 循环读取机器人状态
// ==========================================================
void ModbusManager::monitorRobotStatus()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) return;
    readRegisters(0, 20);
}

void ModbusManager::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();

        // 确保读到了足够长度的数据
        if (unit.startAddress() == 0 && unit.valueCount() >= 15) {
            quint16 status1    = unit.value(Addr::ROBOT_STATUS);       // 40001
            quint16 progLoaded = unit.value(Addr::ROBOT_PROG_LOAD);    // 40010
            quint16 cmdResp    = unit.value(Addr::ROBOT_CMD_RESP);     // 40014
            quint16 weldDone   = unit.value(Addr::ROBOT_WELD_DONE);    // 40015

            // 解析 40001 状态位
            bool servoStatus = (status1 & (1 << 3));
            bool alarm       = (status1 & (1 << 4));
            bool estop       = (status1 & (1 << 5));
            bool progRunning = (status1 & (1 << 6));
            bool servoReady  = (status1 & (1 << 12));

            // ==========================================
            // 步骤 1 & 2: 启动状态机 (系统就绪与主程序加载)
            // ==========================================
            switch (m_startupState) {
            case CheckSafety:
                if (!alarm && !estop) {
                    emit logMessage("步骤1: 无报警急停，发伺服使能脉冲(40129.Bit12)");
                    pulseControlWord129(ControlBits::SERVO_ENABLE, 300);
                    m_startupState = WaitServoReady;
                } else {
                    emit errorOccurred("步骤1失败：存在报警或急停！");
                    m_startupState = StartIdle;
                }
                break;

            case WaitServoReady:
                if (servoStatus && servoReady) {
                    emit logMessage("伺服已就绪。");
                    if (!progRunning) {
                        emit logMessage(QString("步骤2: 设程序号 40144=%1 并发加载脉冲(40129.Bit4)").arg(m_targetProgram));
                        writeRegister(Addr::PC_PROG_NUM, m_targetProgram); // 先写数据
                        QTimer::singleShot(200, this, [this](){
                            pulseControlWord129(ControlBits::LOAD_PULSE, 300); // 后发加载脉冲
                        });
                        m_startupState = WaitProgramLoaded;
                    } else {
                        emit logMessage("程序已经在运行中。");
                        m_startupState = StartIdle;
                    }
                }
                break;

            case WaitProgramLoaded:
                if (progLoaded == m_targetProgram) {
                    emit logMessage(QString("程序 %1 已加载，发启动脉冲(40129.Bit1)").arg(m_targetProgram));
                    pulseControlWord129(ControlBits::RUN_PULSE, 300);
                    m_startupState = WaitProgramRunning;
                }
                break;

            case WaitProgramRunning:
                if (progRunning) {
                    emit logMessage("步骤2完成：机器人主程序空跑就绪。");
                    m_startupState = StartIdle; // 启动流程结束
                }
                break;

            default: break;
            }

            // ==========================================
            // 步骤 3 & 4: 业务握手状态机
            // ==========================================
            switch (m_jobState) {
            case WaitingCmdAck:
                // 循环读取直到 40035 == 1
                if (cmdResp == 1) {
                    emit logMessage("步骤3: 机器人收到命令(40014=1)，上位机释放命令(40142=0)");
                    writeRegister(Addr::PC_CMD, 0);
                    m_jobState = WaitingCmdClear;
                }
                break;

            case WaitingCmdClear:
                if (cmdResp == 0) {
                    emit logMessage("步骤3结束: 握手完成，机器人开始移动。");
                    m_jobState = WaitingJobDone;
                }
                break;

            case WaitingJobDone:
                // 循环等待动作结束 40036 == 1
                if (weldDone == 1) {
                    emit logMessage("步骤4: 收到动作完成信号(40015=1)，发送应答(40143=1)");
                    emit robotWeldCompleted();
                    writeRegister(Addr::PC_WELD_ACK, 1);
                    m_jobState = WaitingRobotClear;
                }
                break;

            case WaitingRobotClear:
                // 机器人收到应答后清零
                if (weldDone == 0) {
                    emit logMessage("步骤4结束: 机器人已清零(40015=0)，上位机复位(40143=0)");
                    writeRegister(Addr::PC_WELD_ACK, 0);
                    emit jobSentSuccess();
                    m_jobState = JobIdle; // 双方清账
                }
                break;

            default: break;
            }
        }
    }
    reply->deleteLater();
}

// ==========================================================
// 外部调用：触发步骤1和2
// ==========================================================
void ModbusManager::prepareAndStart()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) return;
    if (m_startupState != StartIdle) {
        emit logMessage("启动流程正在进行中...");
        return;
    }
    emit logMessage("开始系统就绪与启动流程");
    m_startupState = CheckSafety; // 推入状态机
}

// ==========================================================
// 外部调用：执行步骤3
// ==========================================================
void ModbusManager::executeCommand(RobotCmd cmd, const WeldingData *data)
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) return;
    if (m_jobState != JobIdle) {
        emit errorOccurred("系统忙: 机器人正在执行上一个任务");
        return;
    }

    m_currentCmdType = cmd;
    emit logMessage(QString("开始下发工艺参数与动作指令 (CMD=%1)").arg(cmd));

    if (data != nullptr) {
        // 下发参数 (按照规定的顺序：R -> Y -> X -> Z -> ID)
        writeRegister(Addr::PC_PROCESS_ID, (quint16)data->processId);
        writeRegisters(Addr::PC_DATA_R, floatToRegisters(data->r));
        writeRegisters(Addr::PC_DATA_Y, floatToRegisters(data->y));
        writeRegisters(Addr::PC_DATA_X, floatToRegisters(data->x));
        writeRegisters(Addr::PC_DATA_Z, floatToRegisters(data->z));
    }

    // 稍作延时，确保参数全部写入完成后下发动作命令 40142
    QTimer::singleShot(200, this, [this](){
        emit logMessage(QString("下发动作命令: 40142 = %1").arg(m_currentCmdType));
        writeRegister(Addr::PC_CMD, (quint16)m_currentCmdType);
        m_jobState = WaitingCmdAck; // 推入状态机
    });
}

// ==========================================================
// 底层辅助函数
// ==========================================================

// 发送脉冲指令 (置 1 -> 延时 -> 置 0)
void ModbusManager::pulseControlWord129(int bitIndex, int durationMs)
{
    m_ctrlWord129 |= (1 << bitIndex);
    writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

    QTimer::singleShot(durationMs, this, [this, bitIndex](){
        m_ctrlWord129 &= ~(1 << bitIndex);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);
    });
}

void ModbusManager::writeRegister(int address, quint16 value)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, 1);
    unit.setValue(0, value);
    m_modbus->sendWriteRequest(unit, 1);
}

void ModbusManager::writeRegisters(int address, const QVector<quint16> &values)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, values);
    m_modbus->sendWriteRequest(unit, 1);
}

void ModbusManager::readRegisters(int address, int count)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, count);
    if (auto *reply = m_modbus->sendReadRequest(unit, 1)) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &ModbusManager::onReadReady);
        else
            delete reply;
    }
}

QVector<quint16> ModbusManager::floatToRegisters(float val)
{
    QVector<quint16> regs;
    union { float f; uint32_t i; } u;
    u.f = val;
    regs.append((quint16)(u.i >> 16));    // Big Endian 高位
    regs.append((quint16)(u.i & 0xFFFF)); // 低位
    return regs;
}
