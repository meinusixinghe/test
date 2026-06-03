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
        updateRealTimeStatus();
        updateCoordinateSystems();
        m_servoTimer->start(500);
    } else {
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
        runAsyncCommand([this, mode]() {
            RobotAPI::SetJogMode(mode, m_devId);
        });
    });

    modeLayout->addWidget(new QLabel("当前工具 (Tool):"), 1, 0);
    m_toolCombo = new QComboBox(scrollContent);
    m_toolCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    m_toolCombo->installEventFilter(this);
    modeLayout->addWidget(m_toolCombo, 1, 1);

    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_isUpdatingCoords || index < 0 || m_devId == 0) return;
        std::string toolName = m_toolCombo->itemText(index).toStdString();
        runAsyncCommand([this, toolName]() {
            int ret = RobotAPI::SetCurrentToolByName(toolName, m_devId);
            if (ret != 0) qDebug() << "设置工具坐标系失败，错误码:" << ret;
        });
    });

    modeLayout->addWidget(new QLabel("当前用户 (User):"), 2, 0);
    m_userCombo = new QComboBox(scrollContent);
    m_userCombo->setStyleSheet("QComboBox { padding: 4px; font-size: 13px; }");
    m_userCombo->installEventFilter(this);
    modeLayout->addWidget(m_userCombo, 2, 1);

    connect(m_userCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_isUpdatingCoords || index < 0 || m_devId == 0) return;
        std::string userName = m_userCombo->itemText(index).toStdString();
        runAsyncCommand([this, userName]() {
            int ret = RobotAPI::SetCurrentUframeByName(userName, m_devId);
            if (ret != 0) qDebug() << "设置用户坐标系失败，错误码:" << ret;
        });
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
        if (m_devId == 0) return;
        m_manualPowerBtn->setEnabled(false);
        m_manualPowerBtn->setText("动作中...");
        m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #9E9E9E; color: white; border-radius: 4px; }");

        runAsyncCommand([this]() {
            bool isEnabled = false;
            RobotAPI::GetCurrentServoStatus(isEnabled, m_devId);

            int ret = -1;
            if (isEnabled) {
                ret = RobotAPI::PowerOffManual(m_devId);
            } else {
                RobotAPI::GetPermission(m_devId);
                ret = RobotAPI::PowerOnManual(m_devId);
            }

            QMetaObject::invokeMethod(this, [this, ret, isEnabled]() {
                m_manualPowerBtn->setEnabled(true);
                if (ret != 0) {
                    QMessageBox::warning(this, "操作失败", QString("%1失败！错误码: %2").arg(isEnabled ? "下电" : "上电").arg(ret));
                }
            }, Qt::QueuedConnection);
        });
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

    // ==========================================
    // 5. 打开面板时的初始化操作
    // ==========================================
    if (m_devId != 0) {
        unsigned int devId = m_devId;
        runAsyncCommand([this, devId]() {
            RobotAPI::GetPermission(devId);
            RobotAPI::SetJogMode(0, devId);

            QMetaObject::invokeMethod(this, &RobotParameterDialog::updateRealTimeStatus, Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, &RobotParameterDialog::updateCoordinateSystems, Qt::QueuedConnection);
        });
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

    // ==========================================
    // 👇 核心修复：必须把网络发送放到子线程中，绝对不阻塞 UI
    // ==========================================
    runAsyncCommand([this, axisIndex, positive, pressed, statusLabel, devId]() {

        // 1. 每次按下或松开前，必须强制确认获取一次操作令牌！(极其关键)
        RobotAPI::GetPermission(devId);

        int ret = 0;

        // 2. 发送 Jog 指令
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

        QMetaObject::invokeMethod(this, [statusLabel, pressed, ret]() {
            if (ret == 0) {
                statusLabel->setText(pressed ? "状态: 运行中..." : "状态: 就绪");
            } else {
                statusLabel->setText(QString("%1失败，错误码: %2")
                                         .arg(pressed ? "启动" : "停止")
                                         .arg(ret));
            }
        }, Qt::QueuedConnection);
    });
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

// ==========================================
// 定时器触发：后台静默拉取状态
// ==========================================
void RobotParameterDialog::onServoTimerTimeout()
{
    // 如果上一次查询线程还没结束，直接跳过这次，防止 UI 卡顿
    if (m_isQueryingServo) return;

    updateRealTimeStatus();
}

// ==========================================
// 核心状态刷新逻辑
// ==========================================
void RobotParameterDialog::updateRealTimeStatus()
{
    if (m_devId == 0 || !m_servoStatusLabel || !m_manualPowerBtn) return;

    m_isQueryingServo = true; // 上锁

    runAsyncCommand([this]() {
        // 1. 获取伺服状态
        bool isEnabled = false;
        RobotAPI::GetCurrentServoStatus(isEnabled, m_devId);

        // 2. 获取机器人位置
        RobotAPI::PosData posData;
        int retPos = RobotAPI::GetPositionData(posData, m_devId);

        // 切回主线程更新 UI
        QMetaObject::invokeMethod(this, [this, isEnabled, posData, retPos]() {
            // 更新伺服 UI
            if (isEnabled) {
                m_servoStatusLabel->setText("伺服状态: 已上电 (ON)");
                m_servoStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold; font-size: 13px;");
                m_manualPowerBtn->setText("手动模式下电");
                m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #E53935; color: white; border-radius: 4px; }");
            } else {
                m_servoStatusLabel->setText("伺服状态: 已下电 (OFF)");
                m_servoStatusLabel->setStyleSheet("color: #E53935; font-weight: bold; font-size: 13px;");
                m_manualPowerBtn->setText("手动模式上电");
                m_manualPowerBtn->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #4CAF50; color: white; border-radius: 4px; }");
            }

            // 更新位置坐标 UI
            if (retPos == 0) {
                for (int i = 0; i < 6; ++i) {
                    if (!m_posLabels[i]) continue;

                    double val = 0.0;
                    // 根据 SDK 的 PosData 结构体：
                    // 0: 关节模式 -> acsPos
                    // 1: 基座模式 -> kcsPos
                    // 2: 工具模式 -> (通常以基座呈现TCP，故用 kcsPos)
                    // 3: 用户模式 -> pcsPos
                    if (m_currentJogMode == 0) {
                        val = posData.acsPos[i];
                    } else if (m_currentJogMode == 3) {
                        val = posData.pcsPos[i];
                    } else {
                        val = posData.kcsPos[i];
                    }

                    // 格式化为保留 3 位小数的字符串 (工业标准毫米/度)
                    m_posLabels[i]->setText(QString::number(val, 'f', 3));
                }
            }

            m_isQueryingServo = false; // 解锁
        }, Qt::QueuedConnection);
    });
}

// ==========================================
// 独立功能：后台拉取工具/用户坐标系列表及当前选中项
// ==========================================
void RobotParameterDialog::updateCoordinateSystems()
{
    if (m_devId == 0 || !m_toolCombo || !m_userCombo) return;

    runAsyncCommand([this]() {
        std::vector<std::string> toolList;
        std::vector<std::string> userList;
        std::string currentTool;
        std::string currentUser;

        // SDK 调用：获取列表
        RobotAPI::GetToolNameList(toolList, m_devId);
        RobotAPI::GetUserNameList(userList, m_devId);

        // SDK 调用：获取当前被选中的名称
        RobotAPI::GetCurrentToolName(currentTool, m_devId);
        RobotAPI::GetCurrentUframeName(currentUser, m_devId);

        // 获取完毕后，切回主 UI 线程进行渲染
        QMetaObject::invokeMethod(this, [this, toolList, userList, currentTool, currentUser]() {
            // 👇【加锁】：屏蔽 comboBox 的 onChange 信号触发，防止数据刚填入就被当成用户点击下发给机器人
            m_isUpdatingCoords = true;

            // 1. 刷新 Tool 下拉框
            m_toolCombo->clear();
            for (const auto& name : toolList) {
                m_toolCombo->addItem(QString::fromStdString(name));
            }
            int toolIdx = m_toolCombo->findText(QString::fromStdString(currentTool));
            if (toolIdx >= 0) {
                m_toolCombo->setCurrentIndex(toolIdx);
            }

            // 2. 刷新 User 下拉框
            m_userCombo->clear();
            for (const auto& name : userList) {
                m_userCombo->addItem(QString::fromStdString(name));
            }
            int userIdx = m_userCombo->findText(QString::fromStdString(currentUser));
            if (userIdx >= 0) {
                m_userCombo->setCurrentIndex(userIdx);
            }

            // 👇【解锁】：渲染完毕，重新接受用户手动点击
            m_isUpdatingCoords = false;
        }, Qt::QueuedConnection);
    });
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
