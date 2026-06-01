#include "motiontestdialog.h"
#include "EfortSdk.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QThread>
#include <QMetaObject>
#include <QMessageBox>

MotionTestDialog::MotionTestDialog(unsigned int devId, QWidget *parent)
    : QDialog(parent), m_devId(devId)
{
    setWindowTitle("机器人高级运动测试台");
    setFixedSize(850, 450); // 调大窗口以容纳左侧菜单和右侧表格

    setupUi();
}

void MotionTestDialog::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 1. 创建左侧导航菜单
    m_navMenu = new QListWidget(this);
    m_navMenu->setFixedWidth(160);
    m_navMenu->addItem("非阻塞直线 (MLIN)");
    m_navMenu->addItem("组合运动 (MultiMove)");
    m_navMenu->addItem("第2类组合 (MultiMove2)");
    m_navMenu->setStyleSheet(
        "QListWidget { border: 1px solid #ccc; background-color: #f8f9fa; outline: none; font-weight: bold; }"
        "QListWidget::item { height: 40px; padding-left: 10px; }"
        "QListWidget::item:selected { background-color: #2196F3; color: white; }"
        );

    // 2. 创建右侧层叠容器
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(createMlinPage());      // 索引 0
    m_stackedWidget->addWidget(createMultiMovePage()); // 索引 1
    m_stackedWidget->addWidget(createMultiMove2Page());

    mainLayout->addWidget(m_navMenu);
    mainLayout->addWidget(m_stackedWidget);

    connect(m_navMenu, &QListWidget::currentRowChanged, this, &MotionTestDialog::switchPage);
    m_navMenu->setCurrentRow(0);
}

void MotionTestDialog::switchPage(int index) {
    m_stackedWidget->setCurrentIndex(index);
}

// =======================================================================
// 第一页：非阻塞连续直线运动 (MLIN) 构建与逻辑
// =======================================================================
QWidget* MotionTestDialog::createMlinPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* tableGroup = new QGroupBox("MLIN 笛卡尔连续轨迹队列", page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    m_mlinTable = new QTableWidget(0, 8, page);
    m_mlinTable->setHorizontalHeaderLabels({"X", "Y", "Z", "RX", "RY", "RZ", "速度", "过渡区"});
    m_mlinTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_mlinTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLayout->addWidget(m_mlinTable);

    QHBoxLayout* editLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("添加空点位", page);
    QPushButton* removeBtn = new QPushButton("删除选中", page);
    QPushButton* getPosBtn = new QPushButton("获取当前笛卡尔坐标", page);

    editLayout->addWidget(addBtn); editLayout->addWidget(removeBtn); editLayout->addWidget(getPosBtn);
    editLayout->addStretch();
    tableLayout->addLayout(editLayout);
    layout->addWidget(tableGroup);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_mlinStatusLabel = new QLabel("状态: 准备就绪...", page);
    m_mlinStatusLabel->setStyleSheet("color: #1976D2; font-weight: bold;");

    m_mlinRunBtn = new QPushButton("执行 MLIN 队列", page);
    m_mlinRunBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 6px 15px; border-radius: 4px;");

    bottomLayout->addWidget(m_mlinStatusLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_mlinRunBtn);
    layout->addLayout(bottomLayout);

    connect(addBtn, &QPushButton::clicked, this, &MotionTestDialog::onMlinAddRowClicked);
    connect(removeBtn, &QPushButton::clicked, this, &MotionTestDialog::onMlinRemoveRowClicked);
    connect(getPosBtn, &QPushButton::clicked, this, &MotionTestDialog::onMlinGetPosClicked);
    connect(m_mlinRunBtn, &QPushButton::clicked, this, &MotionTestDialog::onExecuteMlinClicked);

    return page;
}

void MotionTestDialog::addRowToMlinTable(double* pos, int speed, double zone) {
    int row = m_mlinTable->rowCount();
    m_mlinTable->insertRow(row);
    double defaultPos[6] = {0,0,0,0,0,0};
    if (!pos) pos = defaultPos;

    for (int i = 0; i < 6; ++i) m_mlinTable->setItem(row, i, new QTableWidgetItem(QString::number(pos[i], 'f', 3)));
    m_mlinTable->setItem(row, 6, new QTableWidgetItem(QString::number(speed)));
    m_mlinTable->setItem(row, 7, new QTableWidgetItem(QString::number(zone, 'f', 1)));
}

