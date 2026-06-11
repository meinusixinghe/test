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
#include <QSpinBox>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ====================================================================
// 构造函数：解析线条序列并生成运动程序表格
// ====================================================================
TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, QWidget *parent)
    : QDialog(parent), m_devId(devId), m_paths(paths), m_ucs(ucs)
{
    setWindowTitle("任务程序运行控制台 (极简纯净直线 + 关节跟踪解卷绕)");
    setMinimumSize(1100, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QGroupBox* tableGroup = new QGroupBox("运行轨迹程序 (相对于用户坐标系 UCS)", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    // ==========================================================
    // 【多坐标系选择 UI】
    // ==========================================================
    QHBoxLayout* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel("加工几何基准:", this));
    m_coordCombo = new QComboBox(this);
    m_coordCombo->addItem("默认基座坐标系 (绝对坐标)", 0);
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

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        try {
            std::vector<std::string> toolNames, wobjNames;
            if (RobotAPI::GetToolNameList(toolNames, m_devId) == 0) {
                for (const auto& name : toolNames) m_robotToolCombo->addItem(QString::fromStdString(name));
            }
            if (RobotAPI::GetUserNameList(wobjNames, m_devId) == 0) {
                for (const auto& name : wobjNames) m_robotUserCombo->addItem(QString::fromStdString(name));
            }
            std::string curTool, curWobj;
            if (RobotAPI::GetCurrentToolName(curTool, m_devId) == 0) m_robotToolCombo->setCurrentText(QString::fromStdString(curTool));
            if (RobotAPI::GetCurrentUframeName(curWobj, m_devId) == 0) m_robotUserCombo->setCurrentText(QString::fromStdString(curWobj));
        } catch (...) {}
    }

    m_table = new QTableWidget(0, 13, this);
    m_table->setHorizontalHeaderLabels({"插补模式", "坐标类型", "X", "Y", "Z", "A(切向绕Z)", "B(俯仰)", "C(绕X)", "速度", "加速", "减速", "平滑度", "备注说明"});
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

    // ==========================================================
    // 状态与全局倍率区
    // ==========================================================
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
        if (RobotAPI::GetCurrentSpeedRatio(curRatio, m_devId) == 0) m_speedRatioSpinBox->setValue(curRatio);
        else m_speedRatioSpinBox->setValue(20);
    } else {
        m_speedRatioSpinBox->setValue(20);
    }

    connect(m_speedRatioSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) RobotAPI::SetGlobalSpeed(value, m_devId);
    });

    m_startBtn = new QPushButton("▶ 启动连续加工", this);
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
    posCombo->addItems({"1: Joint数据", "2: Cart(UCS数据)"});
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
void TaskProgramDialog::onRemoveRowClicked() { if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow()); }
void TaskProgramDialog::onSyncPosClicked() {
    if (m_devId == 0) return;
    RobotAPI::RobotPos pd;
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    if (useUcs) {
        if (RobotAPI::GetUserCoordinatePos2(pd, m_devId) == 0) {
            double p[6] = {pd.x, pd.y, pd.z, pd.a, pd.b, pd.c};
            addRow(2, 2, p, 100, 50, 50, 0);
        }
    } else {
        if (RobotAPI::GetBaseCoordinatePos2(pd, m_devId) == 0) {
            double p[6] = {pd.x, pd.y, pd.z, pd.a, pd.b, pd.c};
            addRow(2, 2, p, 100, 50, 50, 0);
        }
    }
}

