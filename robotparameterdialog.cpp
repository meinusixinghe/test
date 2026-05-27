#include "robotparameterdialog.h"
#include "EfortSdk.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>
#include <QThread>
#include <QMetaObject>
#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QStringList>

RobotParameterDialog::RobotParameterDialog(unsigned int devId, QWidget *parent)
    : QDialog(parent), m_devId(devId)
{
    setWindowTitle("机器参数与控制中心");
    // 加大一点窗口尺寸，因为现在有左侧菜单了
    setFixedSize(500, 350);

    // 初始化界面搭建
    setupUi();
}

void RobotParameterDialog::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 1. 创建左侧导航菜单
    m_navMenu = new QListWidget(this);
    m_navMenu->setFixedWidth(120);
    m_navMenu->addItem("全局速度");
    m_navMenu->addItem("机器人运动");
    // 你可以在这里继续添加：m_navMenu->addItem("TCP标定");
    // 设置一些基础样式让它看起来更像工业软件的侧边栏
    m_navMenu->setStyleSheet(
        "QListWidget { border: 1px solid #ccc; background-color: #f8f9fa; outline: none; }"
        "QListWidget::item { height: 40px; padding-left: 10px; }"
        "QListWidget::item:selected { background-color: #2196F3; color: white; font-weight: bold; }"
        );

    // 2. 创建右侧层叠窗口容器
    m_stackedWidget = new QStackedWidget(this);

    // 把不同的页面塞进层叠容器里
    m_stackedWidget->addWidget(createSpeedPage());  // 索引 0
    m_stackedWidget->addWidget(createMotionPage()); // 索引 1

    // 3. 将左右两部分加入主布局
    mainLayout->addWidget(m_navMenu);
    mainLayout->addWidget(m_stackedWidget);

    // 4. 连接菜单点击与页面切换信号
    connect(m_navMenu, &QListWidget::currentRowChanged, this, &RobotParameterDialog::switchPage);

    // 默认选中第一项
    m_navMenu->setCurrentRow(0);
}

// 槽函数：实现点击左侧，右侧切换
void RobotParameterDialog::switchPage(int index)
{
    m_stackedWidget->setCurrentIndex(index);
}

// ==========================================
// 页面构建：全局速度页
// ==========================================
QWidget* RobotParameterDialog::createSpeedPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // --- 区域 1：直接设置全局速度 ---
    QGroupBox* setGroup = new QGroupBox("全局速度设置", page);
    QHBoxLayout* speedLayout = new QHBoxLayout(setGroup);
    speedLayout->addWidget(new QLabel("全局速度倍率 (%):"));

    m_speedSpinBox = new QSpinBox(page);
    m_speedSpinBox->setRange(1, 100);
    unsigned int realSpeed = 10;
    if (m_devId != 0) {
        // 调用 SDK 获取当前全局速度
        int ret = RobotAPI::GetCurrentSpeedRatio(realSpeed, m_devId);
        if (ret != 0) {
            qDebug() << "获取真实全局速度失败，错误码:" << ret;
            realSpeed = 10;
        }
    }
    m_speedSpinBox->setValue(realSpeed);
    speedLayout->addWidget(m_speedSpinBox);

    m_setSpeedBtn = new QPushButton("设置", page);
    m_setSpeedBtn->setCursor(Qt::PointingHandCursor);
    speedLayout->addWidget(m_setSpeedBtn);
    layout->addWidget(setGroup);

    // --- 区域 2：动态微调 ---
    QGroupBox* adjustGroup = new QGroupBox("动态微调 (按住按钮生效)", page);
    QHBoxLayout* adjustLayout = new QHBoxLayout(adjustGroup);

    m_decreaseBtn = new QPushButton("速度 -", page);
    m_increaseBtn = new QPushButton("速度 +", page);

    // 设置一点样式让按键更明显
    QString btnStyle = "QPushButton { padding: 15px; font-weight: bold; background-color: #e0e0e0; border: 1px solid #aaa; border-radius: 4px; }"
                       "QPushButton:pressed { background-color: #bdbdbd; }";
    m_decreaseBtn->setStyleSheet(btnStyle);
    m_increaseBtn->setStyleSheet(btnStyle);

    adjustLayout->addWidget(m_decreaseBtn);
    adjustLayout->addWidget(m_increaseBtn);
    layout->addWidget(adjustGroup);

    layout->addStretch(); // 把控件顶到上方

    // 信号绑定
    connect(m_setSpeedBtn, &QPushButton::clicked, this, &RobotParameterDialog::onSetSpeedClicked);
    connect(m_increaseBtn, &QPushButton::pressed, this, &RobotParameterDialog::onIncreasePressed);
    connect(m_increaseBtn, &QPushButton::released, this, &RobotParameterDialog::onIncreaseReleased);
    connect(m_decreaseBtn, &QPushButton::pressed, this, &RobotParameterDialog::onDecreasePressed);
    connect(m_decreaseBtn, &QPushButton::released, this, &RobotParameterDialog::onDecreaseReleased);

    return page;
}

