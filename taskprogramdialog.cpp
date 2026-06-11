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
    setWindowTitle("任务程序运行控制台 (MultiMove2 - Z轴跟随版)");
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
        try {
            std::vector<std::string> toolNames;
            if (RobotAPI::GetToolNameList(toolNames, m_devId) == 0) {
                for (const std::string& name : toolNames) {
                    m_robotToolCombo->addItem(QString::fromStdString(name));
                }
            }
        } catch (...) {}

        try {
            std::vector<std::string> wobjNames;
            if (RobotAPI::GetUserNameList(wobjNames, m_devId) == 0) {
                for (const std::string& name : wobjNames) {
                    m_robotUserCombo->addItem(QString::fromStdString(name));
                }
            }
        } catch (...) {}

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

    if (m_robotToolCombo->count() == 0) {
        for(int i=0; i<=31; i++) m_robotToolCombo->addItem(QString("tool%1").arg(i));
    }
    if (m_robotUserCombo->count() == 0) {
        for(int i=0; i<=31; i++) m_robotUserCombo->addItem(QString("wobj%1").arg(i));
    }

    m_table = new QTableWidget(0, 13, this);
    m_table->setHorizontalHeaderLabels({"插补模式", "坐标类型", "X", "Y", "Z", "RX(A)", "RY(B)", "RZ(C)", "速度", "加速", "减速", "平滑度", "备注说明"});
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
    m_speedRatioSpinBox->setRange(1, 100);
    m_speedRatioSpinBox->setSuffix(" %");
    m_speedRatioSpinBox->setFixedWidth(80);
    m_speedRatioSpinBox->setStyleSheet("QSpinBox { padding: 4px; font-weight: bold; font-size: 14px; border: 1px solid #aaa; border-radius: 3px; }");
    bottomLayout->addWidget(m_speedRatioSpinBox);
    bottomLayout->addSpacing(20);

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        unsigned int curRatio = 20;
        if (RobotAPI::GetCurrentSpeedRatio(curRatio, m_devId) == 0) {
            m_speedRatioSpinBox->setValue(curRatio);
        } else {
            m_speedRatioSpinBox->setValue(20);
        }
    } else {
        m_speedRatioSpinBox->setValue(20);
    }

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

    RobotAPI::RobotPos currentPos;
    memset(&currentPos, 0, sizeof(currentPos));
    if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPos, m_devId);
    else RobotAPI::GetBaseCoordinatePos2(currentPos, m_devId);

    auto solveRow = [&](int row, RobotAPI::MultiMoveInfo2& targetMp, int arrayIndex) -> bool {
        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 1));
        int posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(row, i + 2)->text().toDouble();

        if (posType == 2 && useUcs) {
            RobotAPI::RobotPos localP = currentPos;
            localP.x = p[0]; localP.y = p[1]; localP.z = p[2];
            // 🚨 将 UI 表格中计算好的随动姿态 ABC 注入逆解引擎！
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
            return false;
        } else {
            targetMp.cp[arrayIndex].x = p[0]; targetMp.cp[arrayIndex].y = p[1]; targetMp.cp[arrayIndex].z = p[2];
            targetMp.cp[arrayIndex].a = p[3]; targetMp.cp[arrayIndex].b = p[4]; targetMp.cp[arrayIndex].c = p[5];
            targetMp.cp[arrayIndex].cfgx = currentPos.cfgx; targetMp.cp[arrayIndex].cfg1 = currentPos.cfg1;
            targetMp.cp[arrayIndex].cfg4 = currentPos.cfg4; targetMp.cp[arrayIndex].cfg6 = currentPos.cfg6;
            return true;
        }
    };

    std::vector<RobotAPI::MultiMoveInfo2> mps;

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
            r++;
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
            if (m_blockMoveStopRequested) {
                RobotAPI::MultiMove2Reset(devId);
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

            } else if (ret < 0) {
                QMetaObject::invokeMethod(this, [this, ret]() {
                    m_statusLabel->setText(QString("致命错误：控制器通信断开，错误码 %1").arg(ret));
                }, Qt::QueuedConnection);
                break;
            }
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

