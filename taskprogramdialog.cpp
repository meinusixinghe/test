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

TaskProgramDialog::TaskProgramDialog(unsigned int devId, const QVector<Hole>& holes, QWidget *parent)
    : QDialog(parent), m_devId(devId)
{
    setWindowTitle("任务程序运行控制台 (MultiMove2)");
    setMinimumSize(950, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 1. 运动点位编辑区
    QGroupBox* tableGroup = new QGroupBox("运行轨迹程序 (双击可修改参数)", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    m_table = new QTableWidget(0, 12, this);
    m_table->setHorizontalHeaderLabels({"插补模式", "坐标类型", "X", "Y", "Z", "RX", "RY", "RZ", "速度", "加速", "减速", "平滑度"});
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

    // 2. 状态与控制区
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

    // 4. 将导入的孔位转化为程序列表 (MoveType=2 直线, PosType=2 笛卡尔)
    for (const Hole& h : holes) {
        double p[6] = { h.center3D.x(), h.center3D.y(), h.center3D.z(), currentPose.a, currentPose.b, currentPose.c };
        addRow(2, 2, p, 100, 50, 50, 10); // 默认速度100, 加减速50, 过渡圆角10保证平滑连续
    }
}

void TaskProgramDialog::addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap) {
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
