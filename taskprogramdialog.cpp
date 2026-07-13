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
#include <QMatrix4x4>
#include <QVector3D>
#include <QtConcurrentRun>
#include <QApplication>
#include <QEvent>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
constexpr double kMaxContinuousJ6Delta = 175.0;
constexpr double kHardWristSingularityDeg = 1.0;
constexpr double kSoftWristSingularityDeg = 8.0;
constexpr int kMinMultiTurnCfg = -8;
constexpr int kMaxMultiTurnCfg = 7;

double normalizeAngle180(double angle)
{
    while (angle > 180.0) angle -= 360.0;
    while (angle <= -180.0) angle += 360.0;
    return angle;
}

int calcMultiTurnCfg(double angle)
{
    if (angle > 180.0) return static_cast<int>(std::ceil((angle - 180.0) / 360.0));
    if (angle <= -180.0) return static_cast<int>(std::floor((angle + 180.0) / 360.0));
    return 0;
}

int clampMultiTurnCfg(int cfg)
{
    return std::clamp(cfg, kMinMultiTurnCfg, kMaxMultiTurnCfg);
}

bool isCfgInRange(int cfg)
{
    return cfg >= kMinMultiTurnCfg && cfg <= kMaxMultiTurnCfg;
}

double absJointDelta(double a, double b)
{
    return std::abs(a - b);
}
}

// ====================================================================
// 构造函数：解析线条序列并生成运动程序表格
// ====================================================================
TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, int platePos, double thickness, QWidget *parent)
    : QDialog(parent), m_devId(devId), m_paths(paths), m_ucs(ucs)
{
    setWindowTitle("任务程序运行控制台 (MultiMove2)");
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
    m_coordCombo->installEventFilter(this);
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
    m_robotToolCombo->installEventFilter(this);
    coordLayout->addWidget(m_robotToolCombo);

    coordLayout->addSpacing(10);
    coordLayout->addWidget(new QLabel("机器人 Wobj:", this));
    m_robotUserCombo = new QComboBox(this);
    m_robotUserCombo->setEditable(true);
    m_robotUserCombo->installEventFilter(this);
    coordLayout->addWidget(m_robotUserCombo);
    coordLayout->addSpacing(15);
    coordLayout->addWidget(new QLabel("板材位置:", this));
    m_platePosCombo = new QComboBox(this);
    m_platePosCombo->addItems({"Z轴上方", "Z轴下方"});
    m_platePosCombo->setCurrentIndex(platePos);
    m_platePosCombo->installEventFilter(this);
    coordLayout->addWidget(m_platePosCombo);
    coordLayout->addSpacing(10);
    coordLayout->addWidget(new QLabel("板材厚度:", this));
    m_thicknessSpin = new QDoubleSpinBox(this);
    m_thicknessSpin->setRange(0, 1000);
    m_thicknessSpin->setDecimals(2);
    m_thicknessSpin->setValue(thickness);
    m_thicknessSpin->setSuffix(" mm");
    m_thicknessSpin->installEventFilter(this);
    coordLayout->addWidget(m_thicknessSpin);
    coordLayout->addStretch();
    tableLayout->addLayout(coordLayout);
    QHBoxLayout* advConfigLayout = new QHBoxLayout();
    m_useRetractTurnCheck = new QCheckBox("开启大角度退刀避障", this);
    m_useRetractTurnCheck->setChecked(true); // 默认开启
    m_useRetractTurnCheck->setStyleSheet("font-weight: bold; color: #D84315;");
    m_useRetractTurnCheck->installEventFilter(this);
    advConfigLayout->addWidget(m_useRetractTurnCheck);
    advConfigLayout->addSpacing(10);
    QLabel* thresholdLbl = new QLabel("触发阈值(度):", this);
    advConfigLayout->addWidget(thresholdLbl);
    m_retractAngleThresholdSpin = new QDoubleSpinBox(this);
    m_retractAngleThresholdSpin->setRange(5.0, 180.0);
    m_retractAngleThresholdSpin->setValue(45.0); // 默认超过 45 度触发抬刀翻转
    m_retractAngleThresholdSpin->setDecimals(1);
    m_retractAngleThresholdSpin->installEventFilter(this);
    advConfigLayout->addWidget(m_retractAngleThresholdSpin);
    advConfigLayout->addStretch();
    QHBoxLayout* advConfigLayout2 = new QHBoxLayout();
    m_useDynamicHeightCheck = new QCheckBox("开启智能动态抬刀", this);
    m_useDynamicHeightCheck->setChecked(true); // 默认开启
    m_useDynamicHeightCheck->setStyleSheet("font-weight: bold; color: #2E7D32;"); // 绿色
    m_useDynamicHeightCheck->installEventFilter(this);

    QLabel* baseHeightLbl = new QLabel("基础安全高度(mm):", this);
    m_baseHeightSpin = new QDoubleSpinBox(this);
    m_baseHeightSpin->setRange(10.0, 500.0);
    m_baseHeightSpin->setValue(50.0); // 默认离板材 50mm
    m_baseHeightSpin->installEventFilter(this);

    QLabel* maxHeightLbl = new QLabel("180度附加增量(mm):", this);
    m_maxHeightAddSpin = new QDoubleSpinBox(this);
    m_maxHeightAddSpin->setRange(0.0, 500.0);
    m_maxHeightAddSpin->setValue(100.0); // 默认 180度时多抬高 100mm
    m_maxHeightAddSpin->installEventFilter(this);

    advConfigLayout2->addWidget(m_useDynamicHeightCheck);
    advConfigLayout2->addSpacing(10);
    advConfigLayout2->addWidget(baseHeightLbl);
    advConfigLayout2->addWidget(m_baseHeightSpin);
    advConfigLayout2->addSpacing(10);
    advConfigLayout2->addWidget(maxHeightLbl);
    advConfigLayout2->addWidget(m_maxHeightAddSpin);
    advConfigLayout2->addStretch();

    tableLayout->addLayout(advConfigLayout);
    tableLayout->addLayout(advConfigLayout2);

    connect(m_useRetractTurnCheck, &QCheckBox::stateChanged, this, [this](int state) {
        m_retractAngleThresholdSpin->setEnabled(state == Qt::Checked);
        if (!m_paths.isEmpty()) generateProgram();
    });
    connect(m_retractAngleThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!m_paths.isEmpty()) generateProgram();
    });
    connect(m_useDynamicHeightCheck, &QCheckBox::stateChanged, this, [this](int state) {
        m_baseHeightSpin->setEnabled(state == Qt::Checked);
        m_maxHeightAddSpin->setEnabled(state == Qt::Checked);
        if (!m_paths.isEmpty()) generateProgram();
    });
    connect(m_baseHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!m_paths.isEmpty()) generateProgram();
    });
    connect(m_maxHeightAddSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!m_paths.isEmpty()) generateProgram();
    });

    // 安全获取机器人当前的 Tool 和 Wobj
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        // 1. 转换 Tool 列表
        try {
            std::vector<std::string> toolNames;
            if (RobotAPI::GetToolNameList(toolNames, m_devId) == 0) {
                for (const std::string& name : toolNames) {
                    m_robotToolCombo->addItem(QString::fromStdString(name)); // C++ string 转换为 Qt String
                }
            }
        } catch (...) {} // 防止底层跨库崩溃

        // 2. 转换 Wobj 列表
        try {
            std::vector<std::string> wobjNames;
            if (RobotAPI::GetUserNameList(wobjNames, m_devId) == 0) {
                for (const std::string& name : wobjNames) {
                    m_robotUserCombo->addItem(QString::fromStdString(name));
                }
            }
        } catch (...) {}

        // 3. 获取并设置当前选中的坐标系
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

    // 智能兜底：如果列表还是空的（比如获取失败），自动生成 32 个默认编号！
    if (m_robotToolCombo->count() == 0) {
        for(int i=0; i<=10; i++) m_robotToolCombo->addItem(QString("tool%1").arg(i));
    }
    if (m_robotUserCombo->count() == 0) {
        for(int i=0; i<=32; i++) m_robotUserCombo->addItem(QString("wobj%1").arg(i));
    }

    // 1. 当修改“机器人 Wobj (用户坐标系)”时
    connect(m_robotUserCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_devId != 0 && RobotAPI::IsConnected(m_devId) && !text.isEmpty()) {
            RobotAPI::SetCurrentUframeByName(text.toStdString(), m_devId);
            if (!m_paths.isEmpty()) {
                generateProgram();
            }
        }
    });

    // 2. 当修改“机器人 Tool (工具坐标系)”时，同样进行下发和重算
    connect(m_robotToolCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_devId != 0 && RobotAPI::IsConnected(m_devId) && !text.isEmpty()) {
            // 同步修改底层 Tool
            RobotAPI::SetCurrentToolByName(text.toStdString(), m_devId);

            if (!m_paths.isEmpty()) {
                generateProgram();
            }
        }
    });

    // 3. 当修改“加工几何基准 (默认基座 vs 当前用户)”时，重算表格
    connect(m_coordCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_paths.isEmpty()) {
            generateProgram();
        }
    });
    connect(m_platePosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_paths.isEmpty()) generateProgram();
    });
    connect(m_thicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!m_paths.isEmpty()) generateProgram();
    });

    connect(m_platePosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!m_paths.isEmpty()) generateProgram();
        emit workpieceParamsChanged(index, m_thicknessSpin->value());
    });
    connect(m_thicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        if (!m_paths.isEmpty()) generateProgram();
        emit workpieceParamsChanged(m_platePosCombo->currentIndex(), value);
    });

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
    m_robotStateLabel = new QLabel("底层状态: 获取中...", this);
    m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
    bottomLayout->addWidget(m_robotStateLabel);

    bottomLayout->addSpacing(20);

    m_statusLabel = new QLabel("状态: 程序已生成，准备就绪...", this);
    m_statusLabel->setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;");
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

    QLabel* speedLabel = new QLabel("全局倍率:", this);
    speedLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    bottomLayout->addWidget(speedLabel);

    m_speedRatioSpinBox = new QSpinBox(this);
    m_speedRatioSpinBox->setRange(1, 100);  // 倍率范围 1% - 100%
    m_speedRatioSpinBox->setSuffix(" %");
    m_speedRatioSpinBox->setFixedWidth(80);
    m_speedRatioSpinBox->setStyleSheet("QSpinBox { padding: 4px; font-weight: bold; font-size: 14px; border: 1px solid #aaa; border-radius: 3px; }");
    bottomLayout->addWidget(m_speedRatioSpinBox);
    bottomLayout->addSpacing(20);

    // 1. 初始化时尝试读取底层的真实倍率并同步到界面
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        unsigned int curRatio = 20; // 给个保底值 20
        if (RobotAPI::GetCurrentSpeedRatio(curRatio, m_devId) == 0) {
            m_speedRatioSpinBox->setValue(curRatio);
        } else {
            m_speedRatioSpinBox->setValue(20);
        }
    } else {
        m_speedRatioSpinBox->setValue(20);
    }

    // 2. 绑定事件：允许用户在机器人运行时实时点击上下箭头改变速度
    connect(m_speedRatioSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
            int ret = RobotAPI::SetGlobalSpeed(value, m_devId);
            if (ret == 0 && m_statusLabel) {
                m_statusLabel->setText(QString("✔️ 全局倍率已实时调整为: %1%").arg(value));
            }
        }
    });

    m_startBtn = new QPushButton("▶ 启动程序", this);
    m_startBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_pauseBtn = new QPushButton("⏸ 暂停", this);
    m_pauseBtn->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_resumeBtn = new QPushButton("⏭ 恢复", this);
    m_resumeBtn->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");
    m_resetBtn = new QPushButton("⏹ 重置/停止", this);
    m_resetBtn->setStyleSheet("background-color: #E53935; color: white; font-weight: bold; padding: 8px 15px; border-radius: 4px;");

    bottomLayout->addWidget(m_startBtn);
    bottomLayout->addWidget(m_pauseBtn);
    bottomLayout->addWidget(m_resumeBtn);
    bottomLayout->addWidget(m_resetBtn);
    mainLayout->addLayout(bottomLayout);

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

