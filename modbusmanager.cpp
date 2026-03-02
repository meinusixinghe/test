#include "modbusmanager.h"
#include "qvariant.h"
#include <QDebug>
#include <QtEndian>

ModbusManager::ModbusManager(QObject *parent) : QObject(parent)
{
    m_modbus = new QModbusTcpClient(this);                                                                  // 初始化Modbus TCP客户端
    m_modbus->setTimeout(1000);                                                                             // 设置通信超时1s
    m_modbus->setNumberOfRetries(2);                                                                        // 设置重试次数2次

    // 连接 Modbus状态变化、错误信号
    connect(m_modbus, &QModbusClient::stateChanged, this, &ModbusManager::onStateChanged);
    connect(m_modbus, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error){
        emit errorOccurred(m_modbus->errorString());
    });

    // 用于监控机器人是否发来焊接完成信号，以及握手状态检查
    m_monitorTimer = new QTimer(this);
    connect(m_monitorTimer, &QTimer::timeout, this, &ModbusManager::monitorRobotStatus);
}

ModbusManager::~ModbusManager()
{
    // 析构时断开连接，释放资源
    if (m_modbus->state() == QModbusDevice::ConnectedState)
        m_modbus->disconnectDevice();
}

// ----------------------------------------------------
// 功能：实现上位机与机器人的 Modbus TCP 连接
// ----------------------------------------------------
void ModbusManager::connectToRobot(const QString &ip, int port)
{
    if (m_modbus->state() != QModbusDevice::UnconnectedState)
        m_modbus->disconnectDevice();                                                                       // 先断开原有连接，避免冲突

    // 设置机器人IP和端口，发起连接
    m_modbus->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbus->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);

    if (m_modbus->connectDevice()) {
        emit logMessage(QString("正在连接 %1:%2 ...").arg(ip).arg(port));
    } else {
        emit errorOccurred("连接失败: " + m_modbus->errorString());
    }
}

// ----------------------------------------------------
// 功能：Modbus 连接状态变化时的回调函数
// 连接成功启动轮询、断开连接停止轮询并复位状态
// ----------------------------------------------------
void ModbusManager::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        emit connectionStateChanged(true);
        emit logMessage("已连接到机器人");
        m_monitorTimer->start(200);                                                 // 启动轮询
    } else if (state == QModbusDevice::UnconnectedState) {
        emit connectionStateChanged(false);
        m_monitorTimer->stop();                                                     // 断开连接，停止轮询
        m_sendState = Idle;                                                         // 重置状态
        m_compState = Monitoring;
    }
}

// ============================================================
//  功能: 一键启动 (准备阶段 + 触发启动)
//  对应描述: 上位机下发配置 -> 伺服上电/预约使能 -> 触发确认预约
// ============================================================
void ModbusManager::prepareAndStart()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) {
        emit errorOccurred("未连接机器人");
        return;
    }

    emit logMessage("正在配置机器人参数并启动...");

    // 1. 设置预约程序号 = 1 (地址 40139 / Offset 138)
    // 对应您的需求：设置预约程序号=1
    writeRegister(Addr::PC_RESERVE_PROG, 1);

    // 2. 设置控制字：伺服上电(Bit0=1) + 预约功能启动(Bit1=1)
    // 此时先不触发 Bit9，只是准备状态
    // m_controlRegisterValue 是本地缓存
    m_controlRegisterValue |= (1 << ControlBits::SERVO_ON);   // Bit 0
    m_controlRegisterValue |= (1 << ControlBits::RESERVE_EN); // Bit 1
    m_controlRegisterValue &= ~(1 << ControlBits::CONFIRM_RESERVE); // 确保 Bit 9 先是 0
    m_controlRegisterValue &= ~(1 << ControlBits::PAUSE);

    writeRegister(Addr::PC_CONTROL_BITS, m_controlRegisterValue);

    // 3. 延时一小会儿，触发“确认预约程序” (Bit 9 置 1)
    // 模拟人手按下的操作
    QTimer::singleShot(200, this,[this](){
        triggerConfirmSignal();
    });
}

