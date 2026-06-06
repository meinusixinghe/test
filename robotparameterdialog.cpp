#include "robotparameterdialog.h"
#include "EfortSdk.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>
#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QStringList>
#include <QScrollArea>
#include <vector>
#include <string>
#include <QEvent>

RobotParameterDialog::RobotParameterDialog(unsigned int devId, QWidget *parent)
    : QDialog(parent), m_devId(devId)
{
    setWindowTitle("机器参数与控制中心");
    // 加大一点窗口尺寸，因为现在有左侧菜单了
    setFixedSize(500, 350);

    // 初始化界面搭建
    setupUi();
}

RobotParameterDialog::~RobotParameterDialog()
{
    stopActiveJog();
    if (m_servoTimer) {
        m_servoTimer->stop();
    }
}

void RobotParameterDialog::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 1. 创建左侧导航菜单
    m_navMenu = new QListWidget(this);
    m_navMenu->setFixedWidth(100);
    m_navMenu->addItem("全局速度");
    m_navMenu->addItem("机器人运动");
    m_navMenu->setStyleSheet(
        "QListWidget { border: 1px solid #ccc; background-color: #f8f9fa; outline: none; }"
        "QListWidget::item { height: 30px; padding-left: 10px; }"
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

    // 如果没有初始化定时器，创建一下
    if (!m_servoTimer) {
        m_servoTimer = new QTimer(this);
        connect(m_servoTimer, &QTimer::timeout, this, &RobotParameterDialog::onServoTimerTimeout);
    }

    if (index == 1) { // 1 代表运动控制页
        if (m_devId != 0) {
            RobotAPI::SetJogMode(m_currentJogMode, m_devId);
        }
        updatePositionValues();
        updateRealTimeStatus();
        updateCoordinateSystems();
    } else {
        stopActiveJog();
        if (m_servoTimer->isActive()) m_servoTimer->stop();
    }
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
    QString btnStyle = "QPushButton { padding: 6px; font-weight: bold; background-color: #e0e0e0; border: 1px solid #aaa; border-radius: 4px; }"
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
// 页面构建：机器人运动控制页 (Jogging) - 带滑动条支持
// ==========================================
QWidget* RobotParameterDialog::createMotionPage()
{
    // 1. 创建最外层的页面容器
    QWidget* page = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 2. 创建核心控件：滚动区域 (ScrollArea)
    QScrollArea* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true); // 极其关键：允许内部控件随窗口自适应缩放宽度
    scrollArea->setFrameShape(QFrame::NoFrame); // 去除滚动区域的默认边框，更美观

    // 3. 创建一个承载所有实际内容的内部 Widget
    QWidget* scrollContent = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(5, 5, 15, 5); // 右侧留一点边距给滚动条

    // ==========================================
    // 1. 运动与工作坐标系选择区域
    // ==========================================
    QGroupBox* modeGroup = new QGroupBox("坐标系状态与设置", scrollContent);
    QGridLayout* modeLayout = new QGridLayout(modeGroup);

    modeLayout->addWidget(new QLabel("Jog 运动模式:"), 0, 0);
    QComboBox* jogModeCombo = new QComboBox(scrollContent);
    jogModeCombo->addItem("0: 关节坐标系 (Joint)", 0);
    jogModeCombo->addItem("1: 机器人基座坐标系 (Base)", 1);
    jogModeCombo->addItem("2: 工具坐标系 (Tool)", 2);
    jogModeCombo->addItem("3: 用户坐标系 (User)", 3);
    jogModeCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    jogModeCombo->installEventFilter(this);
    modeLayout->addWidget(jogModeCombo, 0, 1);

    connect(jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, jogModeCombo](int index) {
        if (m_devId == 0) return;
        int mode = jogModeCombo->itemData(index).toInt();
        m_currentJogMode = mode;
        RobotAPI::SetJogMode(mode, m_devId);
        updatePositionValues();
    });

    modeLayout->addWidget(new QLabel("当前工具 (Tool):"), 1, 0);
    m_toolCombo = new QComboBox(scrollContent);
    m_toolCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    m_toolCombo->installEventFilter(this);
    modeLayout->addWidget(m_toolCombo, 1, 1);

    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_isUpdatingCoords || index < 0 || m_devId == 0) return;
        std::string toolName = m_toolCombo->itemText(index).toStdString();
        int ret = RobotAPI::SetCurrentToolByName(toolName, m_devId);
        if (ret != 0) qDebug() << "设置工具坐标系失败，错误码:" << ret;
    });

    modeLayout->addWidget(new QLabel("当前用户 (User):"), 2, 0);
    m_userCombo = new QComboBox(scrollContent);
    m_userCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    m_userCombo->installEventFilter(this);
    modeLayout->addWidget(m_userCombo, 2, 1);

    connect(m_userCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_isUpdatingCoords || index < 0 || m_devId == 0) return;
        std::string userName = m_userCombo->itemText(index).toStdString();
        int ret = RobotAPI::SetCurrentUframeByName(userName, m_devId);
        if (ret != 0) qDebug() << "设置用户坐标系失败，错误码:" << ret;
    });

    layout->addWidget(modeGroup);

    // ==========================================
    // 2. 手动模式伺服上电控制区域
    // ==========================================
    QGroupBox* powerGroup = new QGroupBox("手动模式使能控制", scrollContent);
    QHBoxLayout* powerLayout = new QHBoxLayout(powerGroup);

    m_manualPowerBtn = new QPushButton("读取状态中...", scrollContent);
    m_manualPowerBtn->setCursor(Qt::PointingHandCursor);

    m_servoStatusLabel = new QLabel("伺服状态: 未知", scrollContent);
    m_servoStatusLabel->setStyleSheet("font-weight: bold; color: #757575;");

    QPushButton* refreshStatusBtn = new QPushButton("刷新状态", scrollContent);
    refreshStatusBtn->setCursor(Qt::PointingHandCursor);
    refreshStatusBtn->setStyleSheet("QPushButton { padding: 4px 8px; font-size: 12px; }");

    powerLayout->addWidget(m_manualPowerBtn);
    powerLayout->addWidget(m_servoStatusLabel);
    powerLayout->addWidget(refreshStatusBtn);
    powerLayout->addStretch();
    layout->addWidget(powerGroup);

    connect(m_manualPowerBtn, &QPushButton::clicked, this, [this]() {
        if (m_devId == 0 || !RobotAPI::IsConnected(m_devId)) return;
        m_manualPowerBtn->setEnabled(false);
        m_manualPowerBtn->setText("动作中...");
        m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #9E9E9E; color: white; border-radius: 4px; }");

        bool isEnabled = false;
        int statusRet = RobotAPI::GetCurrentServoStatus(isEnabled, m_devId);
        if (statusRet != 0) {
            m_manualPowerBtn->setEnabled(true);
            updateRealTimeStatus();
            QMessageBox::warning(this, "读取失败", QString("读取伺服状态失败，错误码: %1").arg(statusRet));
            return;
        }

        int ret = isEnabled ? RobotAPI::PowerOffManual(m_devId)
                            : RobotAPI::PowerOnManual(m_devId);

        m_manualPowerBtn->setEnabled(true);
        if (ret != 0) {
            QMessageBox::warning(this, "操作失败", QString("%1失败！错误码: %2").arg(isEnabled ? "关闭手动使能" : "开启手动使能").arg(ret));
        }
        updateRealTimeStatus();
    });

    connect(refreshStatusBtn, &QPushButton::clicked, this, &RobotParameterDialog::updateRealTimeStatus);

    // ==========================================
    // 3. 运动控制按钮与状态显示区域
    // ==========================================
    QGroupBox* jogGroup = new QGroupBox("轴控点动 (按住运行，松开停止)", scrollContent);
    QVBoxLayout* jogMainLayout = new QVBoxLayout(jogGroup);

    QLabel* jogStatusLabel = new QLabel("状态: 就绪", scrollContent);
    jogStatusLabel->setStyleSheet("color: #E53935; font-weight: bold; font-size: 13px;");
    jogMainLayout->addWidget(jogStatusLabel);

    QWidget* gridWidget = new QWidget(scrollContent);
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    jogMainLayout->addWidget(gridWidget);

    QString btnStyle = "QPushButton { padding: 4px 15px; font-weight: bold; background-color: #e0e0e0; border: 1px solid #aaa; border-radius: 4px; font-size: 16px; }"
                       "QPushButton:pressed { background-color: #4CAF50; color: white; border: 1px solid #388E3C; }";
    QStringList axisNames = { "1 轴 (X)", "2 轴 (Y)", "3 轴 (Z)", "4 轴 (RX)", "5 轴 (RY)", "6 轴 (RZ)" };

    for (int i = 0; i < 6; ++i) {
        QLabel* lblAxis = new QLabel(axisNames[i], gridWidget);
        lblAxis->setFixedWidth(55);

        QPushButton* btnMinus = new QPushButton("-", gridWidget);
        QPushButton* btnPlus = new QPushButton("+", gridWidget);
        btnMinus->setFixedWidth(50);
        btnPlus->setFixedWidth(50);

        btnMinus->setStyleSheet(btnStyle); btnPlus->setStyleSheet(btnStyle);
        btnMinus->setCursor(Qt::PointingHandCursor); btnPlus->setCursor(Qt::PointingHandCursor);

        m_posLabels[i] = new QLabel("0.000", gridWidget);
        m_posLabels[i]->setStyleSheet("font-family: Consolas, monospace; font-size: 14px; color: #1976D2; font-weight: bold;");
        m_posLabels[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_posLabels[i]->setFixedWidth(70);

        // 依次放入网格的 0, 1, 2, 3 列
        gridLayout->addWidget(lblAxis, i, 0);
        gridLayout->addWidget(btnMinus, i, 1);
        gridLayout->addWidget(btnPlus, i, 2);
        gridLayout->addWidget(m_posLabels[i], i, 3);

        int axisIndex = i + 1; // 1~6
        connect(btnMinus, &QPushButton::pressed, this, [this, axisIndex, jogStatusLabel]() { handleJogButton(axisIndex, false, true, jogStatusLabel); });
        connect(btnMinus, &QPushButton::released, this, [this, axisIndex, jogStatusLabel]() { handleJogButton(axisIndex, false, false, jogStatusLabel); });
        connect(btnPlus, &QPushButton::pressed, this, [this, axisIndex, jogStatusLabel]() { handleJogButton(axisIndex, true, true, jogStatusLabel); });
        connect(btnPlus, &QPushButton::released, this, [this, axisIndex, jogStatusLabel]() { handleJogButton(axisIndex, true, false, jogStatusLabel); });
    }

    layout->addWidget(jogGroup);
    layout->addStretch(); // 撑起空白部分

    // ==========================================
    // 4. 将内容 Widget 塞入滚动区域，并放入页面
    // ==========================================
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    return page;
}