void TaskProgramDialog::setBlockMoveRunning(bool running) {
    if (m_startBtn) m_startBtn->setEnabled(!running);
    if (m_pauseBtn) m_pauseBtn->setEnabled(running);
    if (m_resetBtn) m_resetBtn->setEnabled(running);
}

// ----------------------------------------------------
// 辅助添加行 (带备注)
// ----------------------------------------------------
void TaskProgramDialog::addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap, const QString& remark, int insertRowIndex) {
    // 如果没有指定插入位置(比如自动生成时)，默认追加到列表末尾
    int row = (insertRowIndex >= 0) ? insertRowIndex : m_table->rowCount();

    m_table->insertRow(row);
    double defaultPos[6] = {0,0,0,0,0,0};
    if (!pos) pos = defaultPos;

    QComboBox* moveCombo = new QComboBox();
    moveCombo->addItems({"1: 关节运动(Joint)", "2: 连续直线(Lin)", "3: 连续圆弧(Circ)", "4: 圆角(CircAng)"});
    if (moveType >= 1 && moveType <= 4) moveCombo->setCurrentIndex(moveType - 1);
    moveCombo->installEventFilter(this);
    m_table->setCellWidget(row, 0, moveCombo);

    QComboBox* posCombo = new QComboBox();
    posCombo->addItems({"1: Joint数据", "2: Cart数据"});
    if (posType == 1 || posType == 2) posCombo->setCurrentIndex(posType - 1);
    posCombo->installEventFilter(this);
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

void TaskProgramDialog::onAddRowClicked() {
    int row = m_table->currentRow();
    if (row < 0) row = m_table->rowCount();
    addRow(2, 2, nullptr, 100, 50, 50, 0, "", row);
    m_table->selectRow(row);
}

void TaskProgramDialog::onRemoveRowClicked() {
    if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow());
}