// ============================================================
//  功能: 触发确认预约信号 (脉冲)
//  场景: 用于首次启动，或暂停后的恢复
// ============================================================
void ModbusManager::triggerConfirmSignal()
{
    emit logMessage("触发: 确认预约程序 (Pulse)...");

    // 置 1
    setControlBit(ControlBits::CONFIRM_RESERVE, true);

    // 500ms 后置 0
    QTimer::singleShot(500, this,[this](){
        setControlBit(ControlBits::CONFIRM_RESERVE, false);
        emit logMessage("启动信号已发送");
    });
}

// 暂停功能
void ModbusManager::setPause(bool paused)
{
    if (paused) {
        // --- 暂停逻辑 ---
        emit logMessage("发送: 暂停程序...");
        setControlBit(ControlBits::PAUSE, true); // 40129.2 置 1
    } else {
        // --- 继续逻辑 ---
        emit logMessage("发送: 继续运行...");

        // 1. 先清除暂停信号
        setControlBit(ControlBits::PAUSE, false); // 40129.2 置 0

        // 2. 再次触发 "确认预约程序" 以恢复运行
        // 稍作延时确保暂停位已清除
        QTimer::singleShot(100, this,[this](){
            triggerConfirmSignal();
        });
    }
}

// 复位报警功能
void ModbusManager::resetAlarm()
{
    emit logMessage("发送: 复位报警...");
    setControlBit(ControlBits::RESET_ALARM, true);

    // 点动复位
    QTimer::singleShot(500, this,[this](){
        setControlBit(ControlBits::RESET_ALARM, false);
    });
}

// 位操作核心逻辑：读改写 (Read-Modify-Write) 的简化版
// 实际工程中，建议先读回 40129 的当前值，修改后再写回。
// 这里假设我们维护了 m_controlRegisterValue
void ModbusManager::setControlBit(int bitIndex, bool value)
{
    if (value)
        m_controlRegisterValue |= (1 << bitIndex);
    else
        m_controlRegisterValue &= ~(1 << bitIndex);

    writeRegister(Addr::PC_CONTROL_BITS, m_controlRegisterValue);
}

// ============================================================
//  核心: 状态机与轮询
// ============================================================
void ModbusManager::monitorRobotStatus()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) return;

    // 批量读取机器人状态:
    // 从 40014 开始读 2个字 (40014 CMD响应, 40015 焊接完成)
    readRegisters(Addr::ROBOT_CMD_RESP, 2);
}

void ModbusManager::executeCommand(RobotCmd cmd, const WeldingData *data)
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) {
        emit errorOccurred("未连接设备");
        return;
    }
    if (m_sendState != Idle) {
        emit errorOccurred("系统忙: 上一个指令未完成");
        return;
    }

    // 1. 保存命令类型
    m_currentCmdType = cmd;

    // 2. 判断是否需要写入坐标数据
    if (data != nullptr) {
        m_currentData = *data; // 复制数据
        m_hasValidData = true; // 标记有数据
    } else {
        m_hasValidData = false; // 标记无数据 (纯指令模式)
    }

    // 3. 启动状态机
    m_sendState = CheckingIdle;
    emit logMessage(QString("准备执行指令 CMD=%1...").arg(cmd));

    // 立即触发一次状态检查
    readRegisters(Addr::ROBOT_CMD_RESP, 2);
}

