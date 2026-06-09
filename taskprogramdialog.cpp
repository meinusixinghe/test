#include "taskprogramdialog.h"
#include "EfortSdk.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QComboBox>
#include <QThread>
#include <QMetaObject>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPointer>
#include <QDebug>
#include <QMatrix4x4>
#include <QVector3D>

namespace {
constexpr int kSdkAuxAxisCount = 12;

struct BlockMoveReadyState
{
    bool servo = false;
    bool alarm = false;
    bool emg = false;
    bool moveState = false;
    bool moveState2 = false;
    RoboxKeyMode keyMode = ROBOX_MODE_MANUAL;
    RoboxProgramState proStatus = RPL_INIT;
    int servoRet = -1;
    int alarmRet = -1;
    int emgRet = -1;
    int keyModeRet = -1;
    int proStatusRet = -1;
    int moveStateRet = -1;
    int moveState2Ret = -1;
};

QString keyModeText(RoboxKeyMode mode)
{
    switch (mode) {
    case ROBOX_MODE_MANUAL: return "MANUAL";
    case ROBOX_MODE_MANUFAST: return "MANUFAST";
    case ROBOX_MODE_AUTO: return "AUTO";
    }
    return QString("UNKNOWN(%1)").arg(static_cast<int>(mode));
}

QString proStatusText(RoboxProgramState status)
{
    switch (status) {
    case RPL_INIT: return "INIT";
    case RPL_RUN: return "RUN";
    case RPL_PAUSE: return "PAUSE";
    case RPL_STOPPED: return "STOPPED";
    case RPL_ENDED: return "ENDED";
    case RPL_ERROR: return "ERROR";
    case RPL_INIT_AFTER_ERROR: return "INIT_AFTER_ERROR";
    default: break;
    }
    return QString("UNKNOWN(%1)").arg(static_cast<int>(status));
}

QString stepModeText(RoboxStartMode mode)
{
    switch (mode) {
    case ROBOX_MODE_CONTINUOUS: return "CONTINUOUS";
    case ROBOX_MODE_STEPIN: return "STEPIN";
    case ROBOX_MODE_STEPOVER: return "STEPOVER";
    case ROBOX_MODE_M_STEPIN: return "M_STEPIN";
    case ROBOX_MODE_M_STEPOVER: return "M_STEPOVER";
    case ROBOX_MODE_ANTIDROMIC: return "ANTIDROMIC";
    }
    return QString("UNKNOWN(%1)").arg(static_cast<int>(mode));
}

QString jogModeText(RoboxJogMode mode)
{
    switch (mode) {
    case ROBOX_MODE_JOG: return "JOG";
    case ROBOX_MODE_ROBOT: return "ROBOT";
    case ROBOX_MODE_TOOL: return "TOOL";
    case ROBOX_MODE_UFRAME: return "UFRAME";
    case ROBOX_MODE_AUX: return "AUX";
    }
    return QString("UNKNOWN(%1)").arg(static_cast<int>(mode));
}

BlockMoveReadyState readBlockMoveReadyState(unsigned int devId)
{
    BlockMoveReadyState state;
    state.servoRet = RobotAPI::GetCurrentServoStatus(state.servo, devId);
    state.alarmRet = RobotAPI::GetCurrentAlarmStatus(state.alarm, devId);
    state.emgRet = RobotAPI::GetCurrentEmgStatus(state.emg, devId);
    state.keyModeRet = RobotAPI::GetCurrentKeyMode(state.keyMode, devId);
    state.proStatusRet = RobotAPI::GetCurrentProStatus(state.proStatus, devId);
    state.moveStateRet = RobotAPI::GetMoveState(state.moveState, devId);
    state.moveState2Ret = RobotAPI::GetMoveState2(state.moveState2, devId);
    return state;
}

void logBlockMoveReadyState(const char* label, const BlockMoveReadyState& state, unsigned int devId)
{
    qInfo() << "[TaskProgram][Ready]" << label
            << "devId =" << devId
            << "servoRet =" << state.servoRet << "servo =" << state.servo
            << "alarmRet =" << state.alarmRet << "alarm =" << state.alarm
            << "emgRet =" << state.emgRet << "emg =" << state.emg
            << "keyModeRet =" << state.keyModeRet << "keyMode =" << keyModeText(state.keyMode)
            << "proStatusRet =" << state.proStatusRet << "proStatus =" << proStatusText(state.proStatus)
            << "moveStateRet =" << state.moveStateRet << "moveState =" << state.moveState
            << "moveState2Ret =" << state.moveState2Ret << "moveState2 =" << state.moveState2;
}

QString blockMoveReadyError(const BlockMoveReadyState& state)
{
    if (state.servoRet != 0 || state.alarmRet != 0 || state.emgRet != 0 || state.keyModeRet != 0) {
        return QString("启动前读取机器人状态失败: servoRet=%1 alarmRet=%2 emgRet=%3 keyModeRet=%4")
            .arg(state.servoRet).arg(state.alarmRet).arg(state.emgRet).arg(state.keyModeRet);
    }
    if (state.alarm) {
        return "机器人存在报警，请先清除报警后再启动 BlockMultiMove。";
    }
    if (state.emg) {
        return "机器人处于急停状态，请释放急停后再启动 BlockMultiMove。";
    }
    if (!state.servo) {
        return "机器人伺服未使能，请先上使能后再启动 BlockMultiMove。";
    }
    if (state.keyMode != ROBOX_MODE_AUTO) {
        return QString("机器人当前不是自动模式(%1)，请切到 AUTO 后再启动 BlockMultiMove。").arg(keyModeText(state.keyMode));
    }
    if (state.moveStateRet == 0 && state.moveState) {
        return "机器人当前已有运动在执行，请停止或等待完成后再启动 BlockMultiMove。";
    }
    if (state.moveState2Ret == 0 && state.moveState2) {
        return "机器人当前已有组合运动在执行，请停止或等待完成后再启动 BlockMultiMove。";
    }
    return {};
}

void logSdkReturn(const char* apiName, int ret, unsigned int devId)
{
    if (ret == 0) {
        qInfo() << "[TaskProgram][SDK]" << apiName << "ret =" << ret << "devId =" << devId;
    } else {
        qWarning() << "[TaskProgram][SDK]" << apiName << "ret =" << ret << "devId =" << devId;
    }
}

void logRunInfo(const char* label, unsigned int devId)
{
    RobotAPI::RunInfo info;
    memset(&info, 0, sizeof(info));
    const int ret = RobotAPI::GetRobotStatusData(info, devId);
    if (ret != 0) {
        qWarning() << "[TaskProgram][RunInfo]" << label
                   << "GetRobotStatusData ret =" << ret
                   << "devId =" << devId;
        return;
    }

    qInfo() << "[TaskProgram][RunInfo]" << label
            << "devId =" << devId
            << "connectMode =" << info.connectMode
            << "keyMode =" << keyModeText(info.keyMode)
            << "coordSys =" << jogModeText(info.coordSysID)
            << "tool =" << QString::fromLocal8Bit(info.toolName)
            << "wobj =" << QString::fromLocal8Bit(info.wobjName)
            << "velocity =" << info.velocity
            << "powerStatus =" << info.powerStatus
            << "robotRunMode =" << info.robotRunMode
            << "programStatus =" << proStatusText(info.programStatus)
            << "programNumber =" << info.programNumber
            << "programName =" << QString::fromLocal8Bit(info.programName)
            << "tcpSpeed =" << info.tcpSpeed;
}

int prepareBlockMultiMoveStart(unsigned int devId, QString& failStep)
{
    logRunInfo("prepare-enter", devId);

    int ret = RobotAPI::GetPermission(devId);
    logSdkReturn("GetPermission(worker)", ret, devId);
    if (ret != 0) {
        qWarning() << "[TaskProgram][Prepare] GetPermission returned non-zero, continue to start checks"
                   << "ret =" << ret
                   << "devId =" << devId;
    }

    ret = RobotAPI::EnableProExec(devId);
    logSdkReturn("EnableProExec(worker)", ret, devId);
    if (ret != 0) {
        failStep = "EnableProExec";
        return ret;
    }

    RobotAPI::RunInfo runInfo;
    memset(&runInfo, 0, sizeof(runInfo));
    ret = RobotAPI::GetRobotStatusData(runInfo, devId);
    if (ret == 0) {
        qInfo() << "[TaskProgram][Prepare] program before start"
                << "robotRunMode =" << runInfo.robotRunMode
                << "programStatus =" << proStatusText(runInfo.programStatus)
                << "programName =" << QString::fromLocal8Bit(runInfo.programName)
                << "devId =" << devId;
        if (!runInfo.robotRunMode || runInfo.programStatus != RPL_RUN) {
            const int startRet = RobotAPI::StartProgram(devId);
            logSdkReturn("StartProgram(worker)", startRet, devId);
            if (startRet != 0) {
                failStep = "StartProgram";
                return startRet;
            }

            for (int i = 0; i < 10; ++i) {
                QThread::msleep(100);
                memset(&runInfo, 0, sizeof(runInfo));
                const int statusRet = RobotAPI::GetRobotStatusData(runInfo, devId);
                if (statusRet == 0) {
                    qInfo() << "[TaskProgram][Prepare] program after StartProgram"
                            << "try =" << (i + 1)
                            << "robotRunMode =" << runInfo.robotRunMode
                            << "programStatus =" << proStatusText(runInfo.programStatus)
                            << "tcpSpeed =" << runInfo.tcpSpeed
                            << "devId =" << devId;
                    if (runInfo.robotRunMode || runInfo.programStatus == RPL_RUN) {
                        break;
                    }
                } else {
                    logSdkReturn("GetRobotStatusData(after StartProgram)", statusRet, devId);
                }
            }
        }
    } else {
        logSdkReturn("GetRobotStatusData(before StartProgram)", ret, devId);
    }

    RoboxStartMode stepMode = ROBOX_MODE_CONTINUOUS;
    ret = RobotAPI::GetCurrentStepMode(stepMode, devId);
    logSdkReturn("GetCurrentStepMode(worker)", ret, devId);
    if (ret == 0) {
        qInfo() << "[TaskProgram][Prepare] current step mode =" << stepModeText(stepMode)
                << "devId =" << devId;
    }
    if (ret != 0 || stepMode != ROBOX_MODE_CONTINUOUS) {
        ret = RobotAPI::SetCurrentStepMode(ROBOX_MODE_CONTINUOUS, devId);
        logSdkReturn("SetCurrentStepMode(CONTINUOUS)", ret, devId);
        if (ret != 0) {
            failStep = "SetCurrentStepMode(CONTINUOUS)";
            return ret;
        }
    }

    unsigned int speedRatio = 0;
    ret = RobotAPI::GetCurrentSpeedRatio(speedRatio, devId);
    logSdkReturn("GetCurrentSpeedRatio(worker)", ret, devId);
    if (ret == 0) {
        qInfo() << "[TaskProgram][Prepare] current speed ratio =" << speedRatio
                << "devId =" << devId;
        if (speedRatio == 0) {
            ret = RobotAPI::SetGlobalSpeed(10, devId);
            logSdkReturn("SetGlobalSpeed(10)", ret, devId);
            if (ret != 0) {
                failStep = "SetGlobalSpeed(10)";
                return ret;
            }
        }
    }

    logRunInfo("prepare-leave", devId);
    return 0;
}

void logRobotPos(const char* label, const RobotAPI::RobotPos& pos)
{
    qInfo() << "[TaskProgram][Pos]" << label
            << "x =" << pos.x << "y =" << pos.y << "z =" << pos.z
            << "a =" << pos.a << "b =" << pos.b << "c =" << pos.c
            << "cfg =" << pos.cfgx << pos.cfg1 << pos.cfg4 << pos.cfg6;
}

void fillCartPoint(RobotAPI::RobotPos& dst, const double p[6], const RobotAPI::RobotPos& currentPos)
{
    dst = currentPos;
    dst.x = p[0];
    dst.y = p[1];
    dst.z = p[2];
    dst.a = p[3];
    dst.b = p[4];
    dst.c = p[5];
}

void logMoveInfo(const RobotAPI::MultiMoveInfo2& mp, int index, int total)
{
    qInfo() << "[TaskProgram][BlockMultiMove] point" << (index + 1) << "/" << total
            << "moveType =" << mp.moveType
            << "posType =" << mp.posType
            << "speed =" << mp.speed
            << "acc =" << mp.acc
            << "dec =" << mp.dec
            << "jerk =" << mp.jerk
            << "overlap =" << mp.overlapping
            << "auxOverlap =" << mp.auxOverlapping
            << "flags =" << mp.flags;

    if (mp.posType == 1) {
        qInfo() << "[TaskProgram][BlockMultiMove]   ap[0] ="
                << mp.ap[0].j[0] << mp.ap[0].j[1] << mp.ap[0].j[2]
                << mp.ap[0].j[3] << mp.ap[0].j[4] << mp.ap[0].j[5];
        return;
    }

    const int cartCount = (mp.moveType == 3) ? 2 : ((mp.moveType == 4) ? 4 : 1);
    for (int i = 0; i < cartCount; ++i) {
        const RobotAPI::RobotPos& p = mp.cp[i];
        qInfo() << "[TaskProgram][BlockMultiMove]   cp[" << i << "] ="
                << p.x << p.y << p.z << p.a << p.b << p.c
                << "cfg =" << p.cfgx << p.cfg1 << p.cfg4 << p.cfg6;
    }
}
}