int RobotParameterDialog::sendJogCommand(int axisIndex, bool positive, bool pressed) const
{
    if (m_devId == 0 || axisIndex < 1 || axisIndex > 6) {
        return -1;
    }

    unsigned int devId = m_devId;
    switch (axisIndex) {
    case 1:
        return positive ? RobotAPI::Jog1Plus(pressed, devId) : RobotAPI::Jog1Minus(pressed, devId);
    case 2:
        return positive ? RobotAPI::Jog2Plus(pressed, devId) : RobotAPI::Jog2Minus(pressed, devId);
    case 3:
        return positive ? RobotAPI::Jog3Plus(pressed, devId) : RobotAPI::Jog3Minus(pressed, devId);
    case 4:
        return positive ? RobotAPI::Jog4Plus(pressed, devId) : RobotAPI::Jog4Minus(pressed, devId);
    case 5:
        return positive ? RobotAPI::Jog5Plus(pressed, devId) : RobotAPI::Jog5Minus(pressed, devId);
    case 6:
        return positive ? RobotAPI::Jog6Plus(pressed, devId) : RobotAPI::Jog6Minus(pressed, devId);
    default:
        return -1;
    }
}

void RobotParameterDialog::stopActiveJog()
{
    if (m_activeJogAxis < 1 || m_activeJogAxis > 6 || m_devId == 0) {
        m_activeJogAxis = 0;
        return;
    }

    if (RobotAPI::IsConnected(m_devId)) {
        sendJogCommand(m_activeJogAxis, m_activeJogPositive, false);
    }
    m_activeJogAxis = 0;
}