// ====================================================================
// 生成阶段：回归极简，全部用直线(MoveL)切入，A角规范在 ±180° 内！
// ====================================================================
void TaskProgramDialog::generateProgram()
{
    m_table->setRowCount(0);
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    const double SAFE_HEIGHT = 50.0; // 安全高度 (Z = 50mm)

    RobotAPI::RobotPos currentPose;
    memset(&currentPose, 0, sizeof(currentPose));
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        if (useUcs) RobotAPI::GetUserCoordinatePos2(currentPose, m_devId);
        else RobotAPI::GetBaseCoordinatePos2(currentPose, m_devId);
    }

    QPointF lastEndPos(-99999.0, -99999.0);

    for (int idx = 0; idx < m_paths.size(); ++idx) {
        const Contour& c = m_paths[idx];
        if (c.points.isEmpty()) continue;

        double rootFace = c.rootFace;
        QString typeStr = c.type;
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(typeStr);

        QVector<QPointF> targetPoints;
        QVector<int> targetMoveTypes;
        QVector<QString> targetRemarks;

        int n = c.points.size();
        bool isCircle = typeStr.contains("圆") && !typeStr.contains("弧") && !typeStr.contains("角");
        bool isFittedData = typeStr.contains("拟合") || typeStr.contains("样条") || typeStr.contains("Spline", Qt::CaseInsensitive);
        bool isArc = typeStr.contains("弧") || typeStr.contains("Arc", Qt::CaseInsensitive);

        // 🚨 极简 30 行数据拆解：精准识别圆弧、样条，绝不拟合碎直线！
        if (isCircle && n >= 4) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-起点";
            targetPoints << c.points[n / 4];     targetMoveTypes << 3; targetRemarks << "-圆弧1途点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-圆弧1终点";
            targetPoints << c.points[3 * n / 4]; targetMoveTypes << 3; targetRemarks << "-圆弧2途点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-圆弧2终点";
        } else if (isFittedData && n >= 3) {
            targetPoints << c.points[0]; targetMoveTypes << 2; targetRemarks << "-样条起点";
            for (int i = 1; i < n - 1; i += 2) {
                int segIdx = (i + 1) / 2;
                QPointF p1 = c.points[i-1], p2 = c.points[i], p3 = c.points[i+1];
                double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));
                if (std::abs(D) < 1e-6) {
                    targetPoints << p3; targetMoveTypes << 2; targetRemarks << QString("-段%1 直线终点").arg(segIdx);
                } else {
                    targetPoints << p2; targetMoveTypes << 3; targetRemarks << QString("-段%1 弧途点").arg(segIdx);
                    targetPoints << p3; targetMoveTypes << 3; targetRemarks << QString("-段%1 弧终点").arg(segIdx);
                }
            }
            if (n % 2 == 0) { targetPoints << c.points[n - 1]; targetMoveTypes << 2; targetRemarks << "-尾部收尾"; }
        } else if (isArc && n >= 3) {
            targetPoints << c.points[0];         targetMoveTypes << 2; targetRemarks << "-起点";
            targetPoints << c.points[n / 2];     targetMoveTypes << 3; targetRemarks << "-途经点";
            targetPoints << c.points[n - 1];     targetMoveTypes << 3; targetRemarks << "-终点";
        } else {
            for (int i = 0; i < n; ++i) {
                targetPoints << c.points[i]; targetMoveTypes << 2;
                targetRemarks << ((i == 0) ? "-起点" : (i == n - 1 ? "-终点" : QString("-途点%1").arg(i)));
            }
        }

        bool isConnectedWithNext = false;
        if (idx + 1 < m_paths.size() && !m_paths[idx + 1].points.isEmpty()) {
            QPointF nextStart = m_paths[idx + 1].points.first();
            if (std::hypot(targetPoints.last().x() - nextStart.x(), targetPoints.last().y() - nextStart.y()) < 0.001) isConnectedWithNext = true;
        }

        double initialTangentAngle = 0.0;
        if (targetPoints.size() > 1) {
            QPointF t0 = targetPoints[1] - targetPoints[0];
            initialTangentAngle = std::atan2(t0.y(), t0.x()) * 180.0 / M_PI;
        }

        for (int i = 0; i < targetPoints.size(); ++i) {
            QPointF pt = targetPoints[i];
            int moveType = targetMoveTypes[i];
            QString baseRemark = shapeName + targetRemarks[i];

            if (i == 0 && std::hypot(pt.x() - lastEndPos.x(), pt.y() - lastEndPos.y()) < 0.001) continue;

            QPointF tangent(0, 0);
            if (targetPoints.size() > 1) {
                if (i == 0) tangent = targetPoints[1] - targetPoints[0];
                else if (i == targetPoints.size() - 1) tangent = targetPoints[i] - targetPoints[i-1];
                else tangent = targetPoints[i+1] - targetPoints[i-1];
            }

            double len = std::hypot(tangent.x(), tangent.y());
            QPointF normal(0, 0);
            double currentTangentAngle = initialTangentAngle;

            if (len > 1e-6) {
                normal = QPointF(-tangent.y() / len, tangent.x() / len);
                currentTangentAngle = std::atan2(tangent.y(), tangent.x()) * 180.0 / M_PI;
            }

            pt.setX(pt.x() + normal.x() * rootFace);
            pt.setY(pt.y() + normal.y() * rootFace);

            if (useUcs && m_ucs.valid) {
                QPointF v = pt - m_ucs.origin;
                double local_x = v.x() * m_ucs.xAxis.x() + v.y() * m_ucs.xAxis.y();
                double local_y = v.x() * m_ucs.yAxis.x() + v.y() * m_ucs.yAxis.y();
                pt = QPointF(local_x, local_y);
            }

            // 🎯 计算姿态：完美遵守底层 [-180, 180] 规则，不再无限累加！
            double deltaA = currentTangentAngle - initialTangentAngle;
            while (deltaA > 180.0) deltaA -= 360.0;
            while (deltaA <= -180.0) deltaA += 360.0;

            double finalA = currentPose.a + deltaA;
            double finalB = currentPose.b;
            double finalC = currentPose.c;

            while (finalA > 180.0) finalA -= 360.0;
            while (finalA <= -180.0) finalA += 360.0;

            double speed = 50.0;

            // 🚨 全员直线 (moveType = 2)
            if (i == 0) {
                double pSafe[6] = { pt.x(), pt.y(), SAFE_HEIGHT, finalA, finalB, finalC };
                addRow(2, 2, pSafe, 100, 50, 50, 0.0, baseRemark + " [直线飞入:安全空走]");

                double pStart[6] = { pt.x(), pt.y(), 0.0, finalA, finalB, finalC };
                addRow(2, 2, pStart, 30, 50, 50, 0.0, baseRemark + " [直线下探:起刀]");
            }
            else if (i == targetPoints.size() - 1) {
                double pEnd[6] = { pt.x(), pt.y(), 0.0, finalA, finalB, finalC };
                double overlap = isConnectedWithNext ? 2.0 : 0.0;
                addRow(moveType, 2, pEnd, speed, 50, 50, overlap, baseRemark + (isConnectedWithNext ? "(无缝)" : ""));

                if (!isConnectedWithNext) {
                    double pRetract[6] = { pt.x(), pt.y(), SAFE_HEIGHT, finalA, finalB, finalC };
                    addRow(2, 2, pRetract, 80, 50, 50, 0.0, baseRemark + " [直线抬刀:安全退出]");
                }
            }
            else {
                double p[6] = { pt.x(), pt.y(), 0.0, finalA, finalB, finalC };
                double distToPrev = std::hypot(pt.x() - targetPoints[i-1].x(), pt.y() - targetPoints[i-1].y());
                double overlap = std::min(2.0, distToPrev * 0.4);
                addRow(moveType, 2, p, speed, 50, 50, overlap, baseRemark);
            }

            lastEndPos = targetPoints[i];
        }
    }
}

