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
#include <cmath>
#include <QPointer>
#include <QDebug>

TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, QWidget *parent)
    : QDialog(parent), m_devId(devId), m_paths(paths), m_ucs(ucs)
{
    setWindowTitle("任务程序运行控制台 (MultiMove2)");
    setMinimumSize(1100, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* tableGroup = new QGroupBox("运行轨迹程序 (双击可手动修改坐标及速度参数)", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    QHBoxLayout* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel("使用坐标系:", this));
    m_coordCombo = new QComboBox(this);
    m_coordCombo->addItem("默认基座坐标系 (图纸绝对坐标)", 0);
    if (m_ucs.valid) {
        m_coordCombo->addItem("用户自定义坐标系 (UCS相对坐标)", 1);
        m_coordCombo->setCurrentIndex(1); // 默认优先选 UCS
    } else {
        m_coordCombo->addItem("用户自定义坐标系 (未建立)", 1);
        m_coordCombo->setItemData(1, QVariant(0), Qt::UserRole - 1); // 禁用该项
    }
    coordLayout->addWidget(m_coordCombo);

    coordLayout->addSpacing(20);
    coordLayout->addWidget(new QLabel("机器人工具(Tool):", this));
    m_robotToolCombo = new QComboBox(this);
    coordLayout->addWidget(m_robotToolCombo);
    coordLayout->addSpacing(10);
    coordLayout->addWidget(new QLabel("机器人用户(Wobj):", this));
    m_robotUserCombo = new QComboBox(this);
    coordLayout->addWidget(m_robotUserCombo);

    coordLayout->addStretch();
    tableLayout->addLayout(coordLayout);

    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        try {
            std::vector<std::string> toolList, userList;
            std::string curTool, curWobj;
            RobotAPI::GetToolNameList(toolList, m_devId);
            RobotAPI::GetUserNameList(userList, m_devId);
            RobotAPI::GetCurrentToolName(curTool, m_devId);
            RobotAPI::GetCurrentUframeName(curWobj, m_devId);

            for (const auto& t : toolList) m_robotToolCombo->addItem(QString::fromStdString(t));
            for (const auto& u : userList) m_robotUserCombo->addItem(QString::fromStdString(u));
            m_robotToolCombo->setCurrentText(QString::fromStdString(curTool));
            m_robotUserCombo->setCurrentText(QString::fromStdString(curWobj));
        } catch (...) {
            qDebug() << "获取 SDK 坐标系列表时发生内部异常，已安全拦截！";
        }
    }

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

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("状态: 准备就绪...", this);
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

    // 下拉框改变时重新生成数据
    connect(m_coordCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TaskProgramDialog::generateProgram);

    // 初始化生成
    generateProgram();
}