// ====================================================================
// 构造函数：解析线条序列并生成运动程序表格
// ====================================================================
TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, QWidget *parent)
    : QDialog(parent), m_devId(devId), m_paths(paths), m_ucs(ucs)
{
    setWindowTitle("任务程序运行控制台 (BlockMultiMove)");
    setMinimumSize(1100, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QGroupBox* tableGroup = new QGroupBox("运行轨迹程序", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    // ==========================================================
    // 【多坐标系选择 UI】
    // ==========================================================
    QHBoxLayout* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel("加工几何基准:", this));
    m_coordCombo = new QComboBox(this);
    m_coordCombo->addItem("默认基座坐标系 (图纸绝对坐标)", 0);
    if (m_ucs.valid) {
        m_coordCombo->addItem("当前用户坐标系 (UCS相对坐标)", 1);
        m_coordCombo->setCurrentIndex(1); // 优先选中 UCS
    }
    coordLayout->addWidget(m_coordCombo);

    coordLayout->addSpacing(20);
    coordLayout->addWidget(new QLabel("机器人 Tool:", this));
    m_robotToolCombo = new QComboBox(this);
    m_robotToolCombo->setEditable(true);
    coordLayout->addWidget(m_robotToolCombo);

    coordLayout->addSpacing(10);
    coordLayout->addWidget(new QLabel("机器人 Wobj:", this));
    m_robotUserCombo = new QComboBox(this);
    m_robotUserCombo->setEditable(true);
    coordLayout->addWidget(m_robotUserCombo);
    coordLayout->addStretch();
    tableLayout->addLayout(coordLayout);

    for (int i = 0; i <= 31; ++i) {
        m_robotToolCombo->addItem(QString("tool%1").arg(i));
        m_robotUserCombo->addItem(QString("wobj%1").arg(i));
    }

    // 只读取当前 Tool/Wobj；不调用列表接口，避免部分控制器在该接口上持续写日志。
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        try {
            std::string curTool, curWobj;
            if (RobotAPI::GetCurrentToolName(curTool, m_devId) == 0) {
                m_robotToolCombo->setCurrentText(QString::fromStdString(curTool));
            }
            if (RobotAPI::GetCurrentUframeName(curWobj, m_devId) == 0) {
                m_robotUserCombo->setCurrentText(QString::fromStdString(curWobj));
            }
        } catch (...) {}
    }

    // 增加第 13 列 -> 备注
    m_table = new QTableWidget(0, 13, this);
    m_table->setHorizontalHeaderLabels({"插补模式", "坐标类型", "X", "Y", "Z", "RX", "RY", "RZ", "速度", "加速", "减速", "平滑度", "备注说明"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLayout->addWidget(m_table);

    QHBoxLayout* editLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("➕ 添加空动作", this);
    QPushButton* removeBtn = new QPushButton("➖ 删除选中行", this);
    QPushButton* syncBtn = new QPushButton("📍 获取当前坐标为新动作", this);
    editLayout->addWidget(addBtn);
    editLayout->addWidget(removeBtn);
    editLayout->addWidget(syncBtn);
    editLayout->addStretch();
    tableLayout->addLayout(editLayout);
    mainLayout->addWidget(tableGroup);

    // 2. 状态与控制区 (保持不变)
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_robotStateLabel = new QLabel("底层状态: BlockMultiMove 空闲", this);
    m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
    bottomLayout->addWidget(m_robotStateLabel);

    bottomLayout->addSpacing(20);

    m_statusLabel = new QLabel("状态: 程序已生成，准备就绪...", this);
    m_statusLabel->setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;");
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

    m_startBtn = new QPushButton("▶ 启动程序", this);
    m_startBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_pauseBtn = new QPushButton("⏹ 停止运动", this);
    m_pauseBtn->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_resumeBtn = new QPushButton("恢复不可用", this);
    m_resumeBtn->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_resetBtn = new QPushButton("⏹ 重置/停止", this);
    m_resetBtn->setStyleSheet("background-color: #E53935; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");

    bottomLayout->addWidget(m_startBtn);
    bottomLayout->addWidget(m_pauseBtn);
    bottomLayout->addWidget(m_resumeBtn);
    bottomLayout->addWidget(m_resetBtn);
    mainLayout->addLayout(bottomLayout);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);

    connect(addBtn, &QPushButton::clicked, this, &TaskProgramDialog::onAddRowClicked);
    connect(removeBtn, &QPushButton::clicked, this, &TaskProgramDialog::onRemoveRowClicked);
    connect(syncBtn, &QPushButton::clicked, this, &TaskProgramDialog::onSyncPosClicked);

    connect(m_startBtn, &QPushButton::clicked, this, &TaskProgramDialog::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &TaskProgramDialog::onPauseClicked);
    connect(m_resumeBtn, &QPushButton::clicked, this, &TaskProgramDialog::onResumeClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &TaskProgramDialog::onResetClicked);

    connect(m_coordCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TaskProgramDialog::generateProgram);
    generateProgram();

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &TaskProgramDialog::updateRobotState);
    m_statusTimer->start(500);
}