// ====================================================================
// 执行阶段：【关节解卷绕跟踪】算法 —— 从底层数学逻辑上绝杀 Error 3！
// ====================================================================
void TaskProgramDialog::onStartClicked() {
    int rowCount = m_table->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);
    const std::string motionWobj = useUcs ? selWobj : std::string("wobj0");

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("正在利用物理关节跟踪器消灭姿态跳变...");
    QApplication::processEvents();

    // 🌟 1. 获取物理机器人当前真实的关节角度，作为平滑基准
    RobotAPI::PosData curPd;
    RobotAPI::RobotJoint trackerJoints;
    memset(&trackerJoints, 0, sizeof(trackerJoints));
    if (RobotAPI::GetPositionData(curPd, m_devId) == 0) {
        for(int i=0; i<6; i++) trackerJoints.j[i] = curPd.acsPos[i];
    }

    // 🌟 2. 核心跟踪器：强制解卷绕，确保连续点之间关节角差值不会超过 180°
    auto solveRow = [&](int row, RobotAPI::MultiMoveInfo2& targetMp, int arrayIndex) -> bool {
        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(row, i + 2)->text().toDouble();

        RobotAPI::RobotPos localP;
        memset(&localP, 0, sizeof(localP));
        localP.x = p[0]; localP.y = p[1]; localP.z = p[2];
        localP.a = p[3]; localP.b = p[4]; localP.c = p[5];

        RobotAPI::RobotJoint tJoints;

        // 我们利用 IK 算出一个理论关节解
        if (RobotAPI::IkSolver(localP, tJoints, selTool, motionWobj, m_devId) == 0) {

            // 🚨 终极武器：物理关节解卷绕 (Joint Unwrapping)
            // 完美还原示教器现象！即使 A 角跳了 360 度，我们强行把关节 J4 和 J6 扭回来，保持平滑！
            for (int k = 0; k < 6; ++k) {
                while (tJoints.j[k] - trackerJoints.j[k] > 180.0)  tJoints.j[k] -= 360.0;
                while (tJoints.j[k] - trackerJoints.j[k] < -180.0) tJoints.j[k] += 360.0;
            }

            // 更新跟踪器
            trackerJoints = tJoints;

            // 拿着完全平滑、没有物理跳变的关节角，重新正解出完美的 Cartesian 和 CFG！
            RobotAPI::RobotPos finalP;
            if (RobotAPI::FkSolver(tJoints, finalP, selTool, motionWobj, m_devId) == 0) {

                targetMp.posType = 2; // 只需发送完美的笛卡尔坐标+CFG配置
                targetMp.cp[arrayIndex] = finalP;
                return true;
            }
        }
        return false;
    };

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(mp));

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        mp.posType = 2; // 全员笛卡尔纯血数据

        mp.speed = m_table->item(r, 8)->text().toDouble();
        mp.acc = m_table->item(r, 9)->text().toDouble();
        mp.dec = m_table->item(r, 10)->text().toDouble();
        mp.overlapping = m_table->item(r, 11)->text().toDouble();
        mp.auxOverlapping = 1e100;
        mp.flags = 0;

        if (mp.moveType == 3) {
            if (r + 1 >= rowCount || !solveRow(r, mp, 0) || !solveRow(r + 1, mp, 1)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行圆弧逆解无法到达或物理扭曲过大！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
            r++;
        } else {
            if (!solveRow(r, mp, 0)) {
                QMessageBox::warning(this, "严重错误", QString("第 %1 行点位逆解无法到达！").arg(r+1));
                m_startBtn->setEnabled(true); return;
            }
        }
        mps.push_back(mp);
    }

    if (mps.empty()) { m_startBtn->setEnabled(true); return; }

    setBlockMoveRunning(true);
    m_blockMoveStopRequested = false;
    m_resetAfterBlockStop = false;
    m_statusLabel->setText(QString("解卷绕完成，共 %1 个平滑指令，开启 MultiMove 滑动窗口...").arg(mps.size()));

    unsigned int startSpeedRatio = m_speedRatioSpinBox->value();

    // ====================================================================
    // 单一引擎线程：全部通过 MultiMove2Start 顺滑下发
    // ====================================================================
    m_blockMoveThread = QThread::create([this, mps, devId = m_devId, selTool, motionWobj, startSpeedRatio]() mutable {

        RobotAPI::SelectRobot(devId);
        RobotAPI::GetPermission(devId); // 拿稳控制权
        QThread::msleep(20);

        RobotAPI::MultiMove2Reset(devId);
        QThread::msleep(50);

        RobotAPI::SetCurrentToolByName(selTool, devId);
        RobotAPI::SetCurrentUframeByName(motionWobj, devId);
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
                    m_statusLabel->setText(QString("🚀 滑动窗口持续喂点中: %1 / %2").arg(sentIndex).arg(totalPoints));
                }, Qt::QueuedConnection);
            } else if (ret < 0) {
                QMetaObject::invokeMethod(this, [this, ret]() {
                    m_statusLabel->setText(QString("网络断开，组合运动错误码: %1").arg(ret));
                }, Qt::QueuedConnection);
                break;
            }
            QThread::msleep(30);
        }

        QMetaObject::invokeMethod(this, [this, sentIndex, totalPoints]() {
            setBlockMoveRunning(false);
            if (m_startBtn) m_startBtn->setEnabled(true);
            m_blockMoveThread = nullptr;

            if (m_resetAfterBlockStop && m_devId != 0) RobotAPI::MultiMove2Reset(m_devId);

            if (m_blockMoveStopRequested) m_statusLabel->setText("已手动中止运行。");
            else if (sentIndex >= totalPoints) m_statusLabel->setText("🎉 轨迹点全量喂入完成，等待物理动作结束...");
        }, Qt::QueuedConnection);
    });

    connect(m_blockMoveThread, &QThread::finished, m_blockMoveThread, &QObject::deleteLater);
    m_blockMoveThread->start();
}

