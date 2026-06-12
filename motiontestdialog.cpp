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
    setWindowTitle("机器人高级运动测试台 (姿态 CFG 自适应版)");
    setFixedSize(850, 450);

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
                // 默认拿工作坐标系(kcsPos 是基坐标, pcsPos 是用户坐标，此处按常用 kcs 取值)
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
            // MLIN_ZONE 等价于 MLIN, 且自带过渡，如果底层有 MLIN_CFG 接口，在这里也是最好调用带 CFG 的
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
    m_multiStatusLabel->setStyleSheet("color: #E53935; font-weight: bold;");

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

    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"1: 关节 (Joint)", "2: 直线 (Lin)", "3: 圆弧 (Circ)"});
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
                addRowToMultiTable(1, p, 100, 0);
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
                addRowToMultiTable(2, p, 100, 0);
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MotionTestDialog::onExecuteMultiMoveClicked() {
    int rowCount = m_multiTable->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    std::vector<RobotAPI::MultiMovePos> mps;
    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMovePos mp;

        QComboBox* combo = qobject_cast<QComboBox*>(m_multiTable->cellWidget(r, 0));
        if(combo) mp.type = combo->currentText().left(1).toInt();
        else mp.type = 2;

        for (int i = 0; i < 6; ++i) mp.pos[i] = m_multiTable->item(r, i + 1)->text().toDouble();
        mp.speed = m_multiTable->item(r, 7)->text().toInt();
        mp.overlapping = m_multiTable->item(r, 8)->text().toDouble();
        mp.cfg = 0; // 旧版接口缺少获取当前 CFG 的简易方法，如需彻底稳妥，更推荐用 MultiMove2
        mps.push_back(mp);
    }

    m_multiRunBtn->setEnabled(false);
    m_multiStatusLabel->setText("正在重置并下发组合运动...");

    QThread* worker = QThread::create([this, mps, devId = m_devId]() {
        RobotAPI::MultiMoveReset(devId);
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
// 第三页：第2类组合运动 (MultiMove2) 构建与逻辑 (完全注入 CFG！)
// =======================================================================
QWidget* MotionTestDialog::createMultiMove2Page()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* tableGroup = new QGroupBox("MultiMove2 高级运动队列 (支持姿态防跳变校验)", page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

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
    m_multi2StatusLabel->setStyleSheet("color: #9C27B0; font-weight: bold;");

    m_multi2RunBtn = new QPushButton("执行 MultiMove2 (自动填充CFG)", page);
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

    QComboBox* moveCombo = new QComboBox();
    moveCombo->addItems({"1: 关节(Joint)", "2: 直线(Lin)", "3: 圆弧(Circ)", "4: 圆角(CircAng)"});
    if (moveType >= 1 && moveType <= 4) moveCombo->setCurrentIndex(moveType - 1);
    m_multi2Table->setCellWidget(row, 0, moveCombo);

    QComboBox* posCombo = new QComboBox();
    posCombo->addItems({"1: Joint数据", "2: Cart数据"});
    if (posType == 1 || posType == 2) posCombo->setCurrentIndex(posType - 1);
    // ✔️ 修复：只使用正确的 m_multi2Table 变量
    m_multi2Table->setCellWidget(row, 1, posCombo);

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
                addRowToMulti2Table(1, 1, p, 100, 50, 50, 0);
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
                addRowToMulti2Table(2, 2, p, 100, 50, 50, 0);
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// 核心修复逻辑：动态提取系统当前 CFG，并强行融合到用户修改的数据包里！
void MotionTestDialog::onExecuteMultiMove2Clicked() {
    int rowCount = m_multi2Table->rowCount();
    if (m_devId == 0 || rowCount == 0) return;

    // 获取机器人的当前工具、坐标系和真实 CFG 姿态
    std::string curTool, curWobj;
    RobotAPI::GetCurrentToolName(curTool, m_devId);
    RobotAPI::GetCurrentUframeName(curWobj, m_devId);

    RobotAPI::RobotPos currentPos;
    memset(&currentPos, 0, sizeof(currentPos));
    // 我们必须调用带后缀 2 的方法，因为它返回的数据体包含了骨架配置！
    RobotAPI::GetBaseCoordinatePos2(currentPos, m_devId);

    std::vector<RobotAPI::MultiMoveInfo2> mps;

    for (int r = 0; r < rowCount; ++r) {
        RobotAPI::MultiMoveInfo2 mp;
        memset(&mp, 0, sizeof(RobotAPI::MultiMoveInfo2));

        QComboBox* moveCombo = qobject_cast<QComboBox*>(m_multi2Table->cellWidget(r, 0));
        mp.moveType = moveCombo ? moveCombo->currentText().left(1).toInt() : 2;

        QComboBox* posCombo = qobject_cast<QComboBox*>(m_multi2Table->cellWidget(r, 1));
        mp.posType = posCombo ? posCombo->currentText().left(1).toInt() : 2;

        double p[6];
        for (int i = 0; i < 6; ++i) p[i] = m_multi2Table->item(r, i + 2)->text().toDouble();

        if (mp.posType == 1) {
            // 如果你选的是纯关节运动，关节天生不带多解问题，不需要 CFG
            mp.ap[0].j[0] = p[0]; mp.ap[0].j[1] = p[1]; mp.ap[0].j[2] = p[2];
            mp.ap[0].j[3] = p[3]; mp.ap[0].j[4] = p[4]; mp.ap[0].j[5] = p[5];
        } else {
            // 🌟 选的是笛卡尔（XYZABC），这才是 Error 3 的重灾区！
            RobotAPI::RobotPos localP;
            memset(&localP, 0, sizeof(localP));
            localP.x = p[0]; localP.y = p[1]; localP.z = p[2];
            localP.a = p[3]; localP.b = p[4]; localP.c = p[5];

            // 致命填充：将刚刚从机器人本体上剥下来的 CFG，硬塞给你在界面上输入的这个坐标里！
            localP.cfgx = currentPos.cfgx;
            localP.cfg1 = currentPos.cfg1;
            localP.cfg4 = currentPos.cfg4;
            localP.cfg6 = currentPos.cfg6;

            RobotAPI::RobotJoint tJoints;
            // 通过 IK/FK 重算，确保笛卡尔数据绝对完美且不违背物理常理！
            if (RobotAPI::IkSolver(localP, tJoints, curTool, curWobj, m_devId) == 0) {
                RobotAPI::RobotPos finalP;
                if (RobotAPI::FkSolver(tJoints, finalP, curTool, curWobj, m_devId) == 0) {
                    mp.cp[0] = finalP; // 使用正解出来、自带完美 CFG 的数据
                } else {
                    mp.cp[0] = localP; // 兜底
                }
            } else {
                mp.cp[0] = localP;     // 如果因为超限逆解失败，原样发下去，让底层去报错
            }
        }

        mp.speed = m_multi2Table->item(r, 8)->text().toDouble();
        mp.acc = m_multi2Table->item(r, 9)->text().toDouble();
        mp.dec = m_multi2Table->item(r, 10)->text().toDouble();
        mp.jerk = 0;
        mp.overlapping = m_multi2Table->item(r, 11)->text().toDouble();
        mp.auxOverlapping = 1e100;
        mp.flags = 0;

        mps.push_back(mp);
    }

    m_multi2RunBtn->setEnabled(false);
    m_multi2StatusLabel->setText("正在利用底层计算防突变骨架并下发...");

    QThread* worker = QThread::create([this, mps, devId = m_devId]() {

        RobotAPI::MultiMove2Reset(devId);
        int ret = RobotAPI::MultiMove2Start(mps, devId);

        QMetaObject::invokeMethod(this, [this, ret]() {
            m_multi2RunBtn->setEnabled(true);
            if (ret == 0) {
                m_multi2StatusLabel->setText("MultiMove2 带有灵魂的 CFG 数据已下发成功！");
            } else {
                m_multi2StatusLabel->setText(QString("MultiMove2 失败，错误码: %1").arg(ret));
                QMessageBox::warning(this, "下发失败", QString("即使加了 CFG 依然被拒绝！\n错误码: %1\n原因可能是你修改的 A 角过大，或者 XYZ 跑到天边去了，物理上确实走不通直线。").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