void TaskProgramDialog::closeEvent(QCloseEvent* event)
{
    if (m_blockMoveRunning) {
        m_blockMoveStopRequested = true;
        m_statusLabel->setText("BlockMultiMove 正在运行，已发送停止请求...");
        QThread* stopper = QThread::create([devId = m_devId]() {
            const int ret = RobotAPI::BlockMultiMoveStop(devId);
            logSdkReturn("BlockMultiMoveStop(close)", ret, devId);
        });
        connect(stopper, &QThread::finished, stopper, &QObject::deleteLater);
        stopper->start();
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

// ----------------------------------------------------
// 辅助添加行 (带备注)
// ----------------------------------------------------
void TaskProgramDialog::addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap, const QString& remark) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    double defaultPos[6] = {0,0,0,0,0,0};
    if (!pos) pos = defaultPos;

    QComboBox* moveCombo = new QComboBox();
    moveCombo->addItems({"1: 关节(Joint)", "2: 直线(Lin)", "3: 圆弧(Circ)", "4: 圆角(CircAng)"});
    if (moveType >= 1 && moveType <= 4) moveCombo->setCurrentIndex(moveType - 1);
    m_table->setCellWidget(row, 0, moveCombo);

    QComboBox* posCombo = new QComboBox();
    posCombo->addItems({"1: Joint数据", "2: Cart数据"});
    if (posType == 1 || posType == 2) posCombo->setCurrentIndex(posType - 1);
    m_table->setCellWidget(row, 1, posCombo);

    for (int i = 0; i < 6; ++i) m_table->setItem(row, i + 2, new QTableWidgetItem(QString::number(pos[i], 'f', 3)));

    m_table->setItem(row, 8, new QTableWidgetItem(QString::number(speed, 'f', 1)));
    m_table->setItem(row, 9, new QTableWidgetItem(QString::number(acc, 'f', 1)));
    m_table->setItem(row, 10, new QTableWidgetItem(QString::number(dec, 'f', 1)));
    m_table->setItem(row, 11, new QTableWidgetItem(QString::number(overlap, 'f', 1)));

    // 添加并灰度化备注列
    QTableWidgetItem* remarkItem = new QTableWidgetItem(remark);
    remarkItem->setForeground(QBrush(QColor("#757575")));
    m_table->setItem(row, 12, remarkItem);
}

