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

// ====================================================================
// 构造函数：解析线条序列并生成运动程序表格
// ====================================================================
TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, QWidget *parent)
    : QDialog(parent), m_devId(devId)
{
    setWindowTitle("任务程序运行控制台 (MultiMove2)");
    setMinimumSize(1100, 500); // 稍微加宽以容纳备注列
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 1. 运动点位编辑区
    QGroupBox* tableGroup = new QGroupBox("运行轨迹程序 (双击可手动修改坐标及速度参数)", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    // 👇【修改】：增加第 13 列 -> 备注
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

    // 3. 提取当先机器人姿态 (ABC值)，以防旋转错位
    RobotAPI::RobotPos currentPose;
    RobotAPI::GetBaseCoordinatePos(currentPose, m_devId);

    // ====================================================================
    // 4. 👇【核心逻辑】：将导入的线条转化为智能程序列表
    // ====================================================================
    QPointF lastEndPos(-99999.0, -99999.0); // 记录上一个到达的真实物理位置

    for (int idx = 0; idx < paths.size(); ++idx) {
        const Contour& c = paths[idx];
        if (c.points.isEmpty()) continue;

        // 生成针对工人的提示前缀：例如 "图元1[Arc]"
        QString shapeName = QString("图元%1[%2]").arg(idx + 1).arg(c.type);

        // 智能判定：如果图纸轮廓类型包含"圆"或"Arc"或"Circle"，底层使用圆弧插补(3)，否则用直线(2)
        bool isArc = c.type.contains("圆") || c.type.contains("Arc", Qt::CaseInsensitive) || c.type.contains("Circle", Qt::CaseInsensitive);
        int contourMoveType = isArc ? 3 : 2;

        for (int i = 0; i < c.points.size(); ++i) {
            QPointF pt = c.points[i];

            // ① 【缝合判断】：计算当前点与上一动作终点的距离 (公差设为 0.001 mm)
            double dx = pt.x() - lastEndPos.x();
            double dy = pt.y() - lastEndPos.y();
            bool isConnectedWithPrev = (std::sqrt(dx*dx + dy*dy) < 0.001);

            // 如果这是当前图元的第1个点，且与上个图元完全相连，直接跳过！不发重复指令！
            if (i == 0 && isConnectedWithPrev) {
                continue;
            }

            // ② 【前瞻判断】：看看这个图元的终点，是否和【下一个】图元的起点连在一起
            bool isConnectedWithNext = false;
            if (i == c.points.size() - 1 && idx + 1 < paths.size()) {
                QPointF nextStart = paths[idx + 1].points.first();
                double nx = pt.x() - nextStart.x();
                double ny = pt.y() - nextStart.y();
                if (std::sqrt(nx*nx + ny*ny) < 0.001) {
                    isConnectedWithNext = true;
                }
            }

            // 这里将 Z坐标暂定为 0.0，实际可根据工艺偏移量调整
            double p[6] = { pt.x(), pt.y(), 0.0, currentPose.a, currentPose.b, currentPose.c };

            if (i == 0) {
                // 如果能走到这里，说明【不连贯】，必须空移过去。
                // 注意：无论后面是走圆弧还是直线，飞到起点这个动作一定是直线(2)，停准 overlap=0
                addRow(2, 2, p, 100, 80, 80, 0, shapeName + "-空走跳转到起点");
            } else if (isConnectedWithNext) {
                // 虽然是本图元终点，但与下个图元无缝相连 -> 保持 overlap 平滑过渡，不减速到0
                addRow(contourMoveType, 2, p, 50, 50, 50, 2, shapeName + "-无缝过渡下个图元");
            } else if (i == c.points.size() - 1) {
                // 真正的断点/终点 -> 必须减速停准，overlap设为 0
                addRow(contourMoveType, 2, p, 50, 50, 50, 0, shapeName + "-终点(抬刀/加工结束)");
            } else {
                // 中间点 (如圆弧的中点，或多段线的折点) -> 保持连贯平滑
                addRow(contourMoveType, 2, p, 50, 50, 50, 2, shapeName + QString("-途经点%1").arg(i));
            }

            // 刷新机器人当前所在的末端物理位置
            lastEndPos = pt;
        }
    }
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
    m_statusLabel->setText("正在下发组合运动程序...");

    QThread* worker = QThread::create([this, mps, devId = m_devId]() {
        RobotAPI::MultiMove2Reset(devId); // 先重置
        int ret = RobotAPI::MultiMove2Start(mps, devId); // 启动

        QMetaObject::invokeMethod(this, [this, ret]() {
            m_startBtn->setEnabled(true);
            if (ret == 0) m_statusLabel->setText("程序正在连续执行中 (Running)...");
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