// ==========================================
// 页面构建：机器人运动控制页 (Jogging)
// ==========================================
QWidget* RobotParameterDialog::createMotionPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. 坐标系选择区域
    // ==========================================
    QGroupBox* modeGroup = new QGroupBox("运动坐标系设置", page);
    QHBoxLayout* modeLayout = new QHBoxLayout(modeGroup);
    modeLayout->addWidget(new QLabel("当前 Jog 坐标系:"));

    QComboBox* jogModeCombo = new QComboBox(page);
    jogModeCombo->addItem("0: 关节坐标系 (Joint)", 0);
    jogModeCombo->addItem("1: 机器人坐标系 (Base)", 1);
    jogModeCombo->addItem("2: 工具坐标系 (Tool)", 2);
    jogModeCombo->addItem("3: 用户坐标系 (User)", 3);
    jogModeCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    modeLayout->addWidget(jogModeCombo);

    // 绑定下拉框切换事件：动态下发 SetJogMode
    connect(jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, jogModeCombo](int index) {
        if (m_devId == 0) return;
        int mode = jogModeCombo->itemData(index).toInt();
        unsigned int devId = m_devId;
        runAsyncCommand([devId, mode]() {
            RobotAPI::SetJogMode(mode, devId);
        });
    });
    layout->addWidget(modeGroup);

    // ==========================================
    // 2. 运动控制按钮与状态显示区域
    // ==========================================
    QGroupBox* jogGroup = new QGroupBox("轴控点动 (按住运行，松开停止)", page);
    QVBoxLayout* jogMainLayout = new QVBoxLayout(jogGroup); // 垂直布局，用来放标签和下方的网格

    // 👉 错误状态监控标签
    QLabel* jogStatusLabel = new QLabel("状态: 就绪", page);
    jogStatusLabel->setStyleSheet("color: #E53935; font-weight: bold; font-size: 13px;");
    jogMainLayout->addWidget(jogStatusLabel);

    // 👉 核心修复：在这里声明并创建 gridLayout！
    QWidget* gridWidget = new QWidget(page);
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    jogMainLayout->addWidget(gridWidget);

    QString btnStyle = "QPushButton { padding: 8px; font-weight: bold; background-color: #e0e0e0; border: 1px solid #aaa; border-radius: 4px; font-size: 14px; }"
                       "QPushButton:pressed { background-color: #4CAF50; color: white; border: 1px solid #388E3C; }";
    QStringList axisNames = { "1 轴 (X)", "2 轴 (Y)", "3 轴 (Z)", "4 轴 (RX)", "5 轴 (RY)", "6 轴 (RZ)" };

    for (int i = 0; i < 6; ++i) {
        QLabel* lblAxis = new QLabel(axisNames[i], gridWidget);
        lblAxis->setFixedWidth(60);

        QPushButton* btnMinus = new QPushButton("- (Minus)", gridWidget);
        QPushButton* btnPlus = new QPushButton("+ (Plus)", gridWidget);

        btnMinus->setStyleSheet(btnStyle); btnPlus->setStyleSheet(btnStyle);
        btnMinus->setCursor(Qt::PointingHandCursor); btnPlus->setCursor(Qt::PointingHandCursor);

        // 使用刚才正确声明的 gridLayout
        gridLayout->addWidget(lblAxis, i, 0);
        gridLayout->addWidget(btnMinus, i, 1);
        gridLayout->addWidget(btnPlus, i, 2);

        int axisIndex = i + 1; // 1~6

        connect(btnMinus, &QPushButton::pressed, this, [this, axisIndex, jogStatusLabel]() {
            handleJogButton(axisIndex, false, true, jogStatusLabel);
        });
        connect(btnMinus, &QPushButton::released, this, [this, axisIndex, jogStatusLabel]() {
            handleJogButton(axisIndex, false, false, jogStatusLabel);
        });
        connect(btnPlus, &QPushButton::pressed, this, [this, axisIndex, jogStatusLabel]() {
            handleJogButton(axisIndex, true, true, jogStatusLabel);
        });
        connect(btnPlus, &QPushButton::released, this, [this, axisIndex, jogStatusLabel]() {
            handleJogButton(axisIndex, true, false, jogStatusLabel);
        });
    }

    layout->addWidget(jogGroup);
    layout->addStretch(); // 撑起空白部分

    // 打开面板时默认发一次指令：切到下拉框第一个坐标系 (关节)
    if (m_devId != 0) {
        unsigned int devId = m_devId;
        runAsyncCommand([devId]() { RobotAPI::SetJogMode(0, devId); });
    }

    return page;
}