// ==========================================
// 点动与速度控制逻辑
// ==========================================
void RobotParameterDialog::handleJogButton(int axisIndex, bool positive, bool pressed, QLabel* statusLabel)
{
    if (m_devId == 0 || axisIndex < 1 || axisIndex > 6) {
        return;
    }

    if (!RobotAPI::IsConnected(m_devId)) return;

    if (pressed && m_activeJogAxis != 0) {
        stopActiveJog();
    }

    int ret = sendJogCommand(axisIndex, positive, pressed);

    if (!pressed && m_activeJogAxis == axisIndex && m_activeJogPositive == positive) {
        m_activeJogAxis = 0;
        if (m_servoTimer) {
            m_servoTimer->stop();
        }
    }

    if (ret == 0) {
        if (pressed) {
            m_activeJogAxis = axisIndex;
            m_activeJogPositive = positive;
            updatePositionValues();
            if (m_servoTimer && m_stackedWidget && m_stackedWidget->currentIndex() == 1) {
                m_servoTimer->start(100);
            }
        }
        statusLabel->setText(pressed ? "状态: 运行中..." : "状态: 就绪");
    } else {
        if (pressed && m_servoTimer) {
            m_servoTimer->stop();
        }
        statusLabel->setText(QString("%1被拒绝，错误码: %2").arg(pressed ? "启动" : "停止").arg(ret));
    }
}

