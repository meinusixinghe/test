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

// ====================================================================
// 构造函数：解析线条序列并生成运动程序表格
// ====================================================================
TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, QWidget *parent)
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
    for(int i=0; i<=31; i++) m_robotToolCombo->addItem(QString("tool%1").arg(i));
    coordLayout->addWidget(m_robotToolCombo);

    coordLayout->addSpacing(10);
    coordLayout->addWidget(new QLabel("机器人 Wobj:", this));
    m_robotUserCombo = new QComboBox(this);
    m_robotUserCombo->setEditable(true);
    for(int i=0; i<=31; i++) m_robotUserCombo->addItem(QString("wobj%1").arg(i));
    coordLayout->addWidget(m_robotUserCombo);
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
        for(int i=0; i<=31; i++) m_robotToolCombo->addItem(QString("tool%1").arg(i));
    }
    if (m_robotUserCombo->count() == 0) {
        for(int i=0; i<=31; i++) m_robotUserCombo->addItem(QString("wobj%1").arg(i));
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
    m_robotStateLabel = new QLabel("底层状态: 获取中...", this);
    m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
    bottomLayout->addWidget(m_robotStateLabel);

    bottomLayout->addSpacing(20);

    m_statusLabel = new QLabel("状态: 程序已生成，准备就绪...", this);
    m_statusLabel->setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;");
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

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
    if (RobotAPI::GetPositionData(pd, m_devId) == 0) {
        double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
        addRow(2, 2, p, 100, 50, 50, 0);
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

    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    const std::string motionWobj = useUcs ? selWobj : std::string("wobj0");

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("正在进行离线全量运动学逆解...");
    QApplication::processEvents();

    // 1. 提取当前物理坐标配置
    RobotAPI::RobotPos currentPos;
    memset(&currentPos, 0, sizeof(currentPos));
    if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPos, m_devId);
    else RobotAPI::GetBaseCoordinatePos2(currentPos, m_devId);

    // ====================================================================
    // 💡 辅助工具：提取表格指定行的数据，并完成安全的逆解/正解
    // ====================================================================
    auto solveRow = [&](int row, RobotAPI::MultiMoveInfo2& targetMp, int arrayIndex) -> bool {
        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 1));
        int posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(row, i + 2)->text().toDouble();

        if (posType == 2 && useUcs) {
            RobotAPI::RobotPos localP = currentPos;
            localP.x = p[0]; localP.y = p[1]; localP.z = p[2];

            RobotAPI::RobotJoint tJoints; memset(&tJoints, 0, sizeof(tJoints));
            // 🚨 全量离线预解算：确保轨迹 100% 可达
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
            targetMp.cp[arrayIndex].a = currentPos.a; targetMp.cp[arrayIndex].b = currentPos.b; targetMp.cp[arrayIndex].c = currentPos.c;
            targetMp.cp[arrayIndex].cfgx = currentPos.cfgx; targetMp.cp[arrayIndex].cfg1 = currentPos.cfg1;
            targetMp.cp[arrayIndex].cfg4 = currentPos.cfg4; targetMp.cp[arrayIndex].cfg6 = currentPos.cfg6;
            return true;
        }
    };

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    // ====================================================================
    // 🚨 第一阶段：全量预计算（内存囤货），发现任何不可达点立刻阻断
    // ====================================================================
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(mp)); // 安全清零

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        // 强制第一点为关节运动，防止过大跨度导致规划器死锁
        if (r == 0) mp.moveType = 1;

        mp.speed = m_table->item(r, 8)->text().toDouble();
        mp.acc = m_table->item(r, 9)->text().toDouble();
        mp.dec = m_table->item(r, 10)->text().toDouble();

        // 强制收尾点平滑度为 0
        if (r == rowCount - 1) mp.overlapping = 0;
        else mp.overlapping = m_table->item(r, 11)->text().toDouble();

        mp.auxOverlapping = 1e100;
        mp.flags = 0;

        if (mp.moveType == 3) {
            if (r + 1 >= rowCount) {
                QMessageBox::warning(this, "轨迹错误", "发现不完整的圆弧指令（缺少终点）！");
                m_startBtn->setEnabled(true);
                return;
            }
            if (!solveRow(r, mp, 0) || !solveRow(r + 1, mp, 1)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行圆弧点逆解不可达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
            r++; // 跳过已消耗的终点行
        } else {
            if (!solveRow(r, mp, 0)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行直线点位逆解不可达！").arg(r+1));
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

    // ====================================================================
    // 🚨 第二阶段：纯血版滑动窗口子线程（绝不混用任何 Block API）
    // ====================================================================
    m_blockMoveThread = QThread::create([this, mps, devId = m_devId, selTool, motionWobj]() mutable {

        // 💡 清除原有遗留的 BlockMultiMoveReset，严格使用 MultiMove2 系列重置
        RobotAPI::MultiMove2Reset(devId);
        QThread::msleep(50);

        if (!selTool.empty()) RobotAPI::SetCurrentToolByName(selTool, devId);
        if (!motionWobj.empty()) RobotAPI::SetCurrentUframeByName(motionWobj, devId);

        // 💡 清除旧的 prepareBlockMultiMoveStart 封装调用，直接使用核心 API 确保连续运行和倍率
        RobotAPI::SetCurrentStepMode(ROBOX_MODE_CONTINUOUS, devId);
        RobotAPI::SetGlobalSpeed(10, devId);
        QThread::msleep(50);

        int totalPoints = mps.size();
        int sentIndex = 0;

        while (sentIndex < totalPoints) {
            // 如果用户点击了“停止”按钮 (复用了你头文件里的变量名)
            if (m_blockMoveStopRequested) {
                RobotAPI::MultiMove2Reset(devId);
                break;
            }

            // 每次只切取 3 个点作为小包发送，细水长流
            int chunkCount = std::min(3, totalPoints - sentIndex);
            std::vector<RobotAPI::MultiMoveInfo2> chunk(mps.begin() + sentIndex, mps.begin() + sentIndex + chunkCount);

            // 🎯 唯一负责下发轨迹的 API，绝不触碰 BlockMultiMove
            int ret = RobotAPI::MultiMove2Start(chunk, devId);

            if (ret == 0) {
                // 只有底层明确返回 0 (成功吃进缓冲区)，我们才允许推进索引
                sentIndex += chunkCount;
                QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
                    m_statusLabel->setText(QString("滑动窗口持续喂点中: %1 / %2").arg(sentIndex).arg(totalPoints));
                }, Qt::QueuedConnection);

            } else if (ret < 0) {
                // 致命通信断开 (-48, -59 等)，强行跳出
                QMetaObject::invokeMethod(this, [this, ret]() {
                    m_statusLabel->setText(QString("致命错误：控制器通信断开，错误码 %1").arg(ret));
                }, Qt::QueuedConnection);
                break;
            } else {
                // 底层满载拒收 (例如返回 40)
                // 【绝对不增加 sentIndex】，线程静默休眠，等机器人消化掉一部分后再重传。
            }

            // 休眠 30 毫秒：匹配控制器的插补消化节奏
            QThread::msleep(30);
        }

        // 推送循环结束，收尾工作
        QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
            setBlockMoveRunning(false);
            m_startBtn->setEnabled(true);
            m_blockMoveThread = nullptr;

            // 💡 清除原有遗留的 BlockMultiMoveReset，严格使用 MultiMove2 系列重置
            if (m_resetAfterBlockStop && m_devId != 0) {
                RobotAPI::MultiMove2Reset(m_devId);
            }

            if (m_blockMoveStopRequested) {
                m_statusLabel->setText("已手动中止运行。");
            } else if (sentIndex >= totalPoints) {
                m_statusLabel->setText("🎉 轨迹已全量喂入控制器，等待物理动作执行结束...");
            }
        }, Qt::QueuedConnection);
    });

    connect(m_blockMoveThread, &QThread::finished, m_blockMoveThread, &QObject::deleteLater);
    m_blockMoveThread->start();
}

void TaskProgramDialog::onPauseClicked() {
    if (m_devId == 0) return;
    // 👇 前面加上 (void) 强转，消除警告
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
        RobotAPI::GetBaseCoordinatePos(currentPose, m_devId);
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

            // 如果出错，可以用红色警示
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