void MotionTestDialog::onMlinAddRowClicked() { addRowToMlinTable(); }
void MotionTestDialog::onMlinRemoveRowClicked() {
    if (m_mlinTable->currentRow() >= 0) m_mlinTable->removeRow(m_mlinTable->currentRow());
}

void MotionTestDialog::onMlinGetPosClicked() {
    if (m_devId == 0) return;
    m_mlinStatusLabel->setText("正在同步...");
    QThread* worker = QThread::create([this]() {
        RobotAPI::PosData pd;
        int ret = RobotAPI::GetPositionData(pd, m_devId);
        QMetaObject::invokeMethod(this, [this, ret, pd]() {
            if (ret == 0) {
                double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
                addRowToMlinTable(p, 100, 10.0);
                m_mlinStatusLabel->setText("当前位置已加入 MLIN 队列！");
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onExecuteMlinClicked() {
    int rowCount = m_mlinTable->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    struct MPoint { double p[6]; int s; double z; };
    std::vector<MPoint> queue;
    for (int r = 0; r < rowCount; ++r) {
        MPoint pt;
        for (int i = 0; i < 6; ++i) pt.p[i] = m_mlinTable->item(r, i)->text().toDouble();
        pt.s = m_mlinTable->item(r, 6)->text().toInt();
        pt.z = m_mlinTable->item(r, 7)->text().toDouble();
        queue.push_back(pt);
    }

    m_mlinRunBtn->setEnabled(false);
    QThread* worker = QThread::create([this, queue, devId = m_devId]() {
        int ret = 0;
        for (auto& pt : queue) {
            double currentPos[6]; for(int k=0; k<6; k++) currentPos[k] = pt.p[k];
            ret = RobotAPI::MLIN(currentPos, pt.s, pt.z, devId);
            if (ret != 0) break;
        }
        QMetaObject::invokeMethod(this, [this, ret]() {
            m_mlinRunBtn->setEnabled(true);
            m_mlinStatusLabel->setText(ret == 0 ? "MLIN 执行成功" : QString("MLIN 失败: %1").arg(ret));
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// =======================================================================
// 第二页：组合运动 (MultiMove) 构建与逻辑
// =======================================================================
QWidget* MotionTestDialog::createMultiMovePage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* tableGroup = new QGroupBox("MultiMove 组合运动队列 (最大 10 点)", page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    m_multiTable = new QTableWidget(0, 9, page);
    m_multiTable->setHorizontalHeaderLabels({"运动类型", "J1/X", "J2/Y", "J3/Z", "J4/RX", "J5/RY", "J6/RZ", "Speed", "Overlap"});
    m_multiTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_multiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLayout->addWidget(m_multiTable);

    QHBoxLayout* editLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("添加", page);
    QPushButton* removeBtn = new QPushButton("删除", page);
    QPushButton* getJointBtn = new QPushButton("获取关节(Type1)", page);
    QPushButton* getCartBtn = new QPushButton("获取笛卡尔(Type2)", page);

    editLayout->addWidget(addBtn); editLayout->addWidget(removeBtn);
    editLayout->addWidget(getJointBtn); editLayout->addWidget(getCartBtn);
    editLayout->addStretch();
    tableLayout->addLayout(editLayout);
    layout->addWidget(tableGroup);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_multiStatusLabel = new QLabel("状态: 准备就绪...", page);
    m_multiStatusLabel->setStyleSheet("color: #E53935; font-weight: bold;"); // 红色警示色

    m_multiRunBtn = new QPushButton("执行组合运动 (自动 Reset)", page);
    m_multiRunBtn->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 6px 15px; border-radius: 4px;");

    bottomLayout->addWidget(m_multiStatusLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_multiRunBtn);
    layout->addLayout(bottomLayout);

    connect(addBtn, &QPushButton::clicked, this, &MotionTestDialog::onMultiAddRowClicked);
    connect(removeBtn, &QPushButton::clicked, this, &MotionTestDialog::onMultiRemoveRowClicked);
    connect(getJointBtn, &QPushButton::clicked, this, &MotionTestDialog::onMultiGetJointPosClicked);
    connect(getCartBtn, &QPushButton::clicked, this, &MotionTestDialog::onMultiGetCartPosClicked);
    connect(m_multiRunBtn, &QPushButton::clicked, this, &MotionTestDialog::onExecuteMultiMoveClicked);

    return page;
}

void MotionTestDialog::addRowToMultiTable(int type, double* pos, int speed, double overlap) {
    if (m_multiTable->rowCount() >= 10) {
        QMessageBox::warning(this, "限制", "MultiMove 一次最多发送 10 组点位！");
        return;
    }

    int row = m_multiTable->rowCount();
    m_multiTable->insertRow(row);
    double defaultPos[6] = {0,0,0,0,0,0};
    if (!pos) pos = defaultPos;

    // 单元格 0: 运动类型下拉框
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"1: 关节 (Joint)", "2: 直线 (Lin)", "3: 圆弧 (Circ)"});
    // 默认选中传入的 type
    if (type >= 1 && type <= 3) typeCombo->setCurrentIndex(type - 1);
    m_multiTable->setCellWidget(row, 0, typeCombo);

    for (int i = 0; i < 6; ++i) m_multiTable->setItem(row, i + 1, new QTableWidgetItem(QString::number(pos[i], 'f', 3)));
    m_multiTable->setItem(row, 7, new QTableWidgetItem(QString::number(speed)));
    m_multiTable->setItem(row, 8, new QTableWidgetItem(QString::number(overlap, 'f', 1)));
}

void MotionTestDialog::onMultiAddRowClicked() { addRowToMultiTable(2); }
void MotionTestDialog::onMultiRemoveRowClicked() {
    if (m_multiTable->currentRow() >= 0) m_multiTable->removeRow(m_multiTable->currentRow());
}

void MotionTestDialog::onMultiGetJointPosClicked() {
    if (m_devId == 0) return;
    QThread* worker = QThread::create([this]() {
        RobotAPI::PosData pd;
        int ret = RobotAPI::GetPositionData(pd, m_devId);
        QMetaObject::invokeMethod(this, [this, ret, pd]() {
            if (ret == 0) {
                double p[6]; for(int i=0; i<6; i++) p[i] = pd.acsPos[i];
                addRowToMultiTable(1, p, 100, 0); // Type 1 关节
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onMultiGetCartPosClicked() {
    if (m_devId == 0) return;
    QThread* worker = QThread::create([this]() {
        RobotAPI::PosData pd;
        int ret = RobotAPI::GetPositionData(pd, m_devId);
        QMetaObject::invokeMethod(this, [this, ret, pd]() {
            if (ret == 0) {
                double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
                addRowToMultiTable(2, p, 100, 0); // Type 2 笛卡尔
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onExecuteMultiMoveClicked() {
    int rowCount = m_multiTable->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    // 从表格打包 MultiMovePos 结构体数据
    std::vector<RobotAPI::MultiMovePos> mps;
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMovePos mp;

        QComboBox* combo = qobject_cast<QComboBox*>(m_multiTable->cellWidget(r, 0));
        if(combo) mp.type = combo->currentText().left(1).toInt(); /* 截取 "1", "2", "3" 转为整数 */
        else mp.type = 2; // 容错兜底

        for (int i = 0; i < 6; ++i) mp.pos[i] = m_multiTable->item(r, i + 1)->text().toDouble();
        mp.speed = m_multiTable->item(r, 7)->text().toInt();
        mp.overlapping = m_multiTable->item(r, 8)->text().toDouble();

        mps.push_back(mp);
    }

    m_multiRunBtn->setEnabled(false);
    m_multiStatusLabel->setText("正在重置并下发组合运动...");

    QThread* worker = QThread::create([this, mps, devId = m_devId]() {
        // 👇 核心机制：执行前必须调用 Reset，否则后续可能会被拒绝
        RobotAPI::MultiMoveReset(devId);

        // 下发整个列表
        int ret = RobotAPI::MultiMoveStart(mps, devId);

        QMetaObject::invokeMethod(this, [this, ret]() {
            m_multiRunBtn->setEnabled(true);
            if (ret == 0) {
                m_multiStatusLabel->setText("组合运动 (MultiMove) 下发成功！");
            } else {
                m_multiStatusLabel->setText(QString("组合运动失败，错误码: %1").arg(ret));
                QMessageBox::warning(this, "下发失败", QString("MultiMoveStart 被拒绝！错误码: %1").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// =======================================================================
// 第三页：第2类组合运动 (MultiMove2) 构建与逻辑
// =======================================================================
QWidget* MotionTestDialog::createMultiMove2Page()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* tableGroup = new QGroupBox("MultiMove2 高级运动队列 (支持加减速设定)", page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    // 表格增加至 12 列，涵盖 acc 和 dec
    m_multi2Table = new QTableWidget(0, 12, page);
    m_multi2Table->setHorizontalHeaderLabels({"插补模式", "位置类型", "J1/X", "J2/Y", "J3/Z", "J4/RX", "J5/RY", "J6/RZ", "速度", "加Acc", "减Dec", "Overlap"});
    m_multi2Table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_multi2Table->horizontalHeader()->setStretchLastSection(true);
    m_multi2Table->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLayout->addWidget(m_multi2Table);

    QHBoxLayout* editLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("添加", page);
    QPushButton* removeBtn = new QPushButton("删除", page);
    QPushButton* getJointBtn = new QPushButton("获取关节(posType=1)", page);
    QPushButton* getCartBtn = new QPushButton("获取笛卡尔(posType=2)", page);

    editLayout->addWidget(addBtn); editLayout->addWidget(removeBtn);
    editLayout->addWidget(getJointBtn); editLayout->addWidget(getCartBtn);
    editLayout->addStretch();
    tableLayout->addLayout(editLayout);
    layout->addWidget(tableGroup);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_multi2StatusLabel = new QLabel("状态: 准备就绪...", page);
    m_multi2StatusLabel->setStyleSheet("color: #9C27B0; font-weight: bold;"); // 紫色区分

    m_multi2RunBtn = new QPushButton("执行 MultiMove2 (自动 Reset)", page);
    m_multi2RunBtn->setStyleSheet("background-color: #9C27B0; color: white; font-weight: bold; padding: 6px 15px; border-radius: 4px;");

    bottomLayout->addWidget(m_multi2StatusLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_multi2RunBtn);
    layout->addLayout(bottomLayout);

    connect(addBtn, &QPushButton::clicked, this, &MotionTestDialog::onMulti2AddRowClicked);
    connect(removeBtn, &QPushButton::clicked, this, &MotionTestDialog::onMulti2RemoveRowClicked);
    connect(getJointBtn, &QPushButton::clicked, this, &MotionTestDialog::onMulti2GetJointPosClicked);
    connect(getCartBtn, &QPushButton::clicked, this, &MotionTestDialog::onMulti2GetCartPosClicked);
    connect(m_multi2RunBtn, &QPushButton::clicked, this, &MotionTestDialog::onExecuteMultiMove2Clicked);

    return page;
}

void MotionTestDialog::addRowToMulti2Table(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap)
{
    int row = m_multi2Table->rowCount();
    m_multi2Table->insertRow(row);
    double defaultPos[6] = {0,0,0,0,0,0};
    if (!pos) pos = defaultPos;

    // 单元格 0: moveType (插补模式)
    QComboBox* moveCombo = new QComboBox();
    moveCombo->addItems({"1: 关节(Joint)", "2: 直线(Lin)", "3: 圆弧(Circ)", "4: 圆角(CircAng)"});
    if (moveType >= 1 && moveType <= 4) moveCombo->setCurrentIndex(moveType - 1);
    m_multi2Table->setCellWidget(row, 0, moveCombo);

    // 单元格 1: posType (位置类型)
    QComboBox* posCombo = new QComboBox();
    posCombo->addItems({"1: Joint数据", "2: Cart数据"});
    if (posType == 1 || posType == 2) posCombo->setCurrentIndex(posType - 1);
    m_multi2Table->setCellWidget(row, 1, posCombo);

    // 坐标与参数填入
    for (int i = 0; i < 6; ++i) m_multi2Table->setItem(row, i + 2, new QTableWidgetItem(QString::number(pos[i], 'f', 3)));

    m_multi2Table->setItem(row, 8, new QTableWidgetItem(QString::number(speed, 'f', 1)));
    m_multi2Table->setItem(row, 9, new QTableWidgetItem(QString::number(acc, 'f', 1)));
    m_multi2Table->setItem(row, 10, new QTableWidgetItem(QString::number(dec, 'f', 1)));
    m_multi2Table->setItem(row, 11, new QTableWidgetItem(QString::number(overlap, 'f', 1)));
}

void MotionTestDialog::onMulti2AddRowClicked() { addRowToMulti2Table(2, 2); }
void MotionTestDialog::onMulti2RemoveRowClicked() {
    if (m_multi2Table->currentRow() >= 0) m_multi2Table->removeRow(m_multi2Table->currentRow());
}

void MotionTestDialog::onMulti2GetJointPosClicked() {
    if (m_devId == 0) return;
    QThread* worker = QThread::create([this]() {
        RobotAPI::PosData pd;
        int ret = RobotAPI::GetPositionData(pd, m_devId);
        QMetaObject::invokeMethod(this, [this, ret, pd]() {
            if (ret == 0) {
                double p[6]; for(int i=0; i<6; i++) p[i] = pd.acsPos[i];
                addRowToMulti2Table(1, 1, p, 100, 50, 50, 0); // moveType=1(关节插补), posType=1(关节数据)
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onMulti2GetCartPosClicked() {
    if (m_devId == 0) return;
    QThread* worker = QThread::create([this]() {
        RobotAPI::PosData pd;
        int ret = RobotAPI::GetPositionData(pd, m_devId);
        QMetaObject::invokeMethod(this, [this, ret, pd]() {
            if (ret == 0) {
                double p[6]; for(int i=0; i<6; i++) p[i] = pd.kcsPos[i];
                addRowToMulti2Table(2, 2, p, 100, 50, 50, 0); // moveType=2(直线插补), posType=2(Cart数据)
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onExecuteMultiMove2Clicked() {
    int rowCount = m_multi2Table->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    // SDK 数据结构：MultiMoveInfo2 (在某些 SDK 版本中被typedef 为 BlockMultiMoveInfo)
    std::vector<RobotAPI::MultiMoveInfo2> mps;

    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(RobotAPI::MultiMoveInfo2)); // 内存清零，防止无效标志位导致机器人死机

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_multi2Table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_multi2Table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        // 提取坐标系数据，并根据 posType 存入 ap(关节) 或 cp(笛卡尔)
        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_multi2Table->item(r, i + 2)->text().toDouble();

        if (mp.posType == 1) { // 关节坐标系 (使用 j 数组，下标 0~5 对应 1~6 轴)
            mp.ap[0].j[0] = p[0]; mp.ap[0].j[1] = p[1]; mp.ap[0].j[2] = p[2];
            mp.ap[0].j[3] = p[3]; mp.ap[0].j[4] = p[4]; mp.ap[0].j[5] = p[5];
        } else { // 笛卡尔坐标系 (x,y,z,a,b,c 是独立的成员变量)
            mp.cp[0].x = p[0]; mp.cp[0].y = p[1]; mp.cp[0].z = p[2];
            mp.cp[0].a = p[3]; mp.cp[0].b = p[4]; mp.cp[0].c = p[5];
        }

        mp.speed = m_multi2Table->item(r, 8)->text().toDouble();
        mp.acc = m_multi2Table->item(r, 9)->text().toDouble();
        mp.dec = m_multi2Table->item(r, 10)->text().toDouble();
        mp.jerk = 0; // 默认平滑
        mp.overlapping = m_multi2Table->item(r, 11)->text().toDouble();
        mp.auxOverlapping = 0;
        mp.flags = 0;

        mps.push_back(mp);
    }

    m_multi2RunBtn->setEnabled(false);
    m_multi2StatusLabel->setText("正在重置并下发 MultiMove2 ...");

    QThread* worker = QThread::create([this, mps, devId = m_devId]() {
        // 执行前强制调用第 2 类组合重置
        RobotAPI::MultiMove2Reset(devId);

        // 下发整个列队
        int ret = RobotAPI::MultiMove2Start(mps, devId);

        QMetaObject::invokeMethod(this, [this, ret]() {
            m_multi2RunBtn->setEnabled(true);
            if (ret == 0) {
                m_multi2StatusLabel->setText("MultiMove2 下发成功 (ERROR_OK)！");
            } else {
                m_multi2StatusLabel->setText(QString("MultiMove2 失败，错误码: %1").arg(ret));
                QMessageBox::warning(this, "下发失败", QString("MultiMove2Start 被拒绝！\n错误码: %1\n请检查加减速度(acc/dec)是否有效，以及坐标数据(posType)是否匹配。").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
