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

        // 断开连接时，复位伺服状态变量
        if (m_lastServoState) {
            m_lastServoState = false;
            emit servoStateChanged(false);
        }

        // 断开连接时，复位自动状态变量
        if (m_isAutoMode) {
            m_isAutoMode = false;
            emit autoStateChanged(false);
        }

        emit startButtonTextChanged("预约");
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

        if (unit.startAddress() == 0 && unit.valueCount() >= 15) {
            quint16 status1    = unit.value(Addr::ROBOT_STATUS);       // 40001
            quint16 progLoaded = unit.value(Addr::ROBOT_PROG_LOAD);    // 40012
            quint16 cmdResp    = unit.value(Addr::ROBOT_CMD_RESP);     // 40014
            quint16 weldDone   = unit.value(Addr::ROBOT_WELD_DONE);    // 40015

            // 解析 40001 状态位
            bool isAuto      = (status1 & (1 << 1));
            bool servoStatus = (status1 & (1 << 3));
            bool alarm       = (status1 & (1 << 4));
            bool estop       = (status1 & (1 << 5));
            bool progRunning = (status1 & (1 << 6));
            bool progLoadDone  = (status1 & (1 << 11));
            m_isReserved = (status1 & (1 << 13));

            // 全局安全拦截机制 (急停/报警)
            if (alarm || estop) {
                if (!m_isAlarmActive) {
                    m_isAlarmActive = true;

                    // 1. 强制清空所有的任务状态机，彻底打断握手
                    m_startupState = StartIdle;
                    m_jobState = JobIdle;

                    emit startButtonTextChanged("预约");
                    // 2. 触发 UI 弹窗和底栏日志
                    QString msg = estop ? "触发物理急停！" : "机器人系统报警！";
                    emit logMessage("=== 严重安全事件: " + msg + " 所有任务已强制中止 ===");
                    emit errorOccurred("系统已强制停止。\n原因: " + msg);
                }

                // 3. 拦截执行：只要处于报警状态，直接 return，禁止往下走任何业务逻辑！
                return;
            } else {
                // 如果物理急停被旋开，或者报警被复位清除了
                if (m_isAlarmActive) {
                    m_isAlarmActive = false;
                    emit logMessage("报警/急停已解除，系统恢复安全待机状态。");
                }
            }

            // 发送伺服状态变化信号给主界面
            if (servoStatus != m_lastServoState) {
                m_lastServoState = servoStatus;
                emit servoStateChanged(servoStatus);
            }

            // 发送自动模式变化信号给主界面
            if (isAuto != m_isAutoMode) {
                m_isAutoMode = isAuto;
                emit autoStateChanged(isAuto);
            }

            // 手动模式拦截机制
            if (!m_isAutoMode) {
                // 如果在运行过程中突然被切回了手动模式，立刻打断所有任务
                if (m_startupState != StartIdle || m_jobState != JobIdle) {
                    m_startupState = StartIdle;
                    m_jobState = JobIdle;
                    emit startButtonTextChanged("预约");
                    emit logMessage("=== 机器人已切换至手动模式，当前任务已强制中止 ===");
                }
                // 只要是手动模式，状态机绝对不往下走
                return;
            }

            // 步骤 1 & 2: 启动状态机 (系统就绪与主程序加载)
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
                if (servoStatus) {
                    if (!progRunning) {
                        if (!m_isReserved) {
                            emit logMessage("步骤2: 伺服就绪，交由心跳节拍器发送 0->1->0 脉冲...");

                            // 核心改变：不暂停心跳！不加定时器！直接把状态交给下一个心跳！
                            m_startupState = PulseReserve_PullLow;
                        } else {
                            emit logMessage("步骤2: 探测到机器人已处于预约状态。");
                            m_startupState = WaitReserveReady;
                        }
                    } else {
                        emit logMessage("程序已经在运行中。");
                        m_startupState = StartIdle;
                    }
                }
                break;

            case PulseReserve_PullLow:
                emit logMessage(">> 节拍 1/3: 强制拉低 (发 0)...");
                m_ctrlWord129 &= ~(1 << ControlBits::RESERVE_ENABLE);
                writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);
                m_startupState = PulseReserve_PullHigh; // 200ms后的下一次心跳，会自动执行下一步！
                break;

            case PulseReserve_PullHigh:
                emit logMessage(">> 节拍 2/3: 制造上升沿 (发 1)...");
                m_ctrlWord129 |= (1 << ControlBits::RESERVE_ENABLE);
                writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);
                m_startupState = PulseReserve_CleanLow;
                break;

            case PulseReserve_CleanLow:
                emit logMessage(">> 节拍 3/3: 恢复低电平 (发 0)，死等预约灯亮...");
                m_ctrlWord129 &= ~(1 << ControlBits::RESERVE_ENABLE);
                writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);
                m_startupState = WaitReserveReady;
                break;

            case WaitReserveReady:
                if (m_isReserved) {
                    emit logMessage(QString("步骤2: 预约就绪！写入程序号 %1").arg(m_targetProgram));
                    writeRegister(Addr::PC_RESERVE_PROG, m_targetProgram);
                    m_startupState = Confirm_PullHigh;
                }
                break;

            case Confirm_PullHigh:
                emit logMessage(">> 确认脉冲 1/2: 触发确认预约 (发 1)...");
                m_ctrlWord129 |= (1 << ControlBits::RESERVE_CONFIRM);
                writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);
                m_startupState = Confirm_CleanLow;
                break;

            case Confirm_CleanLow:
                emit logMessage(">> 确认脉冲 2/2: 恢复确认位 (发 0)，等待运行...");
                m_ctrlWord129 &= ~(1 << ControlBits::RESERVE_CONFIRM);
                writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

                m_startupState = WaitProgramRunning;
                break;

            // case WaitProgramLoaded:
            //     if (progLoadDone) {
            //         emit logMessage(QString("程序 %1 已加载，发启动脉冲(40129.Bit1)").arg(m_targetProgram));
            //         pulseControlWord129(ControlBits::RUN_PULSE, 300);
            //         m_startupState = WaitProgramRunning;
            //     }
            //     break;

            case WaitProgramRunning:
                if (progRunning) {
                    emit logMessage("步骤2完成：机器人主程序空跑就绪。");
                    emit startButtonTextChanged("启动"); // UI 变化
                    m_startupState = StartIdle; // 启动流程结束
                }
                break;

            default: break;
            }

            // 步骤 3 & 4: 业务握手状态机
            switch (m_jobState) {
            case WaitingCmdAck:
                // 收到 40014=1，将 CMD 40142 清零
                if (cmdResp == 1) {
                    emit logMessage("步骤3: 机器人收到命令(40014=1)，上位机清零命令(40142=0)...");
                    writeRegister(Addr::PC_CMD, 0);
                    m_jobState = WaitingCmdClear;
                }
                break;

            case WaitingCmdClear:
                if (cmdResp == 0) {
                    emit logMessage("步骤3结束: CMD握手完成(40014=0)，机器人开始移动并焊接...");
                    m_jobState = WaitingJobDone;
                }
                break;

            case WaitingJobDone:
                // 收到焊接完成 40015=1，回复响应 40143=1
                if (weldDone == 1) {
                    emit logMessage("步骤4: 收到焊接完成(40015=1)，回复响应置位(40143=1)...");
                    emit robotWeldCompleted();
                    writeRegister(Addr::PC_WELD_ACK, 1);
                    m_jobState = AckJobDone_CleanLow;
                }
                break;

            case AckJobDone_CleanLow:
                // 继续执行下一个前，将响应信号清零 40143=0
                emit logMessage("步骤4.1: 将响应清零(40143=0)，等待机器人复位完成信号(40015=0)...");
                writeRegister(Addr::PC_WELD_ACK, 0);
                m_jobState = WaitingRobotClearDone;
                break;

            case WaitingRobotClearDone:
                // 等待机器人清零 40015=0，完成握手！
                if (weldDone == 0) {
                    emit logMessage("步骤4结束: 机器人已清零(40015=0)，单孔焊接流程完美闭环！");
                    emit jobSentSuccess(); // 发送成功信号，去触发下一个孔！
                    m_jobState = JobIdle;
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

    // 检查是否为自动模式
    if (!m_isAutoMode) {
        emit errorOccurred("机器人处于手动模式，请在控制柜上切换至自动模式后再启动！");
        return;
    }

    // 报警状态下禁止启动
    if (m_isAlarmActive) {
        emit errorOccurred("系统处于报警或急停状态，请先解除报警！");
        return;
    }

    // 静默拦截重复点击
    if (m_startupState != StartIdle) return;
    emit logMessage("开始系统就绪与启动流程");
    m_startupState = CheckSafety; // 推入状态机
}

// ==========================================================
// 外部调用：执行步骤3
// ==========================================================
void ModbusManager::executeCommand(RobotCmd cmd, const WeldingData *data)
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) return;

    // 报警状态下禁止下发任务
    if (m_isAlarmActive) {
        emit errorOccurred("危险！急停/报警中，禁止下发移动指令！");
        return;
    }

    // 检查是否为自动模式
    if (!m_isAutoMode) {
        emit errorOccurred("机器人处于手动模式，禁止下发任务！");
        return;
    }

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
// 报警复位 (发送 40129.Bit3 脉冲)
// ==========================================================
void ModbusManager::resetAlarm()
{
    if (m_modbus->state() == QModbusDevice::ConnectedState) {
        emit logMessage("下发系统报警复位脉冲 (40129.Bit3)");
        pulseControlWord129(ControlBits::ALARM_RESET, 300); // 300ms 脉冲
    }
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

// ==========================================================
// 暂停/继续 控制逻辑
// ==========================================================
void ModbusManager::setPause(bool paused)
{
    if (m_modbus->state() == QModbusDevice::ConnectedState) {
        if (paused) {
            emit logMessage("下发暂停脉冲 (40129.Bit2)");
            pulseControlWord129(ControlBits::PAUSE_PULSE, 300);
        } else {
            emit logMessage("下发恢复运行脉冲 (触发确认预约 40129.Bit9)");
            pulseControlWord129(ControlBits::RESERVE_CONFIRM, 300);
        }
    }
}

// ==========================================================
// 触发安全关闭
// ==========================================================
void ModbusManager::safeShutdown()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState) {
        emit shutdownFinished(); // 没连上直接允许退出
        return;
    }

    emit logMessage("执行安全退出：启动步进序列...");

    // 停止日常轮询，独占网络
    if (m_monitorTimer->isActive()) {
        m_monitorTimer->stop();
    }

    // 初始化关机步进器
    m_shutdownStep = 0;
    if (!m_shutdownTimer) {
        m_shutdownTimer = new QTimer(this);
        m_shutdownTimer->setSingleShot(true); // 设为单次触发
        // 定时器时间一到，就自动执行下一步
        connect(m_shutdownTimer, &QTimer::timeout, this, &ModbusManager::processShutdownStep);
    }

    // 立刻执行第 0 步
    processShutdownStep();
}