void RobotParameterDialog::onSetSpeedClicked() {
    if (m_devId == 0) return;
    int ratio = m_speedSpinBox->value();
    m_setSpeedBtn->setEnabled(false);

    int ret = RobotAPI::SetGlobalSpeed(ratio, m_devId);
    m_setSpeedBtn->setEnabled(true);
    if (ret == 0) {
        QMessageBox::information(this, "成功", "全局速度设置成功！");
    } else {
        QMessageBox::warning(this, "失败", QString("设置速度失败，错误码: %1").arg(ret));
    }
}

void RobotParameterDialog::onIncreasePressed() {
    if (m_devId != 0) RobotAPI::IncreaseVelocity(true, m_devId);
}
void RobotParameterDialog::onIncreaseReleased() {
    if (m_devId != 0) RobotAPI::IncreaseVelocity(false, m_devId);
}
void RobotParameterDialog::onDecreasePressed() {
    if (m_devId != 0) RobotAPI::DecreaseVelocity(true, m_devId);
}
void RobotParameterDialog::onDecreaseReleased() {
    if (m_devId != 0) RobotAPI::DecreaseVelocity(false, m_devId);
}

void RobotParameterDialog::onServoTimerTimeout()
{
    if (m_isQueryingServo) return;

    if (m_activeJogAxis != 0 && m_stackedWidget && m_stackedWidget->currentIndex() == 1) {
        updatePositionValues();
    }
}