void TaskProgramDialog::onSyncPosClicked() {
    if (m_devId == 0) return;
    RobotAPI::PosData pd;
    if (RobotAPI::GetPositionData(pd, m_devId) == 0) {
        double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
        int row = m_table->currentRow();
        if (row < 0) row = m_table->rowCount(); // 没选中就加到末尾
        addRow(2, 2, p, 100, 50, 50, 0, "抓取当前坐标", row);
        m_table->selectRow(row);
        m_statusLabel->setText("当前位置已抓取为新动作！");
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
    if (m_devId == 0 || rowCount == 0) return;

    // 启动前先检查一次是否已经处于报警状态
    bool hasAlarmInit = false;
    if (RobotAPI::GetCurrentAlarmStatus(hasAlarmInit, m_devId) == 0 && hasAlarmInit) {
        QMessageBox::warning(this, "启动失败", "机器人当前存在报警，无法启动程序！\n请先清除报警。");
        return;
    }

    // =========================================================
    // 第一步：在主线程(UI)提取所有表格数据，防止子线程跨线程崩溃
    // =========================================================
    QVector<PathPointData> tableData(rowCount);
    for (int r = 0; r < rowCount; ++r) {
        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        tableData[r].moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        tableData[r].posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        for (int i = 0; i < 6; ++i) {
            tableData[r].p[i] = m_table->item(r, i + 2)->text().toDouble();
        }
        tableData[r].speed = m_table->item(r, 8)->text().toDouble();
        tableData[r].acc = m_table->item(r, 9)->text().toDouble();
        tableData[r].dec = m_table->item(r, 10)->text().toDouble();
        tableData[r].overlapping = (r == rowCount - 1) ? 0.0 : m_table->item(r, 11)->text().toDouble();
    }

    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    unsigned int startSpeedRatio = m_speedRatioSpinBox->value();
    unsigned int devId = m_devId;

    m_startBtn->setEnabled(false);
    // 界面显示提示，后台开始狂奔，但软件依然顺滑！
    m_statusLabel->setText("正在进行离线全量运动学逆解 (后台多线程计算中)...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;");

    // =========================================================
    // 第二步：开启后台独立子线程，全权负责 IK 解算与网络推送
    // =========================================================
    m_blockMoveThread = QThread::create([this, tableData, devId, selTool, selWobj, useUcs, startSpeedRatio]() mutable {

        std::string motionWobj = useUcs ? selWobj : std::string("wobj0");

        RobotAPI::RobotPos currentPos;
        memset(&currentPos, 0, sizeof(currentPos));
        if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPos, devId);
        else RobotAPI::GetBaseCoordinatePos2(currentPos, devId);

        // ====================================================================
        // 🌟 核心引擎：带有前瞻状态机的动态运动学求解器
        // ====================================================================
        auto simulateIK = [&](RobotAPI::RobotPos& inOutPos, RobotAPI::RobotJoint& inOutJoints, const PathPointData& data, bool isDebug = false, int rowIndex = -1) -> bool {
            RobotAPI::RobotPos localP = currentPos;
            if (data.posType == 2 && useUcs) {
                localP.x = data.p[0]; localP.y = data.p[1]; localP.z = data.p[2];
                localP.a = data.p[3]; localP.b = data.p[4]; localP.c = data.p[5];

                if (data.moveType == 1) {
                    localP.cfgx = 0; localP.cfg1 = 0; localP.cfg4 = 0; localP.cfg6 = 0;
                } else {
                    localP.cfgx = inOutPos.cfgx; localP.cfg1 = inOutPos.cfg1;
                    localP.cfg4 = inOutPos.cfg4; localP.cfg6 = inOutPos.cfg6;
                }

                RobotAPI::RobotJoint tJ; memset(&tJ, 0, sizeof(tJ));
                if (RobotAPI::IkSolver(localP, tJ, selTool, motionWobj, devId) == 0) {

                    if (std::abs(tJ.j[4]) < 3.0) { // 规避奇异点
                        localP.b += 3.0;
                        RobotAPI::IkSolver(localP, tJ, selTool, motionWobj, devId);
                    }

                    if (data.moveType != 1) { // 多圈 CFG 追踪与退绕
                        double deltaJ6 = tJ.j[5] - inOutJoints.j[5];
                        if (deltaJ6 < -180.0) { localP.cfg6 += 1; RobotAPI::IkSolver(localP, tJ, selTool, motionWobj, devId); }
                        else if (deltaJ6 > 180.0) { localP.cfg6 -= 1; RobotAPI::IkSolver(localP, tJ, selTool, motionWobj, devId); }

                        deltaJ6 = tJ.j[5] - inOutJoints.j[5];
                        if (std::abs(deltaJ6) > 160.0) {
                            if (deltaJ6 > 0) localP.a -= 360.0; else localP.a += 360.0;
                            RobotAPI::IkSolver(localP, tJ, selTool, motionWobj, devId);
                        }
                    }

                    RobotAPI::RobotPos baseP;
                    RobotAPI::FkSolver(tJ, baseP, selTool, "wobj0", devId);

                    auto calcCfg = [](double angle) -> int {
                        if (angle > 180.0) return std::ceil((angle - 180.0) / 360.0);
                        if (angle <= -180.0) return std::floor((angle + 180.0) / 360.0);
                        return 0;
                    };

                    baseP.cfg1 = calcCfg(tJ.j[0]);
                    baseP.cfg4 = calcCfg(tJ.j[3]);
                    baseP.cfg6 = calcCfg(tJ.j[5]);

                    inOutPos = baseP;
                    inOutJoints = tJ;

                    // 🌟 日志输出核心：如果开启 debug，则向控制台打印经过所有算法平滑后的最终底层物理角度
                    if (isDebug) {
                        qDebug().noquote() << QString("👉 [第%1行 UCS] J1:%2 J2:%3 J3:%4 | J4:%5 J5:%6 J6:%7 | cfg6:%8")
                                                  .arg(rowIndex + 1, 3)
                                                  .arg(tJ.j[0], 7, 'f', 2).arg(tJ.j[1], 7, 'f', 2).arg(tJ.j[2], 7, 'f', 2)
                                                  .arg(tJ.j[3], 7, 'f', 2).arg(tJ.j[4], 7, 'f', 2).arg(tJ.j[5], 7, 'f', 2)
                                                  .arg(baseP.cfg6);
                    }
                    return true;
                }
                return false;
            } else {
                RobotAPI::RobotPos baseP;
                baseP.x = data.p[0]; baseP.y = data.p[1]; baseP.z = data.p[2];
                baseP.a = data.p[3]; baseP.b = data.p[4]; baseP.c = data.p[5];
                if (data.moveType == 1) { baseP.cfgx = 0; baseP.cfg6 = 0; }
                else { baseP.cfgx = inOutPos.cfgx; baseP.cfg6 = inOutPos.cfg6; }

                RobotAPI::RobotJoint tJ; memset(&tJ, 0, sizeof(tJ));
                if (RobotAPI::IkSolver(baseP, tJ, selTool, "wobj0", devId) == 0) {
                    inOutPos = baseP; inOutJoints = tJ;
                    if (isDebug) {
                        qDebug().noquote() << QString("👉 [第%1行 WOBJ] J1:%2 J2:%3 J3:%4 | J4:%5 J5:%6 J6:%7 | cfg6:%8")
                                                  .arg(rowIndex + 1, 3)
                                                  .arg(tJ.j[0], 7, 'f', 2).arg(tJ.j[1], 7, 'f', 2).arg(tJ.j[2], 7, 'f', 2)
                                                  .arg(tJ.j[3], 7, 'f', 2).arg(tJ.j[4], 7, 'f', 2).arg(tJ.j[5], 7, 'f', 2)
                                                  .arg(baseP.cfg6);
                    }
                    return true;
                }
                return false;
            }
        };

        // ====================================================================
        // 🌟 阶段一：全局前瞻预判与预卷绕 (Global Look-ahead Optimization)
        // ====================================================================
        struct PlannedPoint {
            RobotAPI::RobotPos pos;
            RobotAPI::RobotJoint joint;
            bool valid = false;
            double j6Delta = 0.0;
            int cfg6 = 0;
        };

        auto planPointIK = [&](const RobotAPI::RobotPos& prevPos,
                               const RobotAPI::RobotJoint& prevJoints,
                               const PathPointData& data,
                               PlannedPoint& out,
                               QString& failReason,
                               int rowIndex) -> bool {
            RobotAPI::RobotPos seed = currentPos;
            seed.x = data.p[0];
            seed.y = data.p[1];
            seed.z = data.p[2];
            seed.a = normalizeAngle180(data.p[3]);
            seed.b = normalizeAngle180(data.p[4]);
            seed.c = normalizeAngle180(data.p[5]);

            const int prevCfg1 = isCfgInRange(prevPos.cfg1) ? prevPos.cfg1 : clampMultiTurnCfg(calcMultiTurnCfg(prevJoints.j[0]));
            const int prevCfg4 = isCfgInRange(prevPos.cfg4) ? prevPos.cfg4 : clampMultiTurnCfg(calcMultiTurnCfg(prevJoints.j[3]));
            const int prevCfg6 = isCfgInRange(prevPos.cfg6) ? prevPos.cfg6 : clampMultiTurnCfg(calcMultiTurnCfg(prevJoints.j[5]));
            seed.cfgx = prevPos.cfgx;
            seed.cfg1 = prevCfg1;
            seed.cfg4 = prevCfg4;

            const double angleTurns[] = {0.0, -360.0, 360.0};
            const double bOffsets[] = {0.0, 2.0, -2.0, 5.0, -5.0};
            const int cfg6Candidates[] = {
                prevCfg6,
                clampMultiTurnCfg(calcMultiTurnCfg(prevJoints.j[5])),
                clampMultiTurnCfg(prevCfg6 - 1),
                clampMultiTurnCfg(prevCfg6 + 1)
            };

            bool hasBest = false;
            double bestScore = std::numeric_limits<double>::max();
            PlannedPoint best;
            int lastIkRet = 0;

            for (double aTurn : angleTurns) {
                for (double cTurn : angleTurns) {
                    for (double bOffset : bOffsets) {
                        for (int cfg6Candidate : cfg6Candidates) {
                            RobotAPI::RobotPos candidate = seed;
                            candidate.a = seed.a + aTurn;
                            candidate.b = seed.b + bOffset;
                            candidate.c = seed.c + cTurn;
                            candidate.cfg6 = cfg6Candidate;

                            RobotAPI::RobotJoint joints;
                            memset(&joints, 0, sizeof(joints));
                            const int ikRet = RobotAPI::IkSolver(candidate, joints, selTool, motionWobj, devId);
                            lastIkRet = ikRet;
                            if (ikRet != 0) continue;

                            const double j6Delta = joints.j[5] - prevJoints.j[5];
                            const int solvedCfg6 = calcMultiTurnCfg(joints.j[5]);
                            if (!isCfgInRange(solvedCfg6)) continue;
                            if (std::abs(j6Delta) > kMaxContinuousJ6Delta) continue;
                            if (std::abs(solvedCfg6 - prevCfg6) > 1) continue;
                            if (std::abs(joints.j[4]) < kHardWristSingularityDeg) continue;

                            RobotAPI::RobotPos basePos;
                            memset(&basePos, 0, sizeof(basePos));
                            const int fkRet = RobotAPI::FkSolver(joints, basePos, selTool, "wobj0", devId);
                            if (fkRet != 0) continue;

                            const int solvedCfg1 = calcMultiTurnCfg(joints.j[0]);
                            const int solvedCfg4 = calcMultiTurnCfg(joints.j[3]);
                            if (!isCfgInRange(solvedCfg1) || !isCfgInRange(solvedCfg4)) continue;
                            basePos.cfg1 = solvedCfg1;
                            basePos.cfg4 = solvedCfg4;
                            basePos.cfg6 = solvedCfg6;

                            double jointScore = 0.0;
                            for (int i = 0; i < 6; ++i) {
                                const double weight = (i == 5) ? 6.0 : 1.0;
                                jointScore += weight * absJointDelta(joints.j[i], prevJoints.j[i]);
                            }
                            const double singularPenalty = std::max(0.0, kSoftWristSingularityDeg - std::abs(joints.j[4])) * 100.0;
                            const double cfgPenalty = std::abs(solvedCfg6 - prevCfg6) * 500.0;
                            const double posturePenalty = (std::abs(aTurn) + std::abs(cTurn) + std::abs(bOffset) * 20.0) * 0.05;
                            const double score = jointScore + singularPenalty + cfgPenalty + posturePenalty;

                            if (!hasBest || score < bestScore) {
                                hasBest = true;
                                bestScore = score;
                                best.pos = basePos;
                                best.joint = joints;
                                best.valid = true;
                                best.j6Delta = j6Delta;
                                best.cfg6 = solvedCfg6;
                            }
                        }
                    }
                }
            }

            if (!hasBest) {
                failReason = QString("第 %1 行姿态规划失败：IK不可达、J6跨圈或接近腕部奇异。上一点 J6=%2, cfg6=%3, 目标ABC=(%4,%5,%6), 最后IK返回=%7")
                                 .arg(rowIndex + 1)
                                 .arg(prevJoints.j[5], 0, 'f', 2)
                                 .arg(prevCfg6)
                                 .arg(data.p[3], 0, 'f', 2)
                                 .arg(data.p[4], 0, 'f', 2)
                                 .arg(data.p[5], 0, 'f', 2)
                                 .arg(lastIkRet);
                qWarning().noquote() << "[TaskProgram] IK plan failed:" << failReason;
                return false;
            }

            out = best;
            qDebug().noquote() << QString("[TaskProgram] IK plan row %1: J6=%2 dJ6=%3 cfg6=%4 J5=%5 baseABC=(%6,%7,%8)")
                                      .arg(rowIndex + 1)
                                      .arg(out.joint.j[5], 0, 'f', 2)
                                      .arg(out.j6Delta, 0, 'f', 2)
                                      .arg(out.cfg6)
                                      .arg(out.joint.j[4], 0, 'f', 2)
                                      .arg(out.pos.a, 0, 'f', 2)
                                      .arg(out.pos.b, 0, 'f', 2)
                                      .arg(out.pos.c, 0, 'f', 2);
            return true;
        };

        RobotAPI::RobotPos simPos = currentPos;
        RobotAPI::RobotJoint simJoints; memset(&simJoints, 0, sizeof(simJoints));
        RobotAPI::IkSolver(currentPos, simJoints, selTool, motionWobj, devId);

        int rows = tableData.size();
        QVector<PlannedPoint> plannedRows(rows);
        QString planError;
        bool planOk = true;
        for (int r = 0; r < rows; ++r) {
            PlannedPoint planned;
            if (!planPointIK(simPos, simJoints, tableData[r], planned, planError, r)) {
                planOk = false;
                break;
            }
            plannedRows[r] = planned;
            simPos = planned.pos;
            simJoints = planned.joint;
        }

        if (!planOk) {
            QMetaObject::invokeMethod(this, [this, planError]() {
                QMessageBox::warning(this, "轨迹规划失败", planError);
                m_startBtn->setEnabled(true);
                m_statusLabel->setText("姿态规划失败");
            }, Qt::QueuedConnection);
            return;
        }

        int r_idx = rows;
        while (r_idx < rows) {
            int start_r = r_idx;
            int end_r = r_idx + 1;
            // 扫描直到遇到下一个高空 Joint 动作，划定一个“原子块”
            while (end_r < rows && tableData[end_r].moveType != 1) {
                end_r++;
            }

            // 保存现场，用于回滚
            RobotAPI::RobotPos backupPos = simPos;
            RobotAPI::RobotJoint backupJoints = simJoints;
            double min_j6 = 99999.0;
            double max_j6 = -99999.0;
            bool simSuccess = true;

            // 首次预演：探明该原子块的 J6 自然物理行程边界
            for (int i = start_r; i < end_r; ++i) {
                if (!simulateIK(simPos, simJoints, tableData[i])) { simSuccess = false; break; }
                if (simJoints.j[5] < min_j6) min_j6 = simJoints.j[5];
                if (simJoints.j[5] > max_j6) max_j6 = simJoints.j[5];
            }

            if (simSuccess) {
                double center_j6 = (min_j6 + max_j6) / 2.0;
                // 计算需要反向退绕的圈数，使 J6 行程完美居中于 0 度附近，远离物理报警极限！
                int K = std::round(-center_j6 / 360.0);

                if (K != 0) {
                    // 回滚状态，并向该块所有的工艺点注入预卷绕补偿
                    simPos = backupPos;
                    simJoints = backupJoints;
                    for (int i = start_r; i < end_r; ++i) {
                        tableData[i].p[3] += K * 360.0;
                        simulateIK(simPos, simJoints, tableData[i]); // 重新推进真实状态机
                    }
                }
            }
            r_idx = end_r;
        }

        // ====================================================================
        // 🌟 阶段二：正式生成底层指令栈 (此时全量轨迹已被优化为完美区间)
        // ====================================================================
        std::vector<RobotAPI::MultiMoveInfo2> mps;
        QString errorMsg;

        for (int r = 0; r < rows; ++r) {
            if (m_blockMoveStopRequested) { errorMsg = "用户已中止后台解算"; break; }

            RobotAPI::MultiMoveInfo2 mp;

            mp.moveType = tableData[r].moveType;
            mp.posType = tableData[r].posType;
            if (r == 0) mp.moveType = 1;

            mp.speed = tableData[r].speed;
            mp.acc = tableData[r].acc;
            mp.dec = tableData[r].dec;
            mp.jerk = 100.0;
            mp.auxOverlapping = 1e100;
            mp.flags = 0;
            mp.overlapping = tableData[r].overlapping;

            if (mp.moveType == 3) { // 连续圆弧
                if (r + 1 >= rows) { errorMsg = "发现不完整的圆弧指令（缺少终点）！"; break; }
                mp.cp[0] = plannedRows[r].pos;
                mp.cp[1] = plannedRows[r + 1].pos;
                r++;
            } else if (mp.moveType == 4) {
                if (r + 3 >= rows) { errorMsg = "发现不完整的整圆指令（缺少参数点）！"; break; }

                for (int i = 0; i < 4; ++i) {
                    // 🌟 激活打印
                    mp.cp[i] = plannedRows[r + i].pos;
                }
                if (!errorMsg.isEmpty()) break;

                double x0 = mp.cp[0].x, y0 = mp.cp[0].y;
                double x1 = mp.cp[1].x, y1 = mp.cp[1].y;
                double x2 = mp.cp[2].x, y2 = mp.cp[2].y;
                double cross = (x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1);
                mp.flags = (cross < 0) ? 2 : 0;
                mp.overlapping = 0.0;
                r += 3;
            } else {
                // 🌟 激活打印
                mp.cp[0] = plannedRows[r].pos;
            }
            mps.push_back(mp);
        }

        // 报错拦截：切回主线程进行弹窗 UI 交互
        if (!errorMsg.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, errorMsg]() {
                if (errorMsg != "用户已中止后台解算") QMessageBox::warning(this, "轨迹错误", errorMsg);
                m_startBtn->setEnabled(true);
                m_statusLabel->setText("就绪");
            }, Qt::QueuedConnection);
            return;
        }

        if (mps.empty()) {
            QMetaObject::invokeMethod(this, [this]() {
                m_startBtn->setEnabled(true);
                m_statusLabel->setText("轨迹为空！");
            }, Qt::QueuedConnection);
            return;
        }

        // 解算成功：通知主界面并开始实际的运动推送
        QMetaObject::invokeMethod(this, [this, total = mps.size()]() {
            setBlockMoveRunning(true);
            m_blockMoveStopRequested = false;
            m_resetAfterBlockStop = false;
            m_statusLabel->setText(QString("后台前瞻解算完毕，已重构为最优姿态！共 %1 个动作...").arg(total));
        }, Qt::QueuedConnection);

        // =========================================================
        // 运动执行阶段
        // =========================================================
        RobotAPI::MultiMove2Reset(devId);
        QThread::msleep(50);

        if (!selTool.empty()) RobotAPI::SetCurrentToolByName(selTool, devId);
        if (!motionWobj.empty()) RobotAPI::SetCurrentUframeByName(motionWobj, devId);

        RobotAPI::SetCurrentStepMode(ROBOX_MODE_CONTINUOUS, devId);
        RobotAPI::SetGlobalSpeed(startSpeedRatio, devId);
        QThread::msleep(50);

        int totalPoints = static_cast<int>(mps.size());
        int sentIndex = 0;

        while (sentIndex < totalPoints) {
            if (m_blockMoveStopRequested) {
                RobotAPI::MultiMove2Reset(devId);
                break;
            }

            bool hasAlarm = false;
            if (RobotAPI::GetCurrentAlarmStatus(hasAlarm, devId) == 0 && hasAlarm) {
                RobotAPI::MultiMove2Hold(devId);
                RobotAPI::MultiMove2Reset(devId);

                QMetaObject::invokeMethod(this, [this]() {
                    m_statusLabel->setText("机器人在运行中出现报警，程序已自动暂停并重置！");
                    m_statusLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
                }, Qt::QueuedConnection);
                break;
            }

            int chunkCount = std::min(3, totalPoints - sentIndex);
            std::vector<RobotAPI::MultiMoveInfo2> chunk(mps.begin() + sentIndex, mps.begin() + sentIndex + chunkCount);

            int ret = RobotAPI::MultiMove2Start(chunk, devId);

            if (ret == 0) {
                sentIndex += chunkCount;
                QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
                    m_statusLabel->setText(QString("滑动窗口持续喂点中: %1 / %2").arg(sentIndex).arg(totalPoints));
                }, Qt::QueuedConnection);

            } else if (ret == 40) {
                // 缓存打满，下次重试，不报错
            } else {
                QString errMsg;
                switch(ret) {
                case 1: errMsg = "与机器人连接失败"; break;
                case 2: errMsg = "尚未与机器人连接"; break;
                case 3: errMsg = "与机器人连接中断"; break;
                case 4: errMsg = "访问拒绝，机器人设置为不允许"; break;
                case 5: errMsg = "当前模式不支持该操作"; break;
                case 6: errMsg = "上电失败"; break;
                case 7: errMsg = "下电失败"; break;
                case 8: errMsg = "设置控制模式失败"; break;
                case 9: errMsg = "设置速度倍率失败，请检查倍率值"; break;
                case 10: errMsg = "切换通道失败，请检查通道号"; break;
                case 11: errMsg = "启动程序失败(需无告警、已加载且暂停/停止)"; break;
                case 12: errMsg = "复位程序失败(需处于暂停/停止状态)"; break;
                case 13: errMsg = "加载程序失败，请查看告警列表"; break;
                case 15: errMsg = "系统仍处于运行中，无法执行运动指令"; break;
                case 16: errMsg = "设置变量失败"; break;
                case 17: errMsg = "查询变量失败"; break;
                case 18: errMsg = "控制机器人移动失败"; break;
                case 19: errMsg = "设置外部控制失败"; break;
                case 10001: errMsg = "操作失败"; break;
                case 10002: errMsg = "获取客户端失败"; break;
                case 10003: errMsg = "创建monitor失败"; break;
                case 10004: errMsg = "启动monitor失败"; break;
                case 10005: errMsg = "获取文件操作接口失败"; break;
                case 10006: errMsg = "从控制器下载文件失败"; break;
                case 10007: errMsg = "往控制器上传XPL文件失败"; break;
                case 10008: errMsg = "XPL文件存在"; break;
                case 10009: errMsg = "XPL文件不存在"; break;
                case 10010: errMsg = "XPL文件查询失败"; break;
                case 10011: errMsg = "获取机型名失败"; break;
                case 10012: errMsg = "获取运动数据失败"; break;
                case 10013: errMsg = "环境未获取"; break;
                case 10014: errMsg = "写权限设置失败"; break;
                case 10015: errMsg = "获取monitor中数据失败"; break;
                case 10016: errMsg = "获取状态信息失败"; break;
                case 10017: errMsg = "创建文件夹失败"; break;
                case 10018: errMsg = "删除文件夹失败"; break;
                case 10019: errMsg = "文件夹路径无效"; break;
                case 10020: errMsg = "禁止删除系统文件夹"; break;
                case 10021: errMsg = "设备已存在"; break;
                case 10022: errMsg = "设备不存在"; break;
                case 10023: errMsg = "保存到文件失败"; break;
                case 10024: errMsg = "操作超时"; break;
                case 10025: errMsg = "正运动学计算失败"; break;
                case 10026: errMsg = "逆运动学计算失败"; break;
                case 10027: errMsg = "错误状态启动 (请检查机器是否处于连续运行模式)"; break;
                case 10028: errMsg = "组合运动数量超限"; break;
                case 10029: errMsg = "设置参数错误"; break;
                case 10030: errMsg = "没有数据"; break;
                case 10031: errMsg = "接受数据丢包"; break;
                case 10032: errMsg = "机器人不处于指定位置 (检查轨迹起点和姿态)"; break;
                case 10033: errMsg = "目标点位不可达 (奇异点或超出物理限位)"; break;
                case 10034: errMsg = "内存不足"; break;
                default:
                    if (ret < 0) errMsg = "底层通信断开或严重异常";
                    else errMsg = "未知 SDK 内部错误";
                    break;
                }

                QMetaObject::invokeMethod(this, [this, ret, errMsg]() {
                    m_statusLabel->setText(QString("下发中止！错误码 %1: %2").arg(ret).arg(errMsg));
                    m_statusLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
                }, Qt::QueuedConnection);
                break;
            }
            QThread::msleep(30);
        }

        QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints, devId]() {
            setBlockMoveRunning(false);
            m_startBtn->setEnabled(true);
            m_blockMoveThread = nullptr;

            if (m_resetAfterBlockStop && devId != 0) {
                RobotAPI::MultiMove2Reset(devId);
            }

            if (m_blockMoveStopRequested) {
                m_statusLabel->setText("已手动中止运行。");
            } else if (sentIndex >= totalPoints) {
                m_statusLabel->setText("轨迹已全量喂入控制器，等待物理动作执行结束...");
            }
        }, Qt::QueuedConnection);
    });

    // 绑定线程结束后销毁对象，启动后台大解算
    connect(m_blockMoveThread, &QThread::finished, m_blockMoveThread, &QObject::deleteLater);
    m_blockMoveThread->start();
}