// ==========================================================
// 扁平化的关机步进状态机
// ==========================================================
void ModbusManager::processShutdownStep()
{
    switch (m_shutdownStep) {
    case 0:
        // 第 1 步：发暂停 (Bit 2)
        m_ctrlWord129 |= (1 << ControlBits::PAUSE_PULSE);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        m_shutdownStep++;
        m_shutdownTimer->start(200); // 准备 200ms 后执行下一步
        break;

    case 1:
        // 第 2 步：发复位 (Bit 3) 打断程序
        m_ctrlWord129 &= ~(1 << ControlBits::PAUSE_PULSE);
        m_ctrlWord129 |= (1 << ControlBits::ALARM_RESET);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        m_shutdownStep++;
        m_shutdownTimer->start(400); // 必须等程序死透
        break;

    case 2:
        // 第 3 步：松开复位，洗白系统
        m_ctrlWord129 &= ~(1 << ControlBits::ALARM_RESET);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        if (m_isReserved) {
            m_shutdownStep++;
            m_shutdownTimer->start(300); // 开启预约处理
        } else {
            // 如果根本没开启预约，直接跳到最后一步结束
            m_shutdownStep = 6;
            processShutdownStep();
        }
        break;

    case 3:
        // 第 4 步：关预约 -> 先拉低 (0)
        m_ctrlWord129 &= ~(1 << ControlBits::RESERVE_ENABLE);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        m_shutdownStep++;
        m_shutdownTimer->start(300);
        break;

    case 4:
        // 第 5 步：关预约 -> 再拉高 (1) 制造上升沿
        m_ctrlWord129 |= (1 << ControlBits::RESERVE_ENABLE);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        m_shutdownStep++;
        m_shutdownTimer->start(300);
        break;

    case 5:
        // 第 6 步：关预约 -> 彻底拉低 (0) 洗白状态
        m_ctrlWord129 &= ~(1 << ControlBits::RESERVE_ENABLE);
        writeRegister(Addr::PC_CONTROL_WORD, m_ctrlWord129);

        m_shutdownStep++;
        m_shutdownTimer->start(300); // 给网卡 50ms 发送最后一条指令
        break;

    case 6:
        // 全部结束！发送信号通知主界面
        emit logMessage("安全关机序列执行完毕，断开连接。");
        emit shutdownFinished();
        break;

    default: break;
    }
}