void TaskProgramDialog::generateProgram()
{
    m_table->setRowCount(0);
    bool useUcs = (m_coordCombo->currentData().toInt() == 1);

    RobotAPI::RobotPos currentPose;
    memset(&currentPose, 0, sizeof(RobotAPI::RobotPos));
    if (m_devId != 0 && RobotAPI::IsConnected(m_devId)) {
        RobotAPI::GetBaseCoordinatePos(currentPose, m_devId);
    }

    QPointF lastEndPos(-99999.0, -99999.0);

    for (int idx = 0; idx < m_paths.size(); ++idx) {
        const Contour& c = m_paths[idx];
        if (c.points.isEmpty()) continue;

        QString typeStr = c.type;
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(typeStr);

        QVector<QPointF> targetPoints;
        QVector<int> targetMoveTypes;
        QVector<QString> targetRemarks;

        int n = c.points.size();
        bool isFittedData = typeStr.contains("拟合") || typeStr.contains("样条") || typeStr.contains("Spline", Qt::CaseInsensitive);
        bool isCircle = typeStr.contains("圆") && !typeStr.contains("弧") && !typeStr.contains("角");
        bool isArc = typeStr.contains("弧") || typeStr.contains("Arc", Qt::CaseInsensitive);

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
                QPointF p1 = c.points[i-1], p2 = c.points[i], p3 = c.points[i+1];
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
            if (std::hypot(myEnd.x() - nextStart.x(), myEnd.y() - nextStart.y()) < 0.001) isConnectedWithNext = true;
        }

        for (int i = 0; i < targetPoints.size(); ++i) {
            QPointF pt = targetPoints[i];
            int moveType = targetMoveTypes[i];
            QString baseRemark = shapeName + targetRemarks[i];

            bool isConnectedWithPrev = (std::hypot(pt.x() - lastEndPos.x(), pt.y() - lastEndPos.y()) < 0.001);
            if (i == 0 && isConnectedWithPrev) continue;

            // 👇【核心映射】：如果是用户坐标系，执行空间平移矩阵转换
            if (useUcs && m_ucs.valid) {
                QPointF v = pt - m_ucs.origin;
                double local_x = v.x() * m_ucs.xAxis.x() + v.y() * m_ucs.xAxis.y();
                double local_y = v.x() * m_ucs.yAxis.x() + v.y() * m_ucs.yAxis.y();
                pt = QPointF(local_x, local_y);
            }

            double p[6] = { pt.x(), pt.y(), 0.0, currentPose.a, currentPose.b, currentPose.c };

            double overlap = 0.0; double speed = 50.0;
            if (i == 0) { speed = 100.0; overlap = 0.0; baseRemark += "(空走跳转)"; }
            else if (i == targetPoints.size() - 1) {
                if (isConnectedWithNext) { overlap = 2.0; baseRemark += "(无缝衔接)"; }
                else { overlap = 0.0; baseRemark += "(加工结束抬刀)"; }
            } else { overlap = 2.0; }

            addRow(moveType, 2, p, speed, 50, 50, overlap, baseRemark);
            lastEndPos = targetPoints[i]; // 真实 DXF 位置留存
        }
    }
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
void TaskProgramDialog::onRemoveRowClicked() { if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow()); }
void TaskProgramDialog::onSyncPosClicked() {
    if (m_devId == 0) return;
    RobotAPI::PosData pd;
    if (RobotAPI::GetPositionData(pd, m_devId) == 0) {
        double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
        addRow(2, 2, p, 100, 50, 50, 0);
    }
}

void TaskProgramDialog::onStartClicked() {
    int rowCount = m_table->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    std::vector<RobotAPI::MultiMoveInfo2> mps;
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(RobotAPI::MultiMoveInfo2));

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_table->item(r, i + 2)->text().toDouble();

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
    std::string selTool = m_robotToolCombo->currentText().toStdString();
    std::string selWobj = m_robotUserCombo->currentText().toStdString();

    QPointer<TaskProgramDialog> safeThis(this);
    QThread* worker = QThread::create([safeThis, mps, devId = m_devId, selTool, selWobj]() {
        if (!selTool.empty()) RobotAPI::SetCurrentToolByName(selTool, devId);
        if (!selWobj.empty()) RobotAPI::SetCurrentUframeByName(selWobj, devId);

        RobotAPI::MultiMove2Reset(devId);
        int ret = RobotAPI::MultiMove2Start(mps, devId);

        if (safeThis) {
            QMetaObject::invokeMethod(safeThis.data(), [safeThis, ret]() {
                if (!safeThis) return;
                safeThis->m_startBtn->setEnabled(true);
                if (ret == 0) safeThis->m_statusLabel->setText("执行中 (Running)...");
                else safeThis->m_statusLabel->setText(QString("启动失败！错误码: %1").arg(ret));
            }, Qt::QueuedConnection);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
void TaskProgramDialog::onPauseClicked() { RobotAPI::MultiMove2Hold(m_devId); }
void TaskProgramDialog::onResumeClicked() { RobotAPI::MultiMove2Resume(m_devId); }
void TaskProgramDialog::onResetClicked() { RobotAPI::MultiMove2Reset(m_devId); RobotAPI::MOVECLEAR(m_devId); }