void TaskProgramDialog::onPauseClicked() {
    if (m_devId == 0) return;
    (void)QtConcurrent::run([this]() {
        RobotAPI::MultiMove2Hold(m_devId);
        QMetaObject::invokeMethod(this, [this](){ m_statusLabel->setText("程序已暂停 (Hold)."); });
    });
}

void TaskProgramDialog::onResumeClicked() {
    if (m_devId == 0) return;
    (void)QtConcurrent::run([this]() {
        RobotAPI::MultiMove2Resume(m_devId);
        QMetaObject::invokeMethod(this, [this](){ m_statusLabel->setText("程序已恢复执行 (Resume)."); });
    });
}

void TaskProgramDialog::onResetClicked() {
    if (m_devId == 0) return;
    (void)QtConcurrent::run([this]() {
        RobotAPI::MultiMove2Reset(m_devId);
        QMetaObject::invokeMethod(this, [this](){ m_statusLabel->setText("程序已重置/停止 (Reset)."); });
    });
}

void TaskProgramDialog::generateProgram()
{
    m_table->setRowCount(0);
    m_table->clearSelection();
    m_table->setCurrentCell(-1, -1);

    bool useUcs = (m_coordCombo->currentData().toInt() == 1);

    RobotAPI::RobotPos currentPose;
    memset(&currentPose, 0, sizeof(currentPose));

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        RobotAPI::GetUserCoordinatePos2(currentPose, m_devId);
    }

    double plateZ = 0.0;
    double baseHeight = 50.0;

    if (m_baseHeightSpin) {
        baseHeight = m_baseHeightSpin->value();
    }

    if (m_platePosCombo && m_thicknessSpin) {
        if (m_platePosCombo->currentIndex() == 0) plateZ = m_thicknessSpin->value();
        else plateZ = -m_thicknessSpin->value();
    }
    double SAFE_HEIGHT = (plateZ > 0) ? (plateZ + baseHeight) : baseHeight;
    auto calculateDynamicHeight = [&](double angDiff) {
        if (!m_useDynamicHeightCheck || !m_useDynamicHeightCheck->isChecked()) {
            return SAFE_HEIGHT;
        }
        // 角度占比：最大计算到 180 度 (ratio = 1.0)
        double ratio = std::min(1.0, std::abs(angDiff) / 180.0);
        return SAFE_HEIGHT + (m_maxHeightAddSpin->value() * ratio);
    };

    double globalLastA = currentPose.a;
    double globalLastTangentAngle = 0.0;
    bool isFirstTangent = true;

    QPointF globalLastOffsetPt(-99999.0, -99999.0);
    QPointF globalLastUcsPt(-99999.0, -99999.0);
    QPointF globalLastOriginalPt(-99999.0, -99999.0);

    for (int idx = 0; idx < m_paths.size(); ++idx) {
        Contour c = m_paths[idx];
        if (c.points.isEmpty()) continue;

        QString typeStr = c.type;
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(typeStr);

        bool isFittedData = typeStr.contains("拟合") || typeStr.contains("样条") || typeStr.contains("Spline", Qt::CaseInsensitive);
        bool isCircle = typeStr.contains("圆") && !typeStr.contains("弧") && !typeStr.contains("角");
        bool isArc = typeStr.contains("弧") || typeStr.contains("Arc", Qt::CaseInsensitive);

        bool isClosedRaw = (std::hypot(c.points.first().x() - c.points.last().x(), c.points.first().y() - c.points.last().y()) < 0.001);
        bool isReversed = false;

        static int closedShapeCount = 0;
        if (idx == 0) closedShapeCount = 0;

        if (isClosedRaw) {
            closedShapeCount++;
            if (closedShapeCount % 2 == 0) {
                isReversed = true;
                std::reverse(c.points.begin(), c.points.end());
            }
        }

        int n = c.points.size();

        QVector<QPointF> targetPoints;
        QVector<int> targetMoveTypes;
        QVector<QString> targetRemarks;
        QVector<int> targetOrigIdx;

        if (isCircle && n >= 4) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetOrigIdx << 0;       targetRemarks << "-整圆逼近点";
            targetPoints << c.points[n / 4];     targetMoveTypes << 4; targetOrigIdx << n / 4;   targetRemarks << "-整圆途经点(P2)";
            targetPoints << c.points[n / 2];     targetMoveTypes << 4; targetOrigIdx << n / 2;   targetRemarks << "-整圆交接点(P3)";
            targetPoints << c.points[3 * n / 4]; targetMoveTypes << 4; targetOrigIdx << 3*n / 4; targetRemarks << "-整圆途经点(P4)";
            targetPoints << c.points[0];         targetMoveTypes << 4; targetOrigIdx << 0;       targetRemarks << "-整圆收刀点(P1)";
        }
        else if (isFittedData && n >= 3) {
            targetPoints << c.points[0]; targetMoveTypes << 2; targetOrigIdx << 0; targetRemarks << "-样条起点";
            for (int i = 1; i < n - 1; i += 2) {
                int segIdx = (i + 1) / 2;
                QPointF p1 = c.points[i-1], p2 = c.points[i], p3 = c.points[i+1];
                double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));
                if (std::abs(D) < 1e-6) {
                    targetPoints << p3; targetMoveTypes << 2; targetOrigIdx << i+1; targetRemarks << QString("-段%1[直线] 终点").arg(segIdx);
                } else {
                    targetPoints << p2; targetMoveTypes << 3; targetOrigIdx << i;   targetRemarks << QString("-段%1[圆弧] 途经点").arg(segIdx);
                    targetPoints << p3; targetMoveTypes << 3; targetOrigIdx << i+1; targetRemarks << QString("-段%1[圆弧] 终点").arg(segIdx);
                }
            }
            if (n % 2 == 0) {
                targetPoints << c.points[n - 1]; targetMoveTypes << 2; targetOrigIdx << n - 1; targetRemarks << "-尾部收尾";
            }
        }
        else if (isArc && n >= 3) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetOrigIdx << 0;     targetRemarks << "-圆弧起点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetOrigIdx << n / 2; targetRemarks << "-圆弧途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetOrigIdx << n - 1; targetRemarks << "-圆弧终点";
        }
        else {
            for (int i = 0; i < n; ++i) {
                targetPoints << c.points[i]; targetMoveTypes << 2; targetOrigIdx << i;
                if (i == 0) targetRemarks << "-起点";
                else if (i == n - 1) targetRemarks << "-终点";
                else targetRemarks << QString("-直线途点%1").arg(i);
            }
        }

        bool isConnectedWithNext = false;
        if (idx + 1 < m_paths.size() && !m_paths[idx + 1].points.isEmpty()) {
            QPointF nextStart = m_paths[idx + 1].points.first();
            QPointF nextEnd = m_paths[idx + 1].points.last();
            QPointF myEnd = targetPoints.last();
            if (std::hypot(myEnd.x() - nextStart.x(), myEnd.y() - nextStart.y()) < 0.001 ||
                std::hypot(myEnd.x() - nextEnd.x(), myEnd.y() - nextEnd.y()) < 0.001) {
                isConnectedWithNext = true;
            }
        }

        double B = c.bevelAngle;
        double offsetDist = c.rootFace * std::tan(B * M_PI / 180.0);

        for (int i = 0; i < targetPoints.size(); ++i) {
            QPointF pt = targetPoints[i];
            int moveType = targetMoveTypes[i];
            int origIdx = targetOrigIdx[i];
            QString remark = targetRemarks[i];
            if (isReversed) remark += "[反转]";

            QPointF tangent(0, 0);
            if (origIdx == 0) {
                if (c.points.size() > 1) tangent = c.points[1] - c.points[0];
                else tangent = QPointF(1, 0);
            } else if (origIdx == c.points.size() - 1) {
                tangent = c.points[origIdx] - c.points[origIdx - 1];
            } else {
                tangent = c.points[origIdx + 1] - c.points[origIdx - 1];
            }

            double len = std::hypot(tangent.x(), tangent.y());
            QPointF normal(0, 0);
            double currentTangentAngle = 0.0;

            if (len > 1e-6) {
                normal = QPointF(-tangent.y() / len, tangent.x() / len);
                currentTangentAngle = std::atan2(tangent.y(), tangent.x()) * 180.0 / M_PI;
            }

            pt.setX(pt.x() + normal.x() * offsetDist);
            pt.setY(pt.y() + normal.y() * offsetDist);

            QPointF ucsPt = pt;
            if (useUcs && m_ucs.valid) {
                QPointF v = pt - m_ucs.origin;
                double local_x = v.x() * m_ucs.xAxis.x() + v.y() * m_ucs.xAxis.y();
                double local_y = v.x() * m_ucs.yAxis.x() + v.y() * m_ucs.yAxis.y();
                ucsPt = QPointF(local_x, local_y);
            }

            if (isFirstTangent) {
                globalLastTangentAngle = currentTangentAngle;
                isFirstTangent = false;
            }

            double deltaA = currentTangentAngle - globalLastTangentAngle;
            while (deltaA > 180.0) deltaA -= 360.0;
            while (deltaA <= -180.0) deltaA += 360.0;
            if (std::abs(deltaA - 180.0) < 0.05) deltaA = 179.9;
            else if (std::abs(deltaA + 180.0) < 0.05) deltaA = -179.9;
            globalLastTangentAngle = currentTangentAngle;

            double finalA = globalLastA + deltaA;
            while (finalA > 180.0) finalA -= 360.0;
            while (finalA <= -180.0) finalA += 360.0;

            bool isConnectedWithPrev = (std::hypot(ucsPt.x() - globalLastUcsPt.x(), ucsPt.y() - globalLastUcsPt.y()) < 0.001);

            double overlapVal = (moveType == 3 || moveType == 4) ? 0.0 : 2.0;

            if (i > 0 || isConnectedWithPrev) {
                if (moveType == 2 || (i == 0 && isConnectedWithPrev)) {
                    double angDiff = finalA - globalLastA;
                    while (angDiff > 180.0) angDiff -= 360.0;
                    while (angDiff <= -180.0) angDiff += 360.0;
                    if (std::abs(angDiff - 180.0) < 0.05) angDiff = 179.9;
                    else if (std::abs(angDiff + 180.0) < 0.05) angDiff = -179.9;
                    if (std::abs(angDiff) > 0.5) {
                        // 读取 UI 上的设置，判断是否需要启动安全退刀
                        bool useRetract = m_useRetractTurnCheck && m_useRetractTurnCheck->isChecked()
                                          && (std::abs(angDiff) >= m_retractAngleThresholdSpin->value());

                        if (useRetract) {
                            double currentSafeHeight = calculateDynamicHeight(angDiff);

                            // 1. 避障抬刀 (Lin)
                            double pRetract[6] = { globalLastUcsPt.x(), globalLastUcsPt.y(), currentSafeHeight, globalLastA, B, 0.0 };
                            addRow(2, 2, pRetract, 100, 50, 50, 0.0, shapeName + remark + QString(" ⬆️[避障抬刀 Z=%1]").arg(currentSafeHeight, 0, 'f', 1));

                            // 2. 空中重姿态 (Joint, 防止奇异点报错)
                            double pTurn[6] = { globalLastUcsPt.x(), globalLastUcsPt.y(), currentSafeHeight, finalA, B, 0.0 };
                            addRow(1, 2, pTurn, 50, 50, 50, 0.0, shapeName + remark + " 🔄[空中重姿态]");

                            // 3. 重新落刀 (Lin)
                            double pPlunge[6] = { globalLastUcsPt.x(), globalLastUcsPt.y(), plateZ, finalA, B, 0.0 };
                            addRow(2, 2, pPlunge, 50, 50, 50, 0.0, shapeName + remark + " ⬇️[重新落刀]");
                        } else {
                            // 角度比较小，或者用户关闭了安全退刀，直接原地硬转 (Lin)
                            double pTurn[6] = { globalLastUcsPt.x(), globalLastUcsPt.y(), plateZ, finalA, B, 0.0 };
                            addRow(2, 2, pTurn, 50, 50, 50, 0.0, shapeName + remark + " 🔄[直行前转向]");
                        }
                        globalLastA = finalA;
                    }
                }
            }

            if (i == 0 && isConnectedWithPrev) {
                globalLastA = finalA;
                globalLastUcsPt = ucsPt;
                globalLastOriginalPt = targetPoints[i];
                continue;
            }

            if (i == 0) {
                if (!isConnectedWithPrev) {
                    double crossAngDiff = finalA - globalLastA;
                    while (crossAngDiff > 180.0) crossAngDiff -= 360.0;
                    while (crossAngDiff <= -180.0) crossAngDiff += 360.0;
                    double currentSafeHeight = calculateDynamicHeight(crossAngDiff);

                    double pSafe[6] = { ucsPt.x(), ucsPt.y(), currentSafeHeight, finalA, 0.0, 0.0 };
                    addRow(1, 2, pSafe, 100, 50, 50, 0.0, shapeName + remark + QString(" [跨域: 高空就位 Z=%1]").arg(currentSafeHeight, 0, 'f', 1));
                }
                double pStart[6] = { ucsPt.x(), ucsPt.y(), plateZ, finalA, B, 0.0 };
                addRow(moveType, 2, pStart, 30, 50, 50, 0.0, shapeName + remark + " [起刀]");
            }
            else if (i == targetPoints.size() - 1) {
                double pEnd[6] = { ucsPt.x(), ucsPt.y(), plateZ, finalA, B, 0.0 };
                if (isConnectedWithNext) {
                    addRow(moveType, 2, pEnd, 50, 50, 50, overlapVal, shapeName + remark);
                } else {
                    addRow(moveType, 2, pEnd, 50, 50, 50, 0.0, shapeName + remark + " (切割结束)");
                    double pRetract[6] = { ucsPt.x(), ucsPt.y(), SAFE_HEIGHT, finalA, 0.0, 0.0 };
                    addRow(1, 2, pRetract, 100, 50, 50, 0.0, shapeName + " [跨域 抬刀]");
                }
            }
            else {
                double p[6] = { ucsPt.x(), ucsPt.y(), plateZ, finalA, B, 0.0 };
                addRow(moveType, 2, p, 50, 50, 50, overlapVal, shapeName + remark);
            }

            globalLastA = finalA;
            globalLastOffsetPt = pt;
            globalLastUcsPt = ucsPt;
            globalLastOriginalPt = targetPoints[i];
        }
    }

    int rowCount = m_table->rowCount();
    for (int r = 0; r < rowCount; ++r) {
        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        int moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;
        if (moveType == 3 || moveType == 4) {
            m_table->item(r, 11)->setText("0.0");
        }

        if (r < rowCount - 1 && moveType != 3 && moveType != 4) {
            double x1 = m_table->item(r, 2)->text().toDouble();
            double y1 = m_table->item(r, 3)->text().toDouble();
            double z1 = m_table->item(r, 4)->text().toDouble();
            double x2 = m_table->item(r+1, 2)->text().toDouble();
            double y2 = m_table->item(r+1, 3)->text().toDouble();
            double z2 = m_table->item(r+1, 4)->text().toDouble();

            double dist = std::hypot(x1 - x2, y1 - y2);
            dist = std::hypot(dist, z1 - z2);
            double currentOverlap = m_table->item(r, 11)->text().toDouble();
            double maxSafeOverlap = dist * 0.45;

            if (dist < 0.001) {
                m_table->item(r, 11)->setText("0.0");
                QComboBox* nextCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r+1, 0));
                int nextMove = nextCombo ? nextCombo->currentText().left(1).toInt() : 2;
                if (nextMove != 4) m_table->item(r+1, 11)->setText("0.0");
            } else if (currentOverlap > maxSafeOverlap) {
                m_table->item(r, 11)->setText(QString::number(maxSafeOverlap, 'f', 2));
            }
        }
    }
}

