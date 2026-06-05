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
void TaskProgramDialog::onStartClicked() {
    int rowCount = m_table->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    // 获取下拉框选中的 Tool 和 Wobj
    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();

    // 判断界面上是否选择了用户坐标系(UCS)
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);

    // =================================================================
    // 🚨 步骤 1：在循环外部，提前获取用户坐标系(wobj)的真实物理偏移矩阵
    // =================================================================
    QMatrix4x4 matWobj;
    if (useUcs) {
        RobotAPI::RobotWorkpiece wobjValue;
        memset(&wobjValue, 0, sizeof(RobotAPI::RobotWorkpiece));

        int ret = RobotAPI::GetUserCoordinate(selWobj, wobjValue, m_devId);
        if (ret == 0) {
            // 构建 Wobj (用户坐标系) 的齐次变换矩阵
            matWobj.translate(wobjValue.x, wobjValue.y, wobjValue.z);
            // 工业标准欧拉角旋转 (Z - Y - X)
            matWobj.rotate(wobjValue.c, 0, 0, 1); // RZ
            matWobj.rotate(wobjValue.b, 0, 1, 0); // RY
            matWobj.rotate(wobjValue.a, 1, 0, 0); // RX
        } else {
            QMessageBox::critical(this, "读取失败", QString("无法读取用户坐标系 [%1]！错误码: %2").arg(QString::fromStdString(selWobj)).arg(ret));
            return;
        }
    }

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(r, i + 2)->text().toDouble();

        // =================================================================
        // 🚨 步骤 2：纯数学空间坐标系映射 (位置 X/Y/Z + 姿态 A/B/C)
        // =================================================================
        if (mp.posType == 2 && useUcs) {

            // 1. 构建 Local (图纸/局部相对坐标) 的齐次变换矩阵
            QMatrix4x4 matLocal;
            matLocal.translate(p[0], p[1], p[2]);
            matLocal.rotate(p[5], 0, 0, 1); // 局部 C
            matLocal.rotate(p[4], 0, 1, 0); // 局部 B
            matLocal.rotate(p[3], 1, 0, 0); // 局部 A

            // 2. 核心数学运算：Base矩阵 = Wobj矩阵 × Local矩阵
            QMatrix4x4 matBase = matWobj * matLocal;

            // 3. 提取基座绝对位置 (X, Y, Z)
            p[0] = matBase.column(3).x();
            p[1] = matBase.column(3).y();
            p[2] = matBase.column(3).z();

            // 4. 提取基座绝对姿态 (欧拉角 A, B, C 反解)
            float r11 = matBase(0,0), r21 = matBase(1,0), r31 = matBase(2,0);
            float r32 = matBase(2,1), r33 = matBase(2,2);

            // 限制反三角函数范围，防止浮点精度越界导致 NaN
            float val = -r31;
            if (val > 1.0f) val = 1.0f;
            if (val < -1.0f) val = -1.0f;

            double b_rad = asin(val);
            double a_rad = atan2(r32, r33);
            double c_rad = atan2(r21, r11);

            // 弧度转角度，覆盖回 p 数组
            p[3] = a_rad * 180.0 / M_PI;
            p[4] = b_rad * 180.0 / M_PI;
            p[5] = c_rad * 180.0 / M_PI;
        }

        // 把算好的绝对数据塞进结构体
        if (mp.posType == 1) {
            for(int i=0; i<6; i++) mp.ap[0].j[i] = p[i];
        } else {
            mp.cp[0].x = p[0]; mp.cp[0].y = p[1]; mp.cp[0].z = p[2];
            mp.cp[0].a = p[3]; mp.cp[0].b = p[4]; mp.cp[0].c = p[5];
        }

        mp.speed = m_table->item(r, 8)->text().toDouble();
        mp.acc = m_table->item(r, 9)->text().toDouble();
        mp.dec = m_table->item(r, 10)->text().toDouble();
        mp.overlapping = m_table->item(r, 11)->text().toDouble();

        mps.push_back(mp);
    }

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("正在下发...");

    // 下发给机器人的队列
    QThread* worker = QThread::create([this, mps, devId = m_devId, selTool, selWobj]() mutable {
        RobotAPI::MultiMove2Reset(devId);
        QThread::msleep(20);

        if (!selTool.empty()) RobotAPI::SetCurrentToolByName(selTool, devId);
        if (!selWobj.empty()) RobotAPI::SetCurrentUframeByName(selWobj, devId);
        QThread::msleep(50);

        int ret = RobotAPI::MultiMove2Start(mps, devId);

        QMetaObject::invokeMethod(this, [this, ret]() {
            m_startBtn->setEnabled(true);
            if (ret == 0) m_statusLabel->setText("执行中 (Running)...");
            else m_statusLabel->setText(QString("启动失败！错误码: %1").arg(ret));
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void TaskProgramDialog::onPauseClicked() {
    int ret = RobotAPI::MultiMove2Hold(m_devId);
    if (ret == 0) m_statusLabel->setText("程序已暂停 (Hold).");
}

void TaskProgramDialog::onResumeClicked() {
    int ret = RobotAPI::MultiMove2Resume(m_devId);
    if (ret == 0) m_statusLabel->setText("程序已恢复执行 (Resume).");
}

void TaskProgramDialog::onResetClicked() {
    RobotAPI::MultiMove2Reset(m_devId);
    RobotAPI::MOVECLEAR(m_devId); // 清空队列
    m_statusLabel->setText("程序已重置/停止 (Reset).");
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