// ====================================================================
// 核心改动：在 UCS 中提取轨迹切向角，叠加到原有的 A 角上，实现绕Z跟随
// ====================================================================
void TaskProgramDialog::generateProgram()
{
    m_table->setRowCount(0);
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);

    RobotAPI::RobotPos currentPose;
    currentPose.a = 0.0;
    currentPose.b = 0.0;
    currentPose.c = 0.0;

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        // 抓取基准姿态：后续的偏航计算都是叠加在这之上！
        if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPose, m_devId);
        else RobotAPI::GetBaseCoordinatePos2(currentPose, m_devId);
    }

    QPointF lastEndPos(-99999.0, -99999.0);

    for (int idx = 0; idx < m_paths.size(); ++idx) {
        const Contour& c = m_paths[idx];
        if (c.points.isEmpty()) continue;

        // 如果之前的代码有传递顿边参数，可以通过这里读取
        // double rootFace = c.rootFace;
        double rootFace = 0.0; // 若结构体无此变量请用默认0.0

        QString typeStr = c.type;
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(typeStr);

        QVector<QPointF> targetPoints;
        QVector<int> targetMoveTypes;
        QVector<QString> targetRemarks;

        int n = c.points.size();
        bool isFittedData = typeStr.contains("拟合") || typeStr.contains("样条") || typeStr.contains("Spline", Qt::CaseInsensitive);
        bool isCircle = typeStr.contains("圆") && !typeStr.contains("弧") && !typeStr.contains("角");
        bool isArc = typeStr.contains("弧") || typeStr.contains("Arc", Qt::CaseInsensitive);

        // --- 完整保留 30 条数据拆分逻辑 ---
        if (isCircle && n >= 4) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-起点(圆弧1开始)";
            targetPoints << c.points[n / 4];     targetMoveTypes << 3; targetRemarks << "-圆弧1途经点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-圆弧1终点(圆弧2开始)";
            targetPoints << c.points[3 * n / 4]; targetMoveTypes << 3; targetRemarks << "-圆弧2途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-圆弧2终点";
        }
        else if (isFittedData && n >= 3) {
            targetPoints << c.points[0]; targetMoveTypes << 2; targetRemarks << "-样条起点";
            for (int i = 1; i < n - 1; i += 2) {
                int segIdx = (i + 1) / 2;
                QPointF p1 = c.points[i-1];
                QPointF p2 = c.points[i];
                QPointF p3 = c.points[i+1];
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
        else if (isArc && n >= 3) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-圆弧起点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-圆弧途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-圆弧终点";
        }
        else {
            for (int i = 0; i < n; ++i) {
                targetPoints << c.points[i]; targetMoveTypes << 2;
                if (i == 0) targetRemarks << "-起点";
                else if (i == n - 1) targetRemarks << "-终点";
                else targetRemarks << QString("-途经点%1").arg(i);
            }
        }

        bool isConnectedWithNext = false;
        if (idx + 1 < m_paths.size() && !m_paths[idx + 1].points.isEmpty()) {
            QPointF nextStart = m_paths[idx + 1].points.first();
            QPointF myEnd = targetPoints.last();
            if (std::hypot(myEnd.x() - nextStart.x(), myEnd.y() - nextStart.y()) < 0.001) {
                isConnectedWithNext = true;
            }
        }

        // 🌟【新增跟随逻辑1】：计算出这个轮廓最开始的切向角，作为旋转的0度参考系
        double initialTangentAngle = 0.0;
        if (targetPoints.size() > 1) {
            QPointF t0 = targetPoints[1] - targetPoints[0];
            initialTangentAngle = std::atan2(t0.y(), t0.x()) * 180.0 / M_PI;
        }

        for (int i = 0; i < targetPoints.size(); ++i) {
            QPointF pt = targetPoints[i];
            int moveType = targetMoveTypes[i];
            QString baseRemark = shapeName + targetRemarks[i];

            bool isConnectedWithPrev = (std::hypot(pt.x() - lastEndPos.x(), pt.y() - lastEndPos.y()) < 0.001);
            if (i == 0 && isConnectedWithPrev) continue;

            // 🌟【新增跟随逻辑2】：提取当前点的前进切向向量
            QPointF tangent(0, 0);
            if (targetPoints.size() > 1) {
                if (i == 0) tangent = targetPoints[1] - targetPoints[0];
                else if (i == targetPoints.size() - 1) tangent = targetPoints[i] - targetPoints[i-1];
                else tangent = targetPoints[i+1] - targetPoints[i-1];
            }

            double len = std::hypot(tangent.x(), tangent.y());
            QPointF normal(0, 0);
            double currentTangentAngle = initialTangentAngle;

            // 算出法向量(用于顿边平移) 以及当前的偏航角
            if (len > 1e-6) {
                normal = QPointF(-tangent.y() / len, tangent.x() / len);
                currentTangentAngle = std::atan2(tangent.y(), tangent.x()) * 180.0 / M_PI;
            }

            // 1. 如果有顿边厚度，把点沿着法线向内/外侧偏移
            pt.setX(pt.x() + normal.x() * rootFace);
            pt.setY(pt.y() + normal.y() * rootFace);

            // 2. 将点转换为相对坐标
            if (useUcs && m_ucs.valid) {
                QPointF v = pt - m_ucs.origin;
                double local_x = v.x() * m_ucs.xAxis.x() + v.y() * m_ucs.xAxis.y();
                double local_y = v.x() * m_ucs.yAxis.x() + v.y() * m_ucs.yAxis.y();
                pt = QPointF(local_x, local_y);
            }

            // 🌟【新增跟随逻辑3】：将切线夹角补偿到机器人的 A角 上，实现绕Z轴的转向跟随！
            double deltaA = currentTangentAngle - initialTangentAngle;

            // 安全限制：防止跨越 180° 时产生畸变甩尾
            while (deltaA > 180.0) deltaA -= 360.0;
            while (deltaA <= -180.0) deltaA += 360.0;

            double finalA = currentPose.a + deltaA;
            while (finalA > 180.0) finalA -= 360.0;
            while (finalA <= -180.0) finalA += 360.0;

            // 构造 6 自由度数组：[X, Y, Z, A, B, C]
            // 注意：因为不考虑坡口，B 和 C 完全继承当前示教的固定角度 currentPose.b/c
            double p[6] = { pt.x(), pt.y(), 0.0, finalA, currentPose.b, currentPose.c };

            double overlap = 0.0;
            double speed = 50.0;

            if (i == 0) {
                speed = 100.0;
                overlap = 0.0;
                baseRemark += "(空走跳转)";
            }
            else if (i == targetPoints.size() - 1) {
                if (isConnectedWithNext) {
                    overlap = 2.0;
                    baseRemark += "(无缝衔接下个)";
                } else {
                    overlap = 0.0;
                    baseRemark += "(加工结束抬刀)";
                }
            } else {
                overlap = 2.0;
            }

            addRow(moveType, 2, p, speed, 50, 50, overlap, baseRemark);
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