void TaskProgramDialog::updateRobotState()
{
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        int state = 0;
        int ret = RobotAPI::GetMultiMove2State(state, m_devId);

        if (ret == 0) {
            QString stateStr;
            switch(state) {
            case 0: stateStr = "初始 (INIT)"; break;
            case 1: stateStr = "载入 (LAUNCH)"; break;
            case 2: stateStr = "执行 (EXEC)"; break;
            case 3: stateStr = "减速 (HOLD_DEC)"; break;
            case 4: stateStr = "停止 (HOLD)"; break;
            case 5: stateStr = "错误 (ERROR)"; break;
            case 6: stateStr = "结束 (ENDED)"; break;
            default: stateStr = QString("未知 (%1)").arg(state); break;
            }
            m_robotStateLabel->setText(QString("底层状态: %1").arg(stateStr));

            if (state == 5) {
                m_robotStateLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
            } else if (state == 2) {
                m_robotStateLabel->setStyleSheet("font-weight: bold; color: green; font-size: 14px;");
            } else {
                m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
            }
        } else {
            m_robotStateLabel->setText("底层状态: 读取失败");
        }
    }
}

// ==========================================
// 事件过滤器：全局拦截特定控件的鼠标事件
// ==========================================
bool TaskProgramDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        if (qobject_cast<QComboBox*>(obj)) {
            event->ignore();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void TaskProgramDialog::updateWorkpieceParams(int posIndex, double thickness) {
    m_platePosCombo->blockSignals(true);
    m_thicknessSpin->blockSignals(true);
    m_platePosCombo->setCurrentIndex(posIndex);
    m_thicknessSpin->setValue(thickness);
    m_platePosCombo->blockSignals(false);
    m_thicknessSpin->blockSignals(false);
    generateProgram();
}