// ==========================================
// 核心状态刷新逻辑
// ==========================================
void RobotParameterDialog::updateRealTimeStatus()
{
    if (m_devId == 0 || !m_servoStatusLabel || !m_manualPowerBtn) return;

    m_isQueryingServo = true; // 上锁

    bool isEnabled = false;
    int ret = RobotAPI::GetCurrentServoStatus(isEnabled, m_devId);
    if (ret != 0) {
        m_servoStatusLabel->setText(QString("伺服状态: 读取失败 (%1)").arg(ret));
        m_servoStatusLabel->setStyleSheet("color: #FF9800; font-weight: bold; font-size: 13px;");
        m_manualPowerBtn->setEnabled(false);
        m_manualPowerBtn->setText("状态未知");
        m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #9E9E9E; color: white; border-radius: 4px; }");
        m_isQueryingServo = false; // 解锁
        return;
    }

    if (isEnabled) {
        m_servoStatusLabel->setText("伺服状态: 已使能 (ON)");
        m_servoStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold; font-size: 13px;");
        m_manualPowerBtn->setEnabled(true);
        m_manualPowerBtn->setText("关闭手动使能");
        m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #E53935; color: white; border-radius: 4px; }");
    } else {
        m_servoStatusLabel->setText("伺服状态: 未使能 (OFF)");
        m_servoStatusLabel->setStyleSheet("color: #E53935; font-weight: bold; font-size: 13px;");
        m_manualPowerBtn->setEnabled(true);
        m_manualPowerBtn->setText("开启手动使能");
        m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #4CAF50; color: white; border-radius: 4px; }");
    }

    m_isQueryingServo = false; // 解锁
}

void RobotParameterDialog::updatePositionValues()
{
    if (m_devId == 0) return;

    RobotAPI::PosData posData = {};
    int retPos = RobotAPI::GetPositionData(posData, m_devId);

    if (retPos == 0) {
        for (int i = 0; i < 6; ++i) {
            if (!m_posLabels[i]) continue;

            double val = 0.0;
            if (m_currentJogMode == 0) {
                val = posData.acsPos[i];
            } else if (m_currentJogMode == 3) {
                val = posData.pcsPos[i];
            } else {
                val = posData.kcsPos[i];
            }

            m_posLabels[i]->setText(QString::number(val, 'f', 3));
        }
    }
}

// ==========================================
// 独立功能：后台拉取工具/用户坐标系列表及当前选中项
// ==========================================
void RobotParameterDialog::updateCoordinateSystems()
{
    if (m_devId == 0 || !m_toolCombo || !m_userCombo) return;

    QStringList tools, users;
    QString curToolStr = "tool0", curUserStr = "wobj0";

    for (int i = 0; i <= 31; ++i) {
        tools << QString("tool%1").arg(i);
        users << QString("wobj%1").arg(i);
    }

    std::string currentTool;
    if (RobotAPI::GetCurrentToolName(currentTool, m_devId) == 0) {
        curToolStr = QString::fromStdString(currentTool);
    }

    std::string currentUser;
    if (RobotAPI::GetCurrentUframeName(currentUser, m_devId) == 0) {
        curUserStr = QString::fromStdString(currentUser);
    }

    m_isUpdatingCoords = true;

    m_toolCombo->clear();
    m_toolCombo->addItems(tools);
    m_toolCombo->setCurrentText(curToolStr);

    m_userCombo->clear();
    m_userCombo->addItems(users);
    m_userCombo->setCurrentText(curUserStr);

    m_isUpdatingCoords = false;
}

// ==========================================
// 事件过滤器：全局拦截特定控件的鼠标及按键事件
// ==========================================
bool RobotParameterDialog::eventFilter(QObject *obj, QEvent *event)
{
    // 如果触发的是鼠标滚轮事件
    if (event->type() == QEvent::Wheel) {
        // 判断被滚动的是不是 QComboBox
        if (qobject_cast<QComboBox*>(obj)) {
            // 忽略该事件，告诉 Qt 当前控件(下拉框)不处理滚轮
            event->ignore();
            return true;
        }
    }
    // 对于其他事件或控件，按 Qt 默认逻辑继续处理
    return QDialog::eventFilter(obj, event);
}