void TaskProgramDialog::onAddRowClicked() { addRow(2, 2, nullptr, 100, 50, 50, 0); }
void TaskProgramDialog::onRemoveRowClicked() {
    if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow());
}
void TaskProgramDialog::onSyncPosClicked() {
    if (m_devId == 0) return;
    RobotAPI::PosData pd;
    const int ret = RobotAPI::GetPositionData(pd, m_devId);
    logSdkReturn("GetPositionData(sync)", ret, m_devId);
    if (ret == 0) {
        double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
        addRow(2, 2, p, 100, 50, 50, 0);
        m_statusLabel->setText("当前位置已抓取为新动作！");
    } else {
        m_statusLabel->setText(QString("获取当前位置失败，错误码: %1").arg(ret));
    }
}

// ======================== 控制执行逻辑 ========================
struct PathPointData {
    int moveType;
    int posType;
    double p[6];
    double speed;
    double acc;
    double dec;
    double overlapping;
};

void TaskProgramDialog::onStartClicked() {
    int rowCount = m_table->rowCount();
    const bool connected = (m_devId != 0 && RobotAPI::IsConnected(m_devId));
    qInfo() << "[TaskProgram][Start] clicked"
            << "rowCount =" << rowCount
            << "devId =" << m_devId
            << "connected =" << connected;

    if (m_devId == 0 || rowCount == 0) {
        qWarning() << "[TaskProgram][Start] rejected: empty devId or empty program"
                   << "rowCount =" << rowCount
                   << "devId =" << m_devId;
        return;
    }

    if (!connected) {
        qWarning() << "[TaskProgram][Start] rejected: robot is not connected"
                   << "devId =" << m_devId;
        m_statusLabel->setText("机器人未连接，拒绝下发 BlockMultiMove。");
        return;
    }

    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    const std::string motionWobj = useUcs ? selWobj : std::string("wobj0");
    qInfo() << "[TaskProgram][Start]"
            << "tool =" << QString::fromStdString(selTool)
            << "selectedWobj =" << QString::fromStdString(selWobj)
            << "motionWobj =" << QString::fromStdString(motionWobj)
            << "useUcs =" << useUcs;

    int selectRet = RobotAPI::SelectRobot(m_devId);
    logSdkReturn("SelectRobot(start-ui)", selectRet, m_devId);
    if (selectRet != 0) {
        m_statusLabel->setText(QString("选择控制器失败，错误码: %1").arg(selectRet));
        return;
    }

    const bool apiControl = RobotAPI::IsApiControl(m_devId);
    qInfo() << "[TaskProgram][Start] IsApiControl =" << apiControl << "devId =" << m_devId;
    if (!apiControl) {
        const int apiRet = RobotAPI::EnableApiControl(true, m_devId);
        logSdkReturn("EnableApiControl(start-ui)", apiRet, m_devId);
        if (apiRet != 0) {
            m_statusLabel->setText(QString("外部 API 控制使能失败，错误码: %1").arg(apiRet));
            return;
        }
    }

    const BlockMoveReadyState readyState = readBlockMoveReadyState(m_devId);
    logBlockMoveReadyState("start-ui-before-build", readyState, m_devId);
    const QString readyError = blockMoveReadyError(readyState);
    if (!readyError.isEmpty()) {
        qWarning() << "[TaskProgram][Start] rejected:" << readyError;
        m_statusLabel->setText(readyError);
        return;
    }

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("正在底层换算坐标与轴配置...");

    // 提前拿到当前的真实物理配置
    RobotAPI::RobotPos currentPos;
    memset(&currentPos, 0, sizeof(currentPos));
    const int currentPosRet = useUcs ? RobotAPI::GetUserCoordinatePos2(currentPos, m_devId)
                                     : RobotAPI::GetBaseCoordinatePos2(currentPos, m_devId);
    logSdkReturn(useUcs ? "GetUserCoordinatePos2(start)" : "GetBaseCoordinatePos2(start)", currentPosRet, m_devId);
    if (currentPosRet != 0) {
        m_startBtn->setEnabled(true);
        m_statusLabel->setText(QString("读取当前坐标失败，错误码: %1").arg(currentPosRet));
        return;
    }
    logRobotPos(useUcs ? "current user coordinate" : "current base coordinate", currentPos);

    // ====================================================================
    // 💡 辅助工具：提取表格指定行的数据，并完成安全的逆解/正解
    // ====================================================================
    auto solveRow = [&](int row, RobotAPI::MultiMoveInfo2& targetMp, int arrayIndex) -> bool {
        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 1));
        int posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(row, i + 2)->text().toDouble();
        qInfo() << "[TaskProgram][SolveRow]"
                << "row =" << (row + 1)
                << "arrayIndex =" << arrayIndex
                << "posType =" << posType
                << "raw =" << p[0] << p[1] << p[2] << p[3] << p[4] << p[5];

        if (posType == 1) {
            for (int i = 0; i < kSdkAuxAxisCount; ++i) {
                targetMp.ap[arrayIndex].j[i] = 0;
            }
            for (int i = 0; i < 6; ++i) {
                targetMp.ap[arrayIndex].j[i] = p[i];
            }
            qInfo() << "[TaskProgram][SolveRow] joint row ok" << "row =" << (row + 1);
            return true;
        }

        if (posType == 2) {
            fillCartPoint(targetMp.cp[arrayIndex], p, currentPos);

            RobotAPI::RobotJoint tJoints; memset(&tJoints, 0, sizeof(tJoints));
            const int ikRet = RobotAPI::IkSolver(targetMp.cp[arrayIndex], tJoints, selTool, motionWobj, m_devId);
            logSdkReturn("IkSolver(solveRow)", ikRet, m_devId);
            if (ikRet == 0) {
                qInfo() << "[TaskProgram][IK] row =" << (row + 1)
                        << "joint =" << tJoints.j[0] << tJoints.j[1] << tJoints.j[2]
                        << tJoints.j[3] << tJoints.j[4] << tJoints.j[5]
                        << "wobj =" << QString::fromStdString(motionWobj);
                logRobotPos(useUcs ? "solveRow user cartesian" : "solveRow base cartesian", targetMp.cp[arrayIndex]);
                return true;
            }
            qWarning() << "[TaskProgram][SolveRow] failed"
                       << "row =" << (row + 1)
                       << "arrayIndex =" << arrayIndex
                       << "posType =" << posType
                       << "wobj =" << QString::fromStdString(motionWobj)
                       << "useUcs =" << useUcs;
            return false; // 解算失败
        }

        qWarning() << "[TaskProgram][SolveRow] unsupported posType"
                   << "row =" << (row + 1)
                   << "posType =" << posType;
        return false;
    };

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    // ====================================================================
    // 🚨 遍历表格：针对圆弧指令进行“合并打包”
    // ====================================================================
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        mp.speed = m_table->item(r, 8)->text().toDouble();
        mp.acc = m_table->item(r, 9)->text().toDouble();
        mp.dec = m_table->item(r, 10)->text().toDouble();
        mp.jerk = 0;
        mp.overlapping = m_table->item(r, 11)->text().toDouble();
        mp.auxOverlapping = 1e100;
        mp.flags = 0;
        qInfo() << "[TaskProgram][BuildMove]"
                << "sourceRow =" << (r + 1)
                << "moveType =" << mp.moveType
                << "posType =" << mp.posType
                << "speed =" << mp.speed
                << "acc =" << mp.acc
                << "dec =" << mp.dec
                << "overlap =" << mp.overlapping;

        // 🎯 核心逻辑：如果是圆弧，必须强行吞下两行数据！
        if (mp.moveType == 3) {
            if (r + 1 >= rowCount) {
                QMessageBox::warning(this, "轨迹错误", "发现不完整的圆弧指令（缺少终点）！");
                m_startBtn->setEnabled(true);
                return;
            }
            // 解决途经点放入 cp[0]
            if (!solveRow(r, mp, 0)) {
                QMessageBox::warning(this, "报错", QString("第 %1 行圆弧途经点逆解不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
            // 解决终点放入 cp[1]
            if (!solveRow(r + 1, mp, 1)) {
                QMessageBox::warning(this, "报错", QString("第 %1 行圆弧终点逆解不可达！").arg(r+2));
                m_startBtn->setEnabled(true); return;
            }

            // 重要：标记当前已使用两行，避免下次循环重复解析
            r++;

        } else {
            // 普通直线或关节动作，只填 cp[0]
            if (!solveRow(r, mp, 0)) {
                QMessageBox::warning(this, "报错", QString("第 %1 行直线点位不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
        }

        mps.push_back(mp);
    }

    if (mps.empty()) {
        qWarning() << "[TaskProgram][Start] rejected: generated BlockMultiMove list is empty";
        m_startBtn->setEnabled(true);
        m_statusLabel->setText("运动列表为空，未下发 BlockMultiMove。");
        return;
    }

    qInfo() << "[TaskProgram][BlockMultiMove] generated list size =" << static_cast<int>(mps.size());
    for (int i = 0; i < static_cast<int>(mps.size()); ++i) {
        logMoveInfo(mps[i], i, static_cast<int>(mps.size()));
    }

    setBlockMoveRunning(true);
    m_blockMoveStopRequested = false;
    m_resetAfterBlockStop = false;
    m_statusLabel->setText(QString("BlockMultiMove 运行中，共 %1 条运动指令...").arg(mps.size()));

    m_blockMoveThread = QThread::create([this, mps, devId = m_devId, selTool, motionWobj]() mutable {
        qInfo() << "[TaskProgram][BlockMultiMove] worker start"
                << "devId =" << devId
                << "listSize =" << static_cast<int>(mps.size())
                << "tool =" << QString::fromStdString(selTool)
                << "wobj =" << QString::fromStdString(motionWobj);

        QString failStep;
        int ret = RobotAPI::SelectRobot(devId);
        logSdkReturn("SelectRobot(worker)", ret, devId);
        if (ret != 0) {
            failStep = "SelectRobot";
        }

        if (ret == 0) {
            ret = RobotAPI::BlockMultiMoveReset(devId);
            logSdkReturn("BlockMultiMoveReset(worker-before)", ret, devId);
            if (ret != 0) {
                failStep = "BlockMultiMoveReset";
            }
        }

        QThread::msleep(20);

        if (ret == 0 && !selTool.empty()) {
            ret = RobotAPI::SetCurrentToolByName(selTool, devId);
            logSdkReturn("SetCurrentToolByName(worker)", ret, devId);
            if (ret != 0) {
                failStep = "SetCurrentToolByName";
            }
        }

        if (ret == 0 && !motionWobj.empty()) {
            ret = RobotAPI::SetCurrentUframeByName(motionWobj, devId);
            logSdkReturn("SetCurrentUframeByName(worker)", ret, devId);
            if (ret != 0) {
                failStep = "SetCurrentUframeByName";
            }
        }

        QThread::msleep(50);

        if (ret == 0) {
            ret = prepareBlockMultiMoveStart(devId, failStep);
        }

        QThread::msleep(50);

        if (ret == 0) {
            const BlockMoveReadyState readyState = readBlockMoveReadyState(devId);
            logBlockMoveReadyState("worker-before-call", readyState, devId);
            const QString readyError = blockMoveReadyError(readyState);
            if (!readyError.isEmpty()) {
                failStep = readyError;
                ret = ERROR_CUR_MODE_NOT_SUPPORT;
                qWarning() << "[TaskProgram][BlockMultiMove] skipped because robot is not ready"
                           << readyError
                           << "devId =" << devId;
            }
        }

        if (ret == 0) {
            qInfo() << "[TaskProgram][BlockMultiMove] call BlockMultiMove";
            ret = RobotAPI::BlockMultiMove(mps, devId);
            logSdkReturn("BlockMultiMove(worker)", ret, devId);
            logRunInfo("worker-after-call", devId);
        } else {
            qWarning() << "[TaskProgram][BlockMultiMove] skipped because prepare step failed"
                       << "step =" << failStep
                       << "ret =" << ret
                       << "devId =" << devId;
        }

        QMetaObject::invokeMethod(this, [this, ret, failStep]() {
            const bool stopped = m_blockMoveStopRequested;
            const bool resetAfterStop = m_resetAfterBlockStop;

            if (resetAfterStop && m_devId != 0) {
                const int resetRet = RobotAPI::BlockMultiMoveReset(m_devId);
                logSdkReturn("BlockMultiMoveReset(ui-after-stop)", resetRet, m_devId);
            }

            setBlockMoveRunning(false);
            m_blockMoveThread = nullptr;
            m_blockMoveStopRequested = false;
            m_resetAfterBlockStop = false;

            if (stopped) {
                m_statusLabel->setText(ret == 0 ? "BlockMultiMove 已停止。" : QString("BlockMultiMove 已停止，返回码: %1").arg(ret));
            } else if (!failStep.isEmpty()) {
                m_statusLabel->setText(QString("BlockMultiMove 准备阶段失败: %1，错误码: %2").arg(failStep).arg(ret));
            } else if (ret == 0) {
                m_statusLabel->setText("BlockMultiMove 执行完成。");
            } else {
                m_statusLabel->setText(QString("BlockMultiMove 结束，错误码: %1").arg(ret));
            }
        }, Qt::QueuedConnection);
    });

    connect(m_blockMoveThread, &QThread::finished, m_blockMoveThread, &QObject::deleteLater);
    m_blockMoveThread->start();
}

void TaskProgramDialog::onPauseClicked() {
    if (m_devId == 0) {
        qWarning() << "[TaskProgram][Stop] rejected: devId is 0";
        return;
    }
    if (!m_blockMoveRunning) {
        qInfo() << "[TaskProgram][Stop] ignored: BlockMultiMove is not running" << "devId =" << m_devId;
        return;
    }
    m_blockMoveStopRequested = true;
    m_statusLabel->setText("正在停止 BlockMultiMove ...");
    QPointer<TaskProgramDialog> self(this);
    QThread* stopper = QThread::create([self, devId = m_devId]() {
        const int selectRet = RobotAPI::SelectRobot(devId);
        logSdkReturn("SelectRobot(stop)", selectRet, devId);
        const int ret = RobotAPI::BlockMultiMoveStop(devId);
        logSdkReturn("BlockMultiMoveStop(stop)", ret, devId);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, selectRet, ret](){
            if (!self) return;
            if (selectRet != 0) {
                self->m_statusLabel->setText(QString("停止前选择控制器失败，错误码: %1").arg(selectRet));
            } else if (ret != 0) {
                self->m_statusLabel->setText(QString("停止 BlockMultiMove 失败，错误码: %1").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(stopper, &QThread::finished, stopper, &QObject::deleteLater);
    stopper->start();
}

void TaskProgramDialog::onResumeClicked() {
    m_statusLabel->setText("BlockMultiMove 不支持暂停后恢复，请重新启动程序。");
}

void TaskProgramDialog::onResetClicked() {
    if (m_devId == 0) {
        qWarning() << "[TaskProgram][Reset] rejected: devId is 0";
        return;
    }
    if (m_blockMoveRunning) {
        m_blockMoveStopRequested = true;
        m_resetAfterBlockStop = true;
        m_statusLabel->setText("正在停止并重置 BlockMultiMove ...");
        QPointer<TaskProgramDialog> self(this);
        QThread* stopper = QThread::create([self, devId = m_devId]() {
            const int selectRet = RobotAPI::SelectRobot(devId);
            logSdkReturn("SelectRobot(reset-stop)", selectRet, devId);
            const int ret = RobotAPI::BlockMultiMoveStop(devId);
            logSdkReturn("BlockMultiMoveStop(reset-stop)", ret, devId);
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, selectRet, ret](){
                if (!self) return;
                if (selectRet != 0) {
                    self->m_statusLabel->setText(QString("重置前选择控制器失败，错误码: %1").arg(selectRet));
                } else if (ret != 0) {
                    self->m_statusLabel->setText(QString("停止 BlockMultiMove 失败，错误码: %1").arg(ret));
                }
            }, Qt::QueuedConnection);
        });
        connect(stopper, &QThread::finished, stopper, &QObject::deleteLater);
        stopper->start();
        return;
    }

    QPointer<TaskProgramDialog> self(this);
    QThread* resetter = QThread::create([self, devId = m_devId]() {
        const int selectRet = RobotAPI::SelectRobot(devId);
        logSdkReturn("SelectRobot(reset)", selectRet, devId);
        const int ret = (selectRet == 0) ? RobotAPI::BlockMultiMoveReset(devId) : selectRet;
        if (selectRet == 0) {
            logSdkReturn("BlockMultiMoveReset(reset)", ret, devId);
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, selectRet, ret](){
            if (!self) return;
            if (selectRet != 0) {
                self->m_statusLabel->setText(QString("重置前选择控制器失败，错误码: %1").arg(selectRet));
            } else if (ret == 0) {
                self->m_statusLabel->setText("BlockMultiMove 已重置。");
            } else {
                self->m_statusLabel->setText(QString("BlockMultiMove 重置失败，错误码: %1").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(resetter, &QThread::finished, resetter, &QObject::deleteLater);
    resetter->start();
}

void TaskProgramDialog::generateProgram()
{
    // 1. 清空旧的表格数据
    m_table->setRowCount(0);

    // 2. 判断当前是否选择了“用户坐标系 (UCS)”
    // 如果下拉框选中第1项（且其 Data 值为 1），说明启用了 UCS
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);

    RobotAPI::RobotPos currentPose;
    currentPose.a = 0.0;
    currentPose.b = 0.0;
    currentPose.c = 0.0;

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        const int poseRet = useUcs ? RobotAPI::GetUserCoordinatePos(currentPose, m_devId)
                                   : RobotAPI::GetBaseCoordinatePos(currentPose, m_devId);
        logSdkReturn(useUcs ? "GetUserCoordinatePos(generate)" : "GetBaseCoordinatePos(generate)", poseRet, m_devId);
        if (poseRet == 0) {
            logRobotPos(useUcs ? "generate current user pose" : "generate current base pose", currentPose);
        }
    }

    // 记录上一个图形最后一个点的真实物理位置，用于判定是否“无缝衔接”
    QPointF lastEndPos(-99999.0, -99999.0);

    // ==========================================
    // 开始遍历所有导入的图元并生成轨迹
    // ==========================================
    for (int idx = 0; idx < m_paths.size(); ++idx) {
        const Contour& c = m_paths[idx];
        if (c.points.isEmpty()) continue;

        QString typeStr = c.type;
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(typeStr);

        // 临时存放当前图元解析出来的点位、插补类型(2:直线, 3:圆弧)和备注
        QVector<QPointF> targetPoints;
        QVector<int> targetMoveTypes;
        QVector<QString> targetRemarks;

        int n = c.points.size();
        bool isFittedData = typeStr.contains("拟合") || typeStr.contains("样条") || typeStr.contains("Spline", Qt::CaseInsensitive);
        bool isCircle = typeStr.contains("圆") && !typeStr.contains("弧") && !typeStr.contains("角");
        bool isArc = typeStr.contains("弧") || typeStr.contains("Arc", Qt::CaseInsensitive);

        // --- A. 整圆处理：拆分为两个半圆弧 ---
        if (isCircle && n >= 4) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-起点(圆弧1开始)";
            targetPoints << c.points[n / 4];     targetMoveTypes << 3; targetRemarks << "-圆弧1途经点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-圆弧1终点(圆弧2开始)";
            targetPoints << c.points[3 * n / 4]; targetMoveTypes << 3; targetRemarks << "-圆弧2途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-圆弧2终点";
        }
        // --- B. 样条曲线/拟合线处理：三点共圆判定 ---
        else if (isFittedData && n >= 3) {
            targetPoints << c.points[0]; targetMoveTypes << 2; targetRemarks << "-样条起点";
            for (int i = 1; i < n - 1; i += 2) {
                int segIdx = (i + 1) / 2;
                QPointF p1 = c.points[i-1];
                QPointF p2 = c.points[i];
                QPointF p3 = c.points[i+1];

                // 行列式判断三点是否共线 (极小曲率防呆)
                double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));
                if (std::abs(D) < 1e-6) {
                    targetPoints << p3; targetMoveTypes << 2; targetRemarks << QString("-段%1[直线] 终点").arg(segIdx);
                } else {
                    targetPoints << p2; targetMoveTypes << 3; targetRemarks << QString("-段%1[圆弧] 途经点").arg(segIdx);
                    targetPoints << p3; targetMoveTypes << 3; targetRemarks << QString("-段%1[圆弧] 终点").arg(segIdx);
                }
            }
            if (n % 2 == 0) { targetPoints << c.points[n - 1]; targetMoveTypes << 2; targetRemarks << "-尾部收尾"; }
        }
        // --- C. 单一圆弧处理 ---
        else if (isArc && n >= 3) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-圆弧起点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-圆弧途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-圆弧终点";
        }
        // --- D. 普通多段线/直线处理 ---
        else {
            for (int i = 0; i < n; ++i) {
                targetPoints << c.points[i]; targetMoveTypes << 2;
                if (i == 0) targetRemarks << "-起点";
                else if (i == n - 1) targetRemarks << "-终点";
                else targetRemarks << QString("-途经点%1").arg(i);
            }
        }

        // --- 缝合计算：前瞻下一个图元的起点 ---
        // 判断当前图元的尾巴是否和下一个图元的头连在一起
        bool isConnectedWithNext = false;
        if (idx + 1 < m_paths.size() && !m_paths[idx + 1].points.isEmpty()) {
            QPointF nextStart = m_paths[idx + 1].points.first();
            QPointF myEnd = targetPoints.last();
            if (std::hypot(myEnd.x() - nextStart.x(), myEnd.y() - nextStart.y()) < 0.001) {
                isConnectedWithNext = true;
            }
        }

        // ==========================================
        // 将提取的点位写入表格，并执行 UCS 坐标变换
        // ==========================================
        for (int i = 0; i < targetPoints.size(); ++i) {
            QPointF pt = targetPoints[i];
            int moveType = targetMoveTypes[i];
            QString baseRemark = shapeName + targetRemarks[i];

            // 拦截：如果当前图形的起点和上一个图形终点无缝重合，直接丢弃该重叠点，防止机器人卡顿
            bool isConnectedWithPrev = (std::hypot(pt.x() - lastEndPos.x(), pt.y() - lastEndPos.y()) < 0.001);
            if (i == 0 && isConnectedWithPrev) continue;

            // 如果启用了 UCS，需要把 DXF 图纸的绝对坐标，投影到自定义坐标系的新坐标轴上
            if (useUcs && m_ucs.valid) {
                QPointF v = pt - m_ucs.origin; // 计算点相对于 UCS 原点的向量
                // 使用点乘 (Dot Product) 投影到新的 X 轴和 Y 轴向量上
                double local_x = v.x() * m_ucs.xAxis.x() + v.y() * m_ucs.xAxis.y();
                double local_y = v.x() * m_ucs.yAxis.x() + v.y() * m_ucs.yAxis.y();
                pt = QPointF(local_x, local_y); // 覆盖为相对坐标
            }

            // 构造 6 自由度数组：[X, Y, Z, RX, RY, RZ]
            double p[6] = { pt.x(), pt.y(), 0.0, currentPose.a, currentPose.b, currentPose.c };

            // 动态调节速度与平滑度 (Overlapping)
            double overlap = 0.0;
            double speed = 50.0;

            if (i == 0) {
                speed = 100.0; // 空走跳转时速度翻倍
                overlap = 0.0;
                baseRemark += "(空走跳转)";
            }
            else if (i == targetPoints.size() - 1) {
                if (isConnectedWithNext) {
                    overlap = 2.0; // 如果与下一个零件无缝衔接，不减速直接划过
                    baseRemark += "(无缝衔接下个)";
                } else {
                    overlap = 0.0; // 独立图形结束，必须精准停住
                    baseRemark += "(加工结束抬刀)";
                }
            } else {
                overlap = 2.0; // 途经点开启平滑度，防止机械臂剧烈抖动
            }

            // 将计算好的这一行数据写入 UI 表格
            // 参数顺序: 插补类型(2/3), 坐标类型(2=Cart), 坐标数组, 速度, 加速, 减速, 平滑度, 备注
            addRow(moveType, 2, p, speed, 50, 50, overlap, baseRemark);

            // 更新最后位置留存（注意：留存的用于判断连贯性的永远是变换前的真实物理坐标）
            lastEndPos = targetPoints[i];
        }
    }
}

void TaskProgramDialog::updateRobotState()
{
    if (m_devId == 0 || !RobotAPI::IsConnected(m_devId)) {
        m_robotStateLabel->setText("底层状态: 未连接");
        m_robotStateLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
        return;
    }

    if (m_blockMoveRunning) {
        ++m_blockMovePollCount;
        if (m_blockMovePollCount == 1 || (m_blockMovePollCount % 20) == 0) {
            qInfo() << "[TaskProgram][BlockMultiMove][Poll] skip SDK polling while BlockMultiMove is blocking"
                    << "stopRequested =" << m_blockMoveStopRequested
                    << "pollCount =" << m_blockMovePollCount
                    << "devId =" << m_devId;
        }
        m_robotStateLabel->setText(m_blockMoveStopRequested ? "底层状态: BlockMultiMove 停止中" : "底层状态: BlockMultiMove 运行中");
        m_robotStateLabel->setStyleSheet("font-weight: bold; color: green; font-size: 14px;");
    } else {
        m_blockMovePollCount = 0;
        m_robotStateLabel->setText("底层状态: BlockMultiMove 空闲");
        m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
    }
}

void TaskProgramDialog::setBlockMoveRunning(bool running)
{
    m_blockMoveRunning = running;
    m_blockMovePollCount = 0;
    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_resumeBtn->setEnabled(false);
    m_resetBtn->setEnabled(true);
    updateRobotState();
}