// ==========================================
// 异步指令与速度控制逻辑
// ==========================================
void RobotParameterDialog::handleJogButton(int axisIndex, bool positive, bool pressed, QLabel* statusLabel)
{
    if (m_devId == 0 || axisIndex < 1 || axisIndex > 6) {
        return;
    }

    unsigned int devId = m_devId;
    int ret = 0;

    switch (axisIndex) {
    case 1:
        ret = positive ? RobotAPI::Jog1Plus(pressed, devId)
                       : RobotAPI::Jog1Minus(pressed, devId);
        break;
    case 2:
        ret = positive ? RobotAPI::Jog2Plus(pressed, devId)
                       : RobotAPI::Jog2Minus(pressed, devId);
        break;
    case 3:
        ret = positive ? RobotAPI::Jog3Plus(pressed, devId)
                       : RobotAPI::Jog3Minus(pressed, devId);
        break;
    case 4:
        ret = positive ? RobotAPI::Jog4Plus(pressed, devId)
                       : RobotAPI::Jog4Minus(pressed, devId);
        break;
    case 5:
        ret = positive ? RobotAPI::Jog5Plus(pressed, devId)
                       : RobotAPI::Jog5Minus(pressed, devId);
        break;
    case 6:
        ret = positive ? RobotAPI::Jog6Plus(pressed, devId)
                       : RobotAPI::Jog6Minus(pressed, devId);
        break;
    }

    if (ret == 0) {
        statusLabel->setText(pressed ? "状态: 运行中..." : "状态: 就绪");
    } else {
        statusLabel->setText(QString("%1失败，错误码: %2")
                                 .arg(pressed ? "启动" : "停止")
                                 .arg(ret));
    }
}

void RobotParameterDialog::runAsyncCommand(std::function<void()> cmd) {
    QThread* worker = QThread::create([cmd]() {
        cmd();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void RobotParameterDialog::onSetSpeedClicked() {
    if (m_devId == 0) return;
    int ratio = m_speedSpinBox->value();
    m_setSpeedBtn->setEnabled(false);

    QThread* worker = QThread::create([this, ratio, devId = m_devId]() {
        int ret = RobotAPI::SetGlobalSpeed(ratio, devId);
        QMetaObject::invokeMethod(this, [this, ret]() {
            m_setSpeedBtn->setEnabled(true);
            if (ret == 0) {
                QMessageBox::information(this, "成功", "全局速度设置成功！");
            } else {
                QMessageBox::warning(this, "失败", QString("设置速度失败，错误码: %1").arg(ret));
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void RobotParameterDialog::onIncreasePressed() {
    if (m_devId != 0) runAsyncCommand([devId = m_devId]() { RobotAPI::IncreaseVelocity(true, devId); });
}
void RobotParameterDialog::onIncreaseReleased() {
    if (m_devId != 0) runAsyncCommand([devId = m_devId]() { RobotAPI::IncreaseVelocity(false, devId); });
}
void RobotParameterDialog::onDecreasePressed() {
    if (m_devId != 0) runAsyncCommand([devId = m_devId]() { RobotAPI::DecreaseVelocity(true, devId); });
}
void RobotParameterDialog::onDecreaseReleased() {
    if (m_devId != 0) runAsyncCommand([devId = m_devId]() { RobotAPI::DecreaseVelocity(false, devId); });
}
