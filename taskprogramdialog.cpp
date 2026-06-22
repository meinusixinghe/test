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

    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    const std::string motionWobj = useUcs ? selWobj : std::string("wobj0");

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("正在进行离线全量运动学逆解...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;"); // 恢复正常的蓝色显示
    QApplication::processEvents();

    // 1. 提取当前物理坐标配置
    RobotAPI::RobotPos currentPos;
    memset(&currentPos, 0, sizeof(currentPos));
    if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPos, m_devId);
    else RobotAPI::GetBaseCoordinatePos2(currentPos, m_devId);

    // ====================================================================
    // 辅助工具：提取表格指定行的数据，并完成安全的逆解/正解
    // ====================================================================
    auto solveRow = [&](int row, RobotAPI::MultiMoveInfo2& targetMp, int arrayIndex) -> bool {
        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 1));
        int posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(row, i + 2)->text().toDouble();

        if (posType == 2 && useUcs) {
            RobotAPI::RobotPos localP = currentPos;
            localP.x = p[0]; localP.y = p[1]; localP.z = p[2];

            // 严格使用表格中读取到的 A, B, C 数据
            localP.a = p[3]; localP.b = p[4]; localP.c = p[5];

            RobotAPI::RobotJoint tJoints; memset(&tJoints, 0, sizeof(tJoints));

            if (RobotAPI::IkSolver(localP, tJoints, selTool, motionWobj, m_devId) == 0) {
                RobotAPI::RobotPos baseP; memset(&baseP, 0, sizeof(baseP));
                if (RobotAPI::FkSolver(tJoints, baseP, selTool, "wobj0", m_devId) == 0) {
                    targetMp.cp[arrayIndex].x = baseP.x; targetMp.cp[arrayIndex].y = baseP.y; targetMp.cp[arrayIndex].z = baseP.z;
                    targetMp.cp[arrayIndex].a = baseP.a; targetMp.cp[arrayIndex].b = baseP.b; targetMp.cp[arrayIndex].c = baseP.c;
                    targetMp.cp[arrayIndex].cfgx = baseP.cfgx; targetMp.cp[arrayIndex].cfg1 = baseP.cfg1;
                    targetMp.cp[arrayIndex].cfg4 = baseP.cfg4; targetMp.cp[arrayIndex].cfg6 = baseP.cfg6;
                    return true;
                }
            }
            return false; // 解算失败
        } else {
            // 不转换，直接赋值
            targetMp.cp[arrayIndex].x = p[0]; targetMp.cp[arrayIndex].y = p[1]; targetMp.cp[arrayIndex].z = p[2];
            // 严格使用表格中读取到的 A, B, C 数据
            targetMp.cp[arrayIndex].a = p[3]; targetMp.cp[arrayIndex].b = p[4]; targetMp.cp[arrayIndex].c = p[5];
            targetMp.cp[arrayIndex].cfgx = currentPos.cfgx; targetMp.cp[arrayIndex].cfg1 = currentPos.cfg1;
            targetMp.cp[arrayIndex].cfg4 = currentPos.cfg4; targetMp.cp[arrayIndex].cfg6 = currentPos.cfg6;
            return true;
        }
    };

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    // ====================================================================
    // 第一阶段：全量预计算（内存囤货），发现任何不可达点立刻阻断
    // ====================================================================
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(mp));

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        if (r == 0) mp.moveType = 1;

        mp.speed = m_table->item(r, 8)->text().toDouble();
        mp.acc = m_table->item(r, 9)->text().toDouble();
        mp.dec = m_table->item(r, 10)->text().toDouble();
        mp.jerk = 100.0;
        mp.auxOverlapping = 1e100;
        mp.flags = 0;

        if (r == rowCount - 1) mp.overlapping = 0;
        else mp.overlapping = m_table->item(r, 11)->text().toDouble();

        if (mp.moveType == 3) {
            if (r + 1 >= rowCount) {
                QMessageBox::warning(this, "轨迹错误", "发现不完整的圆弧指令（缺少终点）！");
                m_startBtn->setEnabled(true); return;
            }
            if (!solveRow(r, mp, 0) || !solveRow(r + 1, mp, 1)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行圆弧点逆解不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
            r++; // 跳过已消耗的终点行
        }
        // 处理专属的整圆指令
        else if (mp.moveType == 4) {
            if (r + 3 >= rowCount) {
                QMessageBox::warning(this, "轨迹错误", "发现不完整的整圆指令（缺少参数点）！");
                m_startBtn->setEnabled(true); return;
            }
            // 整圆指令需要一口气吃下连续的4行点位
            if (!solveRow(r, mp, 0) || !solveRow(r + 1, mp, 1) || !solveRow(r + 2, mp, 2) || !solveRow(r + 3, mp, 3)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行整圆特征点逆解不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }

            // 利用叉积全自动计算画圆的方向 (flags 位1)
            double x0 = mp.cp[0].x, y0 = mp.cp[0].y;
            double x1 = mp.cp[1].x, y1 = mp.cp[1].y;
            double x2 = mp.cp[2].x, y2 = mp.cp[2].y;
            // 数学二维向量叉积判断旋向
            double cross = (x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1);

            if (cross < 0) {
                mp.flags = 2;
            } else {
                mp.flags = 0;
            }

            mp.overlapping = 0.0;
            r += 3;
        }
        else {
            if (!solveRow(r, mp, 0)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行点位逆解不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
        }
        mps.push_back(mp);
    }

    if (mps.empty()) {
        m_startBtn->setEnabled(true);
        m_statusLabel->setText("轨迹为空！");
        return;
    }

    setBlockMoveRunning(true);
    m_blockMoveStopRequested = false;
    m_resetAfterBlockStop = false;
    m_statusLabel->setText(QString("解算完毕，共 %1 个动作，开启动态滑动窗口...").arg(mps.size()));

    unsigned int startSpeedRatio = m_speedRatioSpinBox->value();
    m_blockMoveThread = QThread::create([this, mps, devId = m_devId, selTool, motionWobj, startSpeedRatio]() mutable {

        RobotAPI::MultiMove2Reset(devId);
        QThread::msleep(50);

        if (!selTool.empty()) RobotAPI::SetCurrentToolByName(selTool, devId);
        if (!motionWobj.empty()) RobotAPI::SetCurrentUframeByName(motionWobj, devId);

        RobotAPI::SetCurrentStepMode(ROBOX_MODE_CONTINUOUS, devId);
        RobotAPI::SetGlobalSpeed(startSpeedRatio, devId);
        QThread::msleep(50);

        int totalPoints = mps.size();
        int sentIndex = 0;

        while (sentIndex < totalPoints) {
            // 如果用户点击了“停止”按钮
            if (m_blockMoveStopRequested) {
                RobotAPI::MultiMove2Reset(devId);
                break;
            }

            // 在执行过程中实时监测报警状态，若发现报警立马暂停并重置！
            bool hasAlarm = false;
            if (RobotAPI::GetCurrentAlarmStatus(hasAlarm, devId) == 0 && hasAlarm) {
                RobotAPI::MultiMove2Hold(devId);   // 发送暂停指令
                RobotAPI::MultiMove2Reset(devId);  // 强制重置清空剩余轨迹队列

                QMetaObject::invokeMethod(this, [this]() {
                    m_statusLabel->setText("机器人在运行中出现报警，程序已自动暂停并重置！");
                    m_statusLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
                }, Qt::QueuedConnection);

                break; // 跳出滑动窗口，停止一切发送行为
            }

            int chunkCount = std::min(3, totalPoints - sentIndex);
            std::vector<RobotAPI::MultiMoveInfo2> chunk(mps.begin() + sentIndex, mps.begin() + sentIndex + chunkCount);

            int ret = RobotAPI::MultiMove2Start(chunk, devId);

            if (ret == 0) {
                // 只有底层明确返回 0 (成功吃进缓冲区)，我们才允许推进索引
                sentIndex += chunkCount;
                QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
                    m_statusLabel->setText(QString("滑动窗口持续喂点中: %1 / %2").arg(sentIndex).arg(totalPoints));
                }, Qt::QueuedConnection);

            } else if (ret == 40 || ret == 14) {
                // 状态 40 代表底层缓冲区已满，这是正常的物理消化过程，无需干预，静默等待重传
            } else {
                // 发生异常（包括 SDK 报错、超时、离线等），立刻中断发送并翻译错误码！
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
                // case 14: errMsg = "输入参数范围有误"; break;
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

            // 休眠 30 毫秒：匹配控制器的插补消化节奏
            QThread::msleep(30);
        }

        QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
            setBlockMoveRunning(false);
            m_startBtn->setEnabled(true);
            m_blockMoveThread = nullptr;

            if (m_resetAfterBlockStop && m_devId != 0) {
                RobotAPI::MultiMove2Reset(m_devId);
            }

            if (m_blockMoveStopRequested) {
                m_statusLabel->setText("已手动中止运行。");
            } else if (sentIndex >= totalPoints) {
                m_statusLabel->setText("轨迹已全量喂入控制器，等待物理动作执行结束...");
            }
        }, Qt::QueuedConnection);
    });

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
    double SAFE_HEIGHT = 50.0;

    if (m_platePosCombo && m_thicknessSpin) {
        if (m_platePosCombo->currentIndex() == 0) plateZ = m_thicknessSpin->value();
        else plateZ = -m_thicknessSpin->value();
        SAFE_HEIGHT = (plateZ > 0) ? (plateZ + 50.0) : 50.0;
    }

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
            targetPoints << c.points[0];         targetMoveTypes << 4; targetOrigIdx << 0;       targetRemarks << "-整圆起刀点(P1)";
            targetPoints << c.points[n / 4];     targetMoveTypes << 4; targetOrigIdx << n / 4;   targetRemarks << "-整圆途经点(P2)";
            targetPoints << c.points[n / 2];     targetMoveTypes << 4; targetOrigIdx << n / 2;   targetRemarks << "-整圆交接点(P3)";
            targetPoints << c.points[3 * n / 4]; targetMoveTypes << 4; targetOrigIdx << 3*n / 4; targetRemarks << "-整圆收刀点(P4)";
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
                    if (std::abs(angDiff) > 0.5) {
                        double pTurn[6] = { globalLastUcsPt.x(), globalLastUcsPt.y(), plateZ, finalA, B, 0.0 };
                        // 连续直线 (Lin)
                        addRow(2, 2, pTurn, 50, 50, 50, 0.0, shapeName + remark);
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
                    double pSafe[6] = { ucsPt.x(), ucsPt.y(), SAFE_HEIGHT, finalA, 0.0, 0.0 };
                    addRow(2, 2, pSafe, 100, 50, 50, 0.0, shapeName + remark + " [跨域: 高空就位]");
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
                    addRow(2, 2, pRetract, 100, 50, 50, 0.0, shapeName + " [跨域 抬刀]");
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