void TaskProgramDialog::onPauseClicked() {
    if (m_devId == 0) return;
    (void)QtConcurrent::run([this]() { RobotAPI::MultiMove2Hold(m_devId); });
    m_statusLabel->setText("程序已暂停 (Hold).");
}

void TaskProgramDialog::onResumeClicked() {
    if (m_devId == 0) return;
    (void)QtConcurrent::run([this]() { RobotAPI::MultiMove2Resume(m_devId); });
    m_statusLabel->setText("程序已恢复执行 (Resume).");
}

void TaskProgramDialog::onResetClicked() {
    if (m_devId == 0) return;
    m_blockMoveStopRequested = true;
    m_resetAfterBlockStop = true;
    (void)QtConcurrent::run([this]() { RobotAPI::MultiMove2Reset(m_devId); });
    m_statusLabel->setText("程序已重置/停止 (Reset).");
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
            if (state == 5) m_robotStateLabel->setStyleSheet("font-weight: bold; color: red; font-size: 14px;");
            else if (state == 2) m_robotStateLabel->setStyleSheet("font-weight: bold; color: green; font-size: 14px;");
            else m_robotStateLabel->setStyleSheet("font-weight: bold; color: #D84315; font-size: 14px;");
        } else {
            m_robotStateLabel->setText("底层状态: 读取失败");
        }
    }
}