void ModbusManager::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();

        // 我们请求的是从 40014 开始的
        if (unit.startAddress() == Addr::ROBOT_CMD_RESP && unit.valueCount() >= 2) {
            quint16 cmdResp = unit.value(0);   // 40014
            quint16 weldDone = unit.value(1);  // 40015

            // --- 状态机 1: 发送任务握手 ---
            switch (m_sendState) {
            case CheckingIdle:
                if (cmdResp == 0) {
                    // [核心修改] 根据是否有数据，决定下一步动作
                    if (m_hasValidData) {
                        emit logMessage("写入坐标数据...");
                        // 只有在有数据时，才写入 X,Y,Z,R,ID
                        QVector<quint16> payload;
                        payload << floatToRegisters(m_currentData.x);
                        payload << floatToRegisters(m_currentData.y);
                        payload << floatToRegisters(m_currentData.z);
                        payload << floatToRegisters(m_currentData.r);
                        writeRegisters(Addr::PC_DATA_X, payload);
                        writeRegister(Addr::PC_PROCESS_ID, (quint16)m_currentData.processId);

                        // 写完数据后，进入写 CMD 状态
                        m_sendState = WritingCmd;
                    } else {
                        // 如果是无数据指令 (如机械定位)，直接跳过写数据步骤，去写 CMD
                        emit logMessage("纯指令模式，跳过数据写入...");

                        // 直接跳转到写命令状态 (或者在这里直接写命令也可以)
                        // 为了逻辑统一，我们让状态机流转一下
                        m_sendState = WritingCmd;
                        // 可以在这里加个极短的延时或直接触发
                        QTimer::singleShot(10, this,[this](){
                            // 重新触发读取以进入 WritingCmd 分支，或者直接调用写函数
                            // 这里简单处理：直接调用写 CMD 逻辑，不等待下一次轮询
                            emit logMessage(QString("发送命令 CMD=%1...").arg(m_currentCmdType));
                            writeRegister(Addr::PC_CMD, (quint16)m_currentCmdType);
                            m_sendState = WaitingCmdAck;
                        });

                        // 注意：上面的 Timer 是为了打破递归调用栈，实际也可以直接写
                        return; // 退出本次回调
                    }
                }
                break;

            case WritingCmd:
                // 2. 数据写完，发送 CMD 命令 (如 51)
                emit logMessage(QString("发送命令 CMD=%1...").arg(m_currentCmdType));
                writeRegister(Addr::PC_CMD, (quint16)m_currentCmdType);
                m_sendState = WaitingCmdAck;
                break;

            case WaitingCmdAck:
                // 3. 等待机器人回复 (40014 == 1 或非0)
                // 图片说: "接收到启动命令=1"
                if (cmdResp == 1) {
                    emit logMessage("机器人已确认命令(Resp=1)，清除本地CMD...");
                    writeRegister(Addr::PC_CMD, 0); // 4. 清零 CMD
                    m_sendState = WaitingCmdReset;
                }
                break;

            case WaitingCmdReset:
                // 5. 等待机器人清零回复 (40014 == 0)
                if (cmdResp == 0) {
                    emit logMessage("握手完成，任务开始执行。");
                    emit jobSentSuccess();
                    m_sendState = Idle; // 发送流程结束
                }
                break;
            default: break;
            }

            // --- 状态机 2: 焊接完成监控握手 ---
            switch (m_compState) {
            case Monitoring:
                // 1. 监控 40015 是否为 1
                if (weldDone == 1) {
                    emit logMessage("收到机器人: 焊接完成信号 (Done=1)");
                    emit robotWeldCompleted(); // 通知 UI

                    // 2. PC 回复 Ack (40143 = 1)
                    writeRegister(Addr::PC_WELD_ACK, 1);
                    m_compState = WaitingRobotClear;
                }
                break;

            case WaitingRobotClear:
                // 3. 等待机器人清零 40015
                if (weldDone == 0) {
                    emit logMessage("机器人信号已复位，清除本地ACK...");
                    // 4. PC 清零 Ack (40143 = 0)
                    writeRegister(Addr::PC_WELD_ACK, 0);
                    m_compState = ClearingAck;
                }
                break;

            case ClearingAck:
                // 简单确认一下写回去了即可，回到监控状态
                m_compState = Monitoring;
                break;
            case SendingAck:
                break;
            }
        }
    }
    reply->deleteLater();
}

// ===================== 底层读写封装 =====================
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

// 浮点转换
QVector<quint16> ModbusManager::floatToRegisters(float val)
{
    QVector<quint16> regs;
    union { float f; uint32_t i; } u;
    u.f = val;
    regs.append((quint16)(u.i >> 16));   // High
    regs.append((quint16)(u.i & 0xFFFF)); // Low
    return regs;
}
