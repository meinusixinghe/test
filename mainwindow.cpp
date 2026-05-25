#include "mainwindow.h"
#include "EfortSdk.h"
#include "renderarea.h"
#include "usercoordinatemanager.h"
#include "rotationmatrixdialog.h"
#include "connectiondialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QProcess>
#include <QToolBar>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QTemporaryFile>
#include <QSplitter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialog>
#include <QStatusBar>
#include <QInputDialog>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include <QShortcut>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QActionGroup>
#include <QCalendarWidget>
#include <QDir>
#include <QDateTime>
#include <QTextStream>

FloatingToolWidget::FloatingToolWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    // 设置工具箱的现代 UI 风格
    setStyleSheet("FloatingToolWidget { background-color: rgba(245, 246, 247, 230); border: 1px solid #C0C0C0; border-radius: 6px; }"
                  "QPushButton { background-color: white; border: 1px solid #D0D0D0; border-radius: 4px; padding: 4px; color: #333; font-weight: bold; }"
                  "QPushButton:hover { background-color: #E8F0FE; border: 1px solid #A0C8F0; }"
                  "QPushButton:checked { background-color: #DDEEFE; border: 1px solid #80B8FF; color: #0055A4; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 6);
    mainLayout->setSpacing(0);

    // 顶部标题栏和关闭按钮
    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    QLabel *title = new QLabel("图纸处理工具");
    title->setStyleSheet("font-size: 11px; color: #666; font-weight: normal; border: none; background: transparent;");
    btnClose = new QPushButton("×");
    btnClose->setFixedSize(18, 18);
    btnClose->setStyleSheet("QPushButton { font-size: 14px; font-weight: bold; color: #999; padding: 0px; border: none; background: transparent; }"
                            "QPushButton:hover { color: white; background-color: #FF4444; border-radius: 9px; }");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(btnClose);

    // 底部工具按钮
    QHBoxLayout *tools = new QHBoxLayout();
    tools->setSpacing(4);
    btnRestore = new QPushButton();
    btnRestore->setIcon(QIcon(":/img/images/restore.png"));
    btnRestore->setIconSize(QSize(16, 16));
    btnRestore->setFixedSize(36, 24);
    btnRestore->setToolTip("还原图纸");
    btnRestore->setCursor(Qt::PointingHandCursor);
    btnUndo = new QPushButton();
    btnUndo->setIcon(QIcon(":/img/images/undo.png"));
    btnUndo->setIconSize(QSize(16, 16));
    btnUndo->setFixedSize(36, 24);
    btnUndo->setToolTip("撤销操作 (Ctrl+Z)");
    btnUndo->setCursor(Qt::PointingHandCursor);
    btnUndo->setEnabled(false);
    btnEraser = new QPushButton();
    btnEraser->setIcon(QIcon(":/img/images/eraser.png"));
    btnEraser->setIconSize(QSize(16, 16));
    btnEraser->setFixedSize(36, 24);
    btnEraser->setToolTip("橡皮擦");
    btnEraser->setCheckable(true);
    btnEraser->setCursor(Qt::PointingHandCursor);
    btnLasso = new QPushButton();
    btnLasso->setIcon(QIcon(":/img/images/lasso.png"));
    btnLasso->setIconSize(QSize(16, 16));
    btnLasso->setFixedSize(36, 24);
    btnLasso->setToolTip("框选线条 (按 Delete 键删除)");
    btnLasso->setCheckable(true);
    btnLasso->setCursor(Qt::PointingHandCursor);
    btnMove = new QPushButton();
    btnMove->setIcon(QIcon(":/img/images/shift.png"));
    btnMove->setIconSize(QSize(16, 16));
    btnMove->setFixedSize(36, 24);
    btnMove->setToolTip("移动线条 (框选后按回车选基准点)");
    btnMove->setCheckable(true);
    btnMove->setCursor(Qt::PointingHandCursor);

    tools->addWidget(btnRestore);
    tools->addWidget(btnUndo);
    tools->addWidget(btnLasso);
    tools->addWidget(btnMove);
    tools->addWidget(btnEraser);

    mainLayout->addLayout(header);
    mainLayout->addLayout(tools);

    sliderContainer = new QWidget(this);
    QHBoxLayout *sliderLayout = new QHBoxLayout(sliderContainer);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(6);

    QLabel *lblSliderTitle = new QLabel("大小:");
    lblSliderTitle->setStyleSheet("font-size: 11px; color: #555; border: none; background: transparent;");

    sliderEraserSize = new QSlider(Qt::Horizontal);
    sliderEraserSize->setRange(15, 25); // 橡皮擦大小范围
    sliderEraserSize->setValue(20);    // 默认 20
    sliderEraserSize->setStyleSheet(
        "QSlider {"
        "   background: transparent;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 4px;"                /* 细细的灰色滑轨 */
        "   background: #E0E0E0;"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: #2196F3;"        /* 划过部分的蓝色轨迹 */
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #2196F3;"        /* 纯蓝色的圆形小滑块 */
        "   border: none;"
        "   width: 12px;"                /* 宽度和高度缩小到 12px */
        "   height: 12px;"
        "   margin: -4px 0;"             /* 上下偏移，让滑块完美跨在 4px 的滑轨中心 */
        "   border-radius: 6px;"         /* 圆角设为高的一半，变成纯圆 */
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #42A5F5;"        /* 鼠标放上去时颜色稍微变亮 */
        "}"
        );
    lblEraserSize = new QLabel("20");
    lblEraserSize->setFixedWidth(20);
    lblEraserSize->setStyleSheet("font-size: 11px; color: #333; border: none; background: transparent;");

    sliderLayout->addWidget(lblSliderTitle);
    sliderLayout->addWidget(sliderEraserSize);
    sliderLayout->addWidget(lblEraserSize);

    mainLayout->addWidget(sliderContainer);

    sliderContainer->setVisible(false);
    setFixedWidth(220);
    adjustSize();

    setCursor(Qt::ArrowCursor);
}

void FloatingToolWidget::mousePressEvent(QMouseEvent *event) {
    // 记录鼠标按下的相对位置，用于拖拽
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FloatingToolWidget::mouseMoveEvent(QMouseEvent *event) {
    // 鼠标拖拽移动悬浮框
    if (event->buttons() & Qt::LeftButton) {
        QPoint newPos = event->globalPosition().toPoint() - m_dragPosition;
        // 限制它不要被拖出绘图区边界
        if (parentWidget()) {
            newPos.setX(qBound(0, newPos.x(), parentWidget()->width() - width()));
            newPos.setY(qBound(0, newPos.y(), parentWidget()->height() - height()));
        }
        move(newPos);
        event->accept();
    }
}

void FloatingToolWidget::setSliderVisible(bool visible) {
    sliderContainer->setVisible(visible);
    // 强制窗口重新调整布局和尺寸
    this->adjustSize();
}

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent)
{
    // 1. 初始化 UI
    setupUi();

    // 2. 初始化 Modbus 管理器
    m_modbusManager = new ModbusManager(this);

    // 3. 基础状态与日志信号绑定
    connect(m_modbusManager, &ModbusManager::connectionStateChanged, this, &MainWindow::onModbusStateChanged);
    connect(m_modbusManager, &ModbusManager::logMessage, this, &MainWindow::showAndSaveLog);
    connect(m_modbusManager, &ModbusManager::servoStateChanged, this, &MainWindow::onServoStateChanged);
    connect(m_modbusManager, &ModbusManager::autoStateChanged, this, &MainWindow::onAutoStateChanged);

    // 4. 业务逻辑信号绑定

    // 4.1 错误拦截：断网或急停时，除了弹窗，还必须中断连续焊接状态
    connect(m_modbusManager, &ModbusManager::errorOccurred, this, [this](QString msg){
        showAndSaveLog("错误: " + msg);
        QMessageBox::warning(this, "通讯错误", msg);
        if (m_isWeldingProcessRunning) {
            m_isWeldingProcessRunning = false;
            m_startBtn->setText("预约");
            m_startBtn->setEnabled(true);
            m_pauseBtn->setVisible(false);
        }
    });

    // 4.2 动态按钮文字：不仅改字，还要监控底层状态回退
    connect(m_modbusManager, &ModbusManager::startButtonTextChanged, this, [this](QString text) {
        m_startBtn->setText(text);
        if (text == "预约"|| text == "启动") {
            m_isWeldingProcessRunning = false;
            m_startBtn->setEnabled(true);
            m_pauseBtn->setVisible(false);
        }
    });

    // 4.3 收到总命令握手完成信号，立刻下发第一个孔的数据
    connect(m_modbusManager, &ModbusManager::cmdHandshakeCompleted, this, [this]() {
        if (m_isWeldingProcessRunning) {
            sendNextWeldHole();
        }
    });

    // 4.4 单孔闭环完成，自动触发下一个！
    connect(m_modbusManager, &ModbusManager::jobSentSuccess, this, [this]() {
        if (m_isWeldingProcessRunning) {
            QApplication::processEvents();
            m_currentWeldIndex++; // 索引加 1
            sendNextWeldHole();   // 自动调取下一行数据发送给机器人
        }
    });

    // 5. 初始化数据与自动连接
    loadWeldingProcesses();  // 启动时自动加载焊接工艺数据

    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // 读取本地保存的 IP 和 端口配置，并尝试自动连接
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    m_lastIp = settings.value("RobotIP", "").toString();
    m_lastPort = settings.value("RobotPort", 502).toInt();

    if (!m_lastIp.isEmpty()) {
        m_modbusManager->connectToRobot(m_lastIp, m_lastPort);

        int ret = RobotAPI::ConnectRobot(m_lastIp.toStdString().c_str(), m_currentDevId, true);

        if (ret == ERROR_OK || ret == ERROR_DEVICE_EXIT) { // ERROR_OK=0, ERROR_DEVICE_EXIT=10021
            RobotAPI::SelectRobot(m_currentDevId);
            qDebug() << "SDK 自动连接成功，设备 ID:" << m_currentDevId;
        } else {
            qDebug() << "SDK 自动连接失败，错误码:" << ret;
            m_currentDevId = 0; // 失败必须归零
        }
    } else {
        m_lastIp = "192.168.1.10";
        m_lastPort = 502;
    }
}

MainWindow::~MainWindow() {
    if (m_coordManager) {
        delete m_coordManager;
    }
}

void MainWindow::setupUi()
{
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);                                  // 水平分割器
    splitter->setObjectName("mainSplitter");
    setCentralWidget(splitter);

    splitter->setContentsMargins(10, 10, 10, 10);
    splitter->setHandleWidth(12);
    splitter->setStyleSheet(
        "#mainSplitter { background-color: #F5F6F7; }"
        "#mainSplitter::handle { background: transparent; }"
        );
    renderArea = new RenderArea(splitter);
    renderArea->setObjectName("renderPanel");
    renderArea->setMinimumSize(800, 400);
    renderArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    renderArea->setAutoFillBackground(false);
    renderArea->setAttribute(Qt::WA_StyledBackground, true);
    renderArea->setStyleSheet(
        "#renderPanel {"
        "   background-color: #FFFFFF;"  /* 纯白背景 */
        "   border: 1px solid #E4E4E4;"  /* 与右侧一致的淡灰边框 */
        "   border-radius: 8px;"         /* 8px 圆角 */
        "}"
        );
    splitter->addWidget(renderArea);

    // 显示/隐藏用户坐标系
    QVBoxLayout* overlayLayout = new QVBoxLayout(renderArea);
    overlayLayout->setContentsMargins(15, 15, 15, 15);
    overlayLayout->addStretch();
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();

    m_toggleCoordBtn = new QPushButton("显示用户坐标系", renderArea);
    m_toggleCoordBtn->setCheckable(true);
    m_toggleCoordBtn->setChecked(true);                                                         // 默认显示
    m_toggleCoordBtn->setVisible(false);                                                        // 初始不可见
    m_toggleCoordBtn->setMinimumWidth(120);                                                     // 按钮最小宽度，避免文字挤压
    m_toggleCoordBtn->setCursor(Qt::ArrowCursor);
    QString btnStyle = R"(
        QPushButton { padding: 6px 12px; background-color: rgba(255, 255, 255, 0.95); border: 1px solid #ccc; border-radius: 4px;}
        QPushButton:checked { background-color: #2196F3; color: white; border-color: #2196F3;}
        QPushButton:hover {border-color: #2196F3; }
    )";
    m_toggleCoordBtn->setStyleSheet(btnStyle);

    bottomBtnLayout->addWidget(m_toggleCoordBtn);

    m_startBtn = new QPushButton("预约", renderArea);
    m_startBtn->setStyleSheet("QPushButton { background-color: rgba(76, 175, 80, 0.95); color: white; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #45a049; }");
    m_startBtn->setCursor(Qt::ArrowCursor);
    m_startBtn->setVisible(false);

    m_pauseBtn = new QPushButton("暂停", renderArea);
    m_pauseBtn->setCheckable(true); // 可切换暂停/继续
    m_pauseBtn->setStyleSheet("QPushButton { background-color: rgba(255, 152, 0, 0.95); color: white; border-radius: 4px; padding: 6px 12px; } QPushButton:checked { background-color: #e65100; text-decoration: underline; }");
    m_pauseBtn->setCursor(Qt::ArrowCursor);
    m_pauseBtn->setVisible(false);

    m_resetBtn = new QPushButton("复位", renderArea);
    m_resetBtn->setStyleSheet("QPushButton { background-color: rgba(244, 67, 54, 0.95); color: white; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #d32f2f; }");
    m_resetBtn->setCursor(Qt::ArrowCursor);
    m_resetBtn->setVisible(false);

    m_powerBtn = new QPushButton("机器人上电", renderArea);
    m_powerBtn->setStyleSheet(btnStyle);
    m_powerBtn->setCursor(Qt::PointingHandCursor);

    bottomBtnLayout->addWidget(m_startBtn);
    bottomBtnLayout->addWidget(m_pauseBtn);
    bottomBtnLayout->addWidget(m_resetBtn);
    bottomBtnLayout->addWidget(m_powerBtn);
    bottomBtnLayout->addStretch();
    overlayLayout->addLayout(bottomBtnLayout);

    // 2. 右侧垂直分割器 (上：表格，下：详细信息)
    rightSplitter = new QSplitter(Qt::Vertical, splitter);
    rightSplitter->setHandleWidth(8);
    rightSplitter->setStyleSheet(
        "QSplitter { background: transparent; }"
        "QSplitter::handle { background: transparent; }"
        "QScrollBar:vertical {"
        "   border: none;"
        "   background: transparent;"
        "   width: 8px;"                /* 滚动条宽度变细 */
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: #D0D0D0;"       /* 滚动条滑块颜色 */
        "   min-height: 20px;"
        "   border-radius: 4px;"        /* 滑块圆角 */
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: #A0A0A0;"       /* 鼠标悬浮时颜色加深 */
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"               /* 隐藏上下箭头按钮 */
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: transparent;"   /* 滚动条滑轨背景透明 */
        "}"
        );
    splitter->addWidget(rightSplitter);
    QWidget *tableContainer = new QWidget(rightSplitter);
    tableContainer->setObjectName("tablePanel");
    tableContainer->setAttribute(Qt::WA_StyledBackground, true);
    tableContainer->setStyleSheet(
        "#tablePanel {"
        "   background-color: #FFFFFF;"  // 纯白背景
        "   border: 1px solid #E4E4E4;"  // 淡灰边框
        "   border-radius: 8px;"         // 圆角
        "}"
        );
    QGraphicsDropShadowEffect *tableShadow = new QGraphicsDropShadowEffect(tableContainer);
    tableShadow->setOffset(0, 4);
    tableShadow->setColor(QColor(0, 0, 0, 40));
    tableShadow->setBlurRadius(15);
    tableContainer->setGraphicsEffect(tableShadow);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(2, 2, 2, 6); // 留一点内边距，让内部表格不溢出圆角
    tableLayout->setSpacing(0);
    dataTable = new QTableWidget(rightSplitter);
    dataTable->setMinimumSize(200, 200);
    dataTable->setColumnCount(2);
    dataTable->setHorizontalHeaderLabels({"序号", "线条类型"});
    dataTable->verticalHeader()->setVisible(false);
    dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    dataTable->setColumnWidth(0, 50);
    dataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    dataTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    dataTable->setShowGrid(false);
    dataTable->setFocusPolicy(Qt::NoFocus);
    dataTable->setStyleSheet(
        "QTableWidget {"
        "   border: none;"
        "   background-color: transparent;"
        "   outline: none;"
        "}"
        "QTableWidget::item {"
        "   border-bottom: 1px solid #F0F0F0;" // 极淡的行间分割线
        "   padding: 4px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #E3F2FD;"  // 选中的淡蓝背景
        "   color: #1976D2;"             // 选中的深蓝字体
        "}"
        "QHeaderView::section {"
        "   background-color: #F0F4F8;"  // 表头淡灰背景
        "   border: none;"
        "   border-bottom: 1px solid #D0D5DD;"
        "   font-weight: bold;"
        "   font-size: 15px;"
        "   color: #333;"
        "   padding: 6px;"
        "}"
        );
    dataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dataTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::showTableContextMenu);
    tableLayout->addWidget(dataTable);
    rightSplitter->addWidget(tableContainer);

    // 创建底部的详细信息视窗
    detailWidget = new QWidget(rightSplitter);
    detailWidget->setObjectName("detailPanel");
    detailWidget->setAttribute(Qt::WA_StyledBackground, true);
    detailWidget->setStyleSheet(
        "#detailPanel {"
        "   background-color: #FFFFFF;"  // 极淡的灰白色，视觉上非常舒适且不突兀
        "   border: 1px solid #E4E4E4;"  // 淡灰边框勾勒轮廓
        "   border-radius: 8px;"         // 现代化的圆角
        "}"
        );
    // 创建图形阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(detailWidget);
    shadowEffect->setOffset(0, 0);                  // 偏移量设为 0，让阴影向四周均匀发散
    shadowEffect->setColor(QColor(0, 0, 0, 30));    // 颜色为极淡的半透明黑色 (Alpha 30)
    shadowEffect->setBlurRadius(15);                // 较大的模糊半径，呈现悬浮的层次感
    detailWidget->setGraphicsEffect(shadowEffect);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(10, 10, 10, 10);
    // 带有 X 关闭按钮的表头
    QWidget* detailTitleWidget = new QWidget(detailWidget);
    QHBoxLayout* titleLayout = new QHBoxLayout(detailTitleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* detailTitle = new QLabel("详细信息", detailTitleWidget);
    detailTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    QPushButton* closeDetailBtn = new QPushButton("✕", detailTitleWidget);
    closeDetailBtn->setFixedSize(26, 26);
    closeDetailBtn->setStyleSheet("QPushButton { color: #FF4444; font-size: 16px; font-weight: bold; border: none; border-radius: 4px; } "
                                  "QPushButton:hover { background-color: #FFE6E6; }");
    titleLayout->addWidget(detailTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(closeDetailBtn);

    // 详细信息占位符内容
    m_detailContentText = new QTextEdit(detailWidget);
    m_detailContentText->setReadOnly(true);
    m_detailContentText->setStyleSheet("color: #555; font-size: 12px; line-height: 1.6; font-weight: normal; border: none; background: transparent;");
    m_detailContentText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_detailContentText->setMaximumHeight(160);
    QWidget* bevelWidget = new QWidget(detailWidget);
    QHBoxLayout* bevelLayout = new QHBoxLayout(bevelWidget);
    bevelLayout->setContentsMargins(0, 5, 0, 0);
    bevelLayout->setSpacing(10);
    QLabel* lblBevel = new QLabel("坡口角度:", bevelWidget);
    lblBevel->setStyleSheet("color: #333; font-size: 12px;");
    m_bevelAngleSpin = new QDoubleSpinBox(bevelWidget);
    m_bevelAngleSpin->setRange(-90.0, 90.0);
    m_bevelAngleSpin->setSuffix(" °");
    m_bevelAngleSpin->setDecimals(1);
    m_bevelAngleSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QLabel* lblRootFace = new QLabel("顿边高度:", bevelWidget);
    lblRootFace->setStyleSheet("color: #333; font-size: 12px;");
    m_rootFaceSpin = new QDoubleSpinBox(bevelWidget);
    m_rootFaceSpin->setRange(0.0, 100.0);
    m_rootFaceSpin->setSuffix(" mm");
    m_rootFaceSpin->setDecimals(2);
    m_rootFaceSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bevelLayout->addWidget(lblBevel);
    bevelLayout->addWidget(m_bevelAngleSpin);
    bevelLayout->addWidget(lblRootFace);
    bevelLayout->addWidget(m_rootFaceSpin);
    connect(m_bevelAngleSpin, &QDoubleSpinBox::valueChanged, this, &MainWindow::onBevelParametersChanged);
    connect(m_rootFaceSpin, &QDoubleSpinBox::valueChanged, this, &MainWindow::onBevelParametersChanged);
    detailLayout->addWidget(detailTitleWidget);
    detailLayout->addWidget(m_detailContentText);
    detailLayout->addWidget(bevelWidget);
    detailLayout->addStretch();
    rightSplitter->addWidget(detailWidget);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 1);
    detailWidget->hide();

    // X 按钮点击事件：关闭窗口、清除表格选中、清除左侧高亮
    connect(closeDetailBtn, &QPushButton::clicked, this, [this](){
        dataTable->clearSelection();
        dataTable->setCurrentItem(nullptr);
        detailWidget->hide();
        renderArea->setHighlightedPathIndices(QList<int>());
    });

    // 3. 设置初始比例：左侧占 3份，右侧占 1份
    splitter->setCollapsible(0, false);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    // 4. 创建菜单栏
    menuBar()->hide();
    // --- 提前创建好所有的功能 Action (有图标就传图标，没有就只传文字) ---
    loadAction = new QAction(QIcon(":/img/images/import.png"), "导入DXF", this);
    rotateAction = new QAction("应用旋转矩阵", this);
    m_setupCoordAction = new QAction(QIcon(":/img/images/chuangjianzuobiaoxi.png"), "建立用户坐标系", this);
    m_manageProcessAction = new QAction(QIcon(":/img/images/icons4.png"), "焊接工艺管理", this);
    m_imageProcessAction = new QAction(QIcon(":/img/images/DrawProcessing.png"), "图纸处理", this);
    QAction* m_positioningAction = new QAction("建立定位", this);
    m_connectAction = new QAction(QIcon(":/img/images/icons6.png"), "建立连接", this);
    // --- 创建第一层：“选项卡”栏 ---
    QToolBar* tabBar = addToolBar("选项卡");
    tabBar->setMovable(false); // 禁止拖动
    tabBar->setStyleSheet(
        "QToolBar { background-color: #FFFFFF; border-bottom: 1px solid #E4E4E4; padding: 0px; margin: 0px; }"
        "QToolButton { padding: 4px 20px; font-size: 14px; border: none; background: transparent; color: #555; }"
        "QToolButton:checked { border-bottom: 3px solid #2196F3; color: #2196F3; font-weight: bold; }"
        "QToolButton:hover:!checked { background-color: #F5F5F5; color: #333; }"
        );
    QActionGroup* tabGroup = new QActionGroup(this);
    tabGroup->setExclusive(true);
    QAction* tabFile = new QAction("文件", this); tabFile->setCheckable(true); tabGroup->addAction(tabFile);
    QAction* tabOperation = new QAction("操作", this); tabOperation->setCheckable(true); tabGroup->addAction(tabOperation);
    QAction* tabTools = new QAction("工具", this); tabTools->setCheckable(true); tabGroup->addAction(tabTools);
    QAction* tabConnect = new QAction("连接", this); tabConnect->setCheckable(true); tabGroup->addAction(tabConnect);
    tabBar->addAction(tabFile);
    tabBar->addAction(tabOperation);
    tabBar->addAction(tabTools);
    tabBar->addAction(tabConnect);
    // --- 创建第二层：动态内容工具栏 ---
    addToolBarBreak();
    toolBar = addToolBar("内容工具栏");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(30, 30)); // 图标大小
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); // 文字在图标下方
    toolBar->setStyleSheet(
        "QToolBar { background-color: #FAFAFB; border-bottom: 1px solid #E4E4E4; padding: 4px; }"
        "QToolButton { padding: 6px; border: 1px solid transparent; border-radius: 4px; }"
        "QToolButton:hover { background-color: #E3F2FD; border: 1px solid #90CAF9; }"
        );

    // 5. 绑定选项卡点击逻辑，动态切换下方工具栏的内容
    auto switchTab = [=](QAction* currentTab) {
        toolBar->clear(); // 切换时先清空下方工具栏
        if (currentTab == tabFile) {
            toolBar->addAction(loadAction);
        } else if (currentTab == tabOperation) {
            toolBar->addAction(rotateAction);
            toolBar->addAction(m_setupCoordAction);
            toolBar->addAction(m_manageProcessAction);
        } else if (currentTab == tabTools) {
            toolBar->addAction(m_imageProcessAction);
            toolBar->addAction(m_positioningAction);
        } else if (currentTab == tabConnect) {
            toolBar->addAction(m_connectAction);
        }
    };
    connect(tabGroup, &QActionGroup::triggered, this, switchTab);
    // 打开软件默认处于“文件”状态
    tabFile->setChecked(true);
    switchTab(tabFile);

    // 6. 状态标签
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setCursor(Qt::PointingHandCursor);
    m_statusLabel->setToolTip("点击查看历史日志");
    m_statusLabel->installEventFilter(this);
    statusBar()->addWidget(m_statusLabel);

    // 工具栏上的状态指示器容器 (网络 + 伺服)
    QWidget* statusContainer = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusContainer);
    statusLayout->setContentsMargins(10, 0, 10, 0);
    statusLayout->setSpacing(6);
    // 获取基础字体
    QFont statusFontObj = font();
    statusFontObj.setPointSize(9);
    // 网络连接状态
    m_statusIconLabel = new QLabel(this);
    m_statusIconLabel->setFixedSize(12, 12);
    m_statusIconLabel->setStyleSheet("background-color: #F44336; border-radius: 8px;");
    m_statusTextLabel = new QLabel("未连接", this);
    m_statusTextLabel->setFont(statusFontObj);
    m_statusTextLabel->setStyleSheet("color: #333333;");
    // 分隔符 1
    QLabel* separator1 = new QLabel(" | ", this);
    separator1->setStyleSheet("color: #999; font-weight: bold;");
    // 伺服使能状态
    m_servoIconLabel = new QLabel(this);
    m_servoIconLabel->setFixedSize(12, 12);
    m_servoIconLabel->setStyleSheet("background-color: #9E9E9E; border-radius: 8px;");
    m_servoTextLabel = new QLabel("伺服断开", this);
    m_servoTextLabel->setFont(statusFontObj);
    m_servoTextLabel->setStyleSheet("color: #333333;");
    // 分隔符 2
    QLabel* separator2 = new QLabel(" | ", this);
    separator2->setStyleSheet("color: #999; font-weight: bold;");
    // 自动/手动模式
    m_autoTextLabel = new QLabel("手动模式", this);
    QFont autoFont = statusFontObj;
    autoFont.setBold(true);
    m_autoTextLabel->setFont(autoFont);
    m_autoTextLabel->setStyleSheet("color: #FF9800;");
    // 按顺序添加到布局
    statusLayout->addWidget(m_statusIconLabel);
    statusLayout->addWidget(m_statusTextLabel);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(separator1);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(m_servoIconLabel);
    statusLayout->addWidget(m_servoTextLabel);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(separator2);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(m_autoTextLabel);
    statusBar()->addPermanentWidget(statusContainer);
    statusLayout->setContentsMargins(5, 0, 5, 0);
    statusLayout->setSpacing(5);

    // 7. 初始化坐标管理器
    m_coordManager = new usercoordinatemanager(this);
    m_coordManager->initialize(renderArea, dataTable, weldHoles, mainPlateHole, m_statusLabel);

    // 8. 信号槽连接
    connect(loadAction, &QAction::triggered, this, &MainWindow::importDxf);
    connect(dataTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::handleTableSelectionChanged);
    connect(dataTable, &QTableWidget::itemSelectionChanged,this, &MainWindow::handleTableSelectionChanged);
    connect(rotateAction, &QAction::triggered, this, &MainWindow::applyRotationMatrix);
    connect(m_setupCoordAction, &QAction::triggered, this, &MainWindow::setupCoordinateWizard);
    connect(m_toggleCoordBtn, &QPushButton::clicked, this, [this](bool checked) {
        m_coordManager->toggleCoordinateDisplay(checked);
        m_toggleCoordBtn->setText(checked ? "隐藏用户坐标系" : "显示用户坐标系");
    });

    // 焊接工艺
    connect(m_manageProcessAction, &QAction::triggered, this, &MainWindow::onManageWeldingProcess);

    m_floatingToolWidget = new FloatingToolWidget(renderArea);
    m_floatingToolWidget->hide();

    // 通信
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::onConnectTriggered);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(m_powerBtn, &QPushButton::clicked, this, &MainWindow::toggleRobotPower);
    connect(m_floatingToolWidget->btnRestore, &QPushButton::clicked, this, &MainWindow::restoreDrawing);
    connect(m_floatingToolWidget->btnUndo, &QPushButton::clicked, this, &MainWindow::undo);
    QShortcut *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undo);
    connect(m_floatingToolWidget->btnEraser, &QPushButton::toggled, this, [this](bool checked){
        renderArea->setEraserMode(checked);
    });
    connect(m_floatingToolWidget->btnClose, &QPushButton::clicked, this, [this](){
        m_floatingToolWidget->hide();
        m_floatingToolWidget->btnEraser->setChecked(false);
        m_floatingToolWidget->btnLasso->setChecked(false);
        m_floatingToolWidget->btnMove->setChecked(false);
        m_floatingToolWidget->setSliderVisible(false);
    });
    connect(m_imageProcessAction, &QAction::triggered, this, [this](){
        if (!m_floatingToolWidget->isVisible()) {
            m_floatingToolWidget->move(20, 20);
        }
        m_floatingToolWidget->show();
        m_floatingToolWidget->raise();
    });
    connect(renderArea, &RenderArea::itemDeleted, this, &MainWindow::handleItemDeleted);

    connect(m_floatingToolWidget->sliderEraserSize, &QSlider::valueChanged, this, [this](int value){
        m_floatingToolWidget->lblEraserSize->setText(QString::number(value));
        renderArea->setEraserSize(value);
    });
    connect(m_floatingToolWidget->btnLasso, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnEraser->setChecked(false); m_floatingToolWidget->btnMove->setChecked(false); }
        renderArea->setLassoMode(checked);
    });
    connect(m_floatingToolWidget->btnEraser, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnLasso->setChecked(false); m_floatingToolWidget->btnMove->setChecked(false); }
        m_floatingToolWidget->setSliderVisible(checked);
        renderArea->setEraserMode(checked);
    });
    connect(m_floatingToolWidget->btnMove, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnLasso->setChecked(false); m_floatingToolWidget->btnEraser->setChecked(false); }
        renderArea->setMoveMode(checked);
    });
    // 连接批量删除信号
    connect(renderArea, &RenderArea::bulkPathsDeleted, this, &MainWindow::handleBulkPathsDeleted);
    connect(renderArea, &RenderArea::cancelModesRequested, this, [this](){
        m_floatingToolWidget->btnLasso->setChecked(false);
        m_floatingToolWidget->btnEraser->setChecked(false);
        m_floatingToolWidget->btnMove->setChecked(false);
        m_floatingToolWidget->setSliderVisible(false);
    });
    connect(renderArea, &RenderArea::pathsMoved, this, [this](const QVector<Contour> &updatedPaths){
        saveUndoState();
        this->m_displayPaths = updatedPaths;
    });

    connect(m_positioningAction, &QAction::triggered, this, [this](){
        PositioningDialog dlg(this);
        dlg.setInitialBlocks(renderArea->getPositioningBlocks());
        if(dlg.exec() == QDialog::Accepted) {
            renderArea->setPositioningBlocks(dlg.getBlocks());
        }
    });

    m_floatingToolWidget->installEventFilter(this);
    m_toggleCoordBtn->installEventFilter(this);
    m_startBtn->installEventFilter(this);
    m_pauseBtn->installEventFilter(this);
    m_resetBtn->installEventFilter(this);

    resize(1200, 700);
}

// ----------------------------------------------------
// 功能：DXF导入与 Python脚本调用
// 核心：处理用户选择和 Python 调用的函数
// ----------------------------------------------------
void MainWindow::importDxf()
{
    // 1. 弹出文件对话框，让用户选择 DXF文件
    QString dxfPath = QFileDialog::getOpenFileName(this,"选择DXF文件","","DXF Files (*.dxf)");//提示，能选型的文件类型

    if (dxfPath.isEmpty()) return; // 用户取消选择

    // 2. 准备 Python脚本路径和临时 JSON 输出路径
    QString pythonScript = QCoreApplication::applicationDirPath() + "/import_py.py";

    //这是保存 JSON文件
    QString jsonOutputPath = QFileDialog::getSaveFileName(this, "保存JSON文件", "", "JSON Files (*.json)");
    if (jsonOutputPath.isEmpty()) return;
    // tempFile.close();                                   // 关闭临时文件，让 Python可以写入

    // 3. 配置Python进程参数
    QProcess process;
    QStringList params;
    params << pythonScript;                             // 参数1：脚本路径
    params << dxfPath;                                  // 参数2：用户选择的 DXF 路径 (动态传入！)
    params << jsonOutputPath;                           // 参数3：JSON 输出路径

    // 4. 运行 Python进程 (注意：需要确保系统 PATH 中有 'python' 命令)
    QString pythonExePath = "C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/python.exe";
    process.start(pythonExePath, params);

    // 5. 处理进程执行结果（超时、错误）
    if (!process.waitForFinished(10000)) { // 等待 10 秒
        QMessageBox::warning(this, "Error", "Python script timeout or failed to start.");
        qDebug() << "Error:" << process.errorString();
        return;
    }
    if (process.exitCode() != 0) {
        // 读取 Python 脚本的标准错误输出 (stderr)
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        QMessageBox::critical(this, "Script Error", "Python script failed:\n" + errorOutput);
        return;
    }

    // 6. 检查 Python 脚本是否运行成功
    if (process.exitCode() != 0) {
        QString error = process.readAllStandardError();
        QMessageBox::critical(this, "Script Error", "Python script failed:\n" + error);
        return;
    }

    // 7. 加载解析后的 JSON数据
    loadDrawingData(jsonOutputPath);
}

// ----------------------------------------------------
// 功能：JSON解析与 UI更新
// 核心：解析 Python成的 JSON数据，将数据存入结构体，并更新表格和绘图区
// ----------------------------------------------------
void MainWindow::loadDrawingData(const QString &filePath)
{
    // 1. 打开 JSON文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open data file.");
        return;
    }

    // 2. 解析JSON根对象
    QByteArray data = file.readAll();                               // 读取文件
    QJsonDocument doc = QJsonDocument::fromJson(data);              // 相当于把 JSON字符串转换成 Qt能理解的 JSON文档结构
    QJsonObject rootObj = doc.object();                             // 从解析好的 QJsonDocument中，提取出根层级的 JSON对象，存到 QJsonObject

    // 3. 解析圆数据
    allHoles.clear();
    QJsonArray holeArray = rootObj["holes"].toArray();              // 通过键名 holes从根对象中获取对应的值，将获取到的值强制转换为 QJsonArray（Qt的 JSON数组类型）
    for (const auto& holeObj : std::as_const(holeArray)) {          // 从所有管孔遍历 holeObj
        QJsonObject hole = holeObj.toObject();                      // 将当前遍历到的 JSON值（holeObj）转换为 QJsonObject（Qt的 JSON对象类型）
        Hole h;                                                     // 存储当前解析出的单个管孔数据
        QJsonArray center = hole["center"].toArray();               // 圆心（数组）
        h.center = QPointF(center[0].toDouble(), center[1].toDouble());
        h.radius = hole["radius"].toDouble();                       // 半径
        allHoles.append(h);                                         // 将解析完成的 Hole对象 h添加到 allHoles容器的末尾，完成单个管孔的解析和存储
    }

    // 4. 解析轮廓
    contours.clear();
    QJsonArray contourArray = rootObj["contours"].toArray();
    for (const auto& cObj : std::as_const(contourArray)) {
        Contour c;
        QJsonArray pts = cObj.toArray();
        for (const auto& pVal : std::as_const(pts)) {
            QJsonArray xy = pVal.toArray();
            c.points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
        }
        contours.append(c);
    }
    weldHoles.clear();
    mainPlateContour.clear();
    mainPlateHole = Hole();                                         // 初始化主体圆
    if (!contours.isEmpty()) {
        isRectangularPlate = true;
        // 假设第一个轮廓就是外边框 (通常 DXF 里最大的框是外边框)
        const auto& points = contours[0].points;
        for(const QPointF& p : points) {
            mainPlateContour << p;
        }
        weldHoles = allHoles;                                       // 这种模式下，所有的圆都是焊接孔
        // 需要计算一个虚拟的主板圆心和半径给后续的坐标系功能使用
        QRectF rect = mainPlateContour.boundingRect();
        mainPlateHole.center = rect.center();
        // 近似半径：取宽高中较小的一半，或者对角线的一半，用于坐标系向导的参考距离
        mainPlateHole.radius = qMin(rect.width(), rect.height()) / 2.0;

    } else {
        // 没有轮廓，找最大圆
        isRectangularPlate = false;
        if (!allHoles.isEmpty()) {
            mainPlateHole = allHoles[0];
            // 找最大半径
            for (const auto& hole : std::as_const(allHoles)) {
                if (hole.radius > mainPlateHole.radius + 1e-3)
                    mainPlateHole = hole;
            }
            // 分离管孔
            for (const auto& hole : std::as_const(allHoles)) {
                if (hole.radius < mainPlateHole.radius - 1e-3) {
                    Hole newHole = hole;
                    weldHoles.append(newHole);
                }
            }
        }
    }

    // 5. 解析所有的背景线条（包含类型）
    m_displayPaths.clear();
    if (rootObj.contains("display_paths")) {
        QJsonArray pathArray = rootObj["display_paths"].toArray();
        for (const auto& pRef : std::as_const(pathArray)) {
            QJsonObject pathObj = pRef.toObject();
            Contour contour;
            contour.type = pathObj["type"].toString(); // 取出 python新加的 type
            QJsonArray ptsArray = pathObj["points"].toArray();
            for (const auto& ptRef : std::as_const(ptsArray)) {
                QJsonArray xy = ptRef.toArray();
                if (xy.size() >= 2) {
                    contour.points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
                }
            }
            m_displayPaths.append(contour);
        }
    }

    // 6. 更新左侧绘图区
    disconnect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);
    dataTable->setRowCount(0);
    for (int i = 0; i < m_displayPaths.size(); ++i) {
        dataTable->insertRow(i);
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 0, indexItem);
        QTableWidgetItem* item = new QTableWidgetItem(m_displayPaths[i].type);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 1, item);
    }
    connect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);

    detailWidget->hide();
    renderArea->setHighlightedPathIndices(QList<int>());

    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, true);

    m_originalDisplayPaths = m_displayPaths;
    m_originalWeldHoles = weldHoles;
    m_originalMainPlateHole = mainPlateHole;
    m_originalMainPlateContour = mainPlateContour;
    m_undoStack.clear();
    m_floatingToolWidget->btnUndo->setEnabled(false);
}

// ----------------------------------------------------
// 功能：表格与绘图区的联动（交互逻辑）
// 核心：表格选中某行 → 绘图区高亮对应圆
// ----------------------------------------------------
void MainWindow::handleTableSelectionChanged()
{
    // 1. 获取当前表格中所有被选中的行号
    QList<int> selectedRows;
    QList<QTableWidgetItem*> selectedItems = dataTable->selectedItems();
    for (auto item : selectedItems) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    // 2. 如果有选中行，处理右下角的详细信息面板
    if (!selectedRows.isEmpty()) {
        int firstRow = selectedRows.first(); // 取第一条线来计算参数
        if (firstRow < 0 || firstRow >= m_displayPaths.size()) {
            detailWidget->hide();
            return;
        }
        detailWidget->show();

        const Contour& c = m_displayPaths[firstRow];
        m_bevelAngleSpin->blockSignals(true);
        m_rootFaceSpin->blockSignals(true);
        m_bevelAngleSpin->setValue(c.bevelAngle);
        m_rootFaceSpin->setValue(c.rootFace);
        m_bevelAngleSpin->blockSignals(false);
        m_rootFaceSpin->blockSignals(false);
        QString infoText;

        // --- 几何参数计算逻辑（保留原样） ---
        if (c.type == "直线" && c.points.size() >= 2) {
            QPointF p1 = c.points.first(), p2 = c.points.last();
            double length = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 直线参数 】</span><br>"
                               "起点:(%1, %2) &nbsp;&nbsp;&nbsp; 终点:(%3, %4) &nbsp;&nbsp;&nbsp; 长度:%5")
                           .arg(p1.x(), 0, 'f', 2).arg(p1.y(), 0, 'f', 2)
                           .arg(p2.x(), 0, 'f', 2).arg(p2.y(), 0, 'f', 2).arg(length, 0, 'f', 2);
        } else if (c.type == "圆" && c.points.size() >= 3) {
            double minX = c.points[0].x(), maxX = minX, minY = c.points[0].y(), maxY = minY;
            for (const QPointF& p : std::as_const(c.points)) {
                if (p.x() < minX) minX = p.x(); if (p.x() > maxX) maxX = p.x();
                if (p.y() < minY) minY = p.y(); if (p.y() > maxY) maxY = p.y();
            }
            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 圆 参数 】</span><br>"
                               "圆心:(%1, %2) &nbsp;&nbsp;&nbsp; 半径:%3")
                           .arg((minX+maxX)/2.0, 0, 'f', 2).arg((minY+maxY)/2.0, 0, 'f', 2).arg((maxX-minX)/2.0, 0, 'f', 2);
        } else if (c.type == "圆弧" && c.points.size() >= 3) {
            double arcLength = 0.0;
            for (int i = 0; i < c.points.size() - 1; ++i) {
                arcLength += std::hypot(c.points[i+1].x() - c.points[i].x(), c.points[i+1].y() - c.points[i].y());
            }
            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 圆弧参数 】</span><br>"
                               "起点:(%1, %2) &nbsp;&nbsp;&nbsp; 终点:(%3, %4) &nbsp;&nbsp;&nbsp; 近似弧长:%5")
                           .arg(c.points.first().x(), 0, 'f', 2).arg(c.points.first().y(), 0, 'f', 2)
                           .arg(c.points.last().x(), 0, 'f', 2).arg(c.points.last().y(), 0, 'f', 2)
                           .arg(arcLength, 0, 'f', 2);
        } else {
            struct ArcSegment { QPointF start, end, center; double radius; bool isLine; };
            QList<ArcSegment> segments;

            // 拟合误差阈值。数值越大，拟合的段数越少（圆弧越少），但会轻微失真。
            // 建议：如果是毫米为单位，0.5是个不错的选择。
            const double TOLERANCE = 0.5;

            auto distToLine = [](const QPointF& p, const QPointF& a, const QPointF& b) {
                double l2 = std::pow(a.x() - b.x(), 2) + std::pow(a.y() - b.y(), 2);
                if (l2 == 0) return std::hypot(p.x() - a.x(), p.y() - a.y());
                double t = ((p.x() - a.x()) * (b.x() - a.x()) + (p.y() - a.y()) * (b.y() - a.y())) / l2;
                t = std::max(0.0, std::min(1.0, t));
                return std::hypot(p.x() - (a.x() + t * (b.x() - a.x())), p.y() - (a.y() + t * (b.y() - a.y())));
            };

            int n = c.points.size();
            int i = 0;

            // 贪心搜索循环
            while (i < n - 1) {
                int best_j = i + 1;
                ArcSegment best_arc = {c.points[i], c.points[i+1], QPointF(), 0, true};

                for (int j = n - 1; j >= i + 2; --j) {
                    int mid = i + (j - i) / 2;
                    QPointF p1 = c.points[i], p2 = c.points[mid], p3 = c.points[j];

                    double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));

                    if (std::abs(D) < 1e-6) {
                        bool goodLine = true;
                        for(int k = i + 1; k < j; ++k) {
                            if (distToLine(c.points[k], p1, p3) > TOLERANCE) { goodLine = false; break; }
                        }
                        if (goodLine) {
                            best_j = j;
                            best_arc = {p1, p3, QPointF(), 0, true};
                            break;
                        }
                    } else {
                        double xc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p2.y() - p3.y()) +
                                     (p2.x()*p2.x() + p2.y()*p2.y())*(p3.y() - p1.y()) +
                                     (p3.x()*p3.x() + p3.y()*p3.y())*(p1.y() - p2.y())) / D;
                        double yc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p3.x() - p2.x()) +
                                     (p2.x()*p2.x() + p2.y()*p2.y())*(p1.x() - p3.x()) +
                                     (p3.x()*p3.x() + p3.y()*p3.y())*(p2.x() - p1.x())) / D;
                        double R = std::hypot(p1.x() - xc, p1.y() - yc);

                        bool goodArc = true;
                        for(int k = i + 1; k < j; ++k) {
                            double dist = std::abs(std::hypot(c.points[k].x() - xc, c.points[k].y() - yc) - R);
                            if (dist > TOLERANCE) { goodArc = false; break; }
                        }
                        if (goodArc) {
                            best_j = j;
                            best_arc = {p1, p3, QPointF(xc, yc), R, false};
                            break;
                        }
                    }
                }

                segments.append(best_arc);
                i = best_j;
            }

            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 %1 参数 (拟合) 】</span><br>"
                               "原始点数:%2 &nbsp;&nbsp;&nbsp; 拟合段数:%3<br>")
                           .arg(c.type).arg(n).arg(segments.size());

            for (int s = 0; s < segments.size(); ++s) {
                const auto& seg = segments[s];
                if (seg.isLine) {
                    infoText += QString("<b>段%1 [直线]</b> 起点:(%2,%3) &nbsp;终点:(%4,%5) &nbsp;长度:%6<br>")
                                    .arg(s+1).arg(seg.start.x(), 0, 'f', 1).arg(seg.start.y(), 0, 'f', 1)
                                    .arg(seg.end.x(), 0, 'f', 1).arg(seg.end.y(), 0, 'f', 1)
                                    .arg(std::hypot(seg.end.x()-seg.start.x(), seg.end.y()-seg.start.y()), 0, 'f', 1);
                } else {
                    infoText += QString("<b>段%1 [圆弧]</b> 起点:(%2,%3) &nbsp;终点:(%4,%5) &nbsp;半径:%6<br>")
                                    .arg(s+1).arg(seg.start.x(), 0, 'f', 1).arg(seg.start.y(), 0, 'f', 1)
                                    .arg(seg.end.x(), 0, 'f', 1).arg(seg.end.y(), 0, 'f', 1).arg(seg.radius, 0, 'f', 1);
                }
            }
            infoText = infoText.trimmed();
        }
        if (selectedRows.size() > 1) {
            infoText = QString("<span style='color:#999; font-size:11px;'>（多选 %1 条，仅显首条）</span><br>").arg(selectedRows.size()) + infoText;
        }

        m_detailContentText->setHtml(infoText);
    } else {
        detailWidget->hide(); // 没有选中时隐藏面板
    }

    renderArea->setHighlightedPathIndices(selectedRows);
}

// ----------------------------------------------------
// 功能：响应单元格内容的修改，验证输入的有效性，更新底层数据模型，并同步刷新绘图区
// ----------------------------------------------------
void MainWindow::handleTableCellChanged(int row, int column)
{
    // if (row < 0 || row >= weldHoles.size())                             // 索引有效性检查：防止行索引超出weldHoles的范围，避免数组越界
    //     return;

    // // 获取当前单元格数据
    // QTableWidgetItem *item = dataTable->item(row, column);              // 获取当前单元格的 item对象，判空防止空指针访问（一个单元格对象）
    // if (!item) return;

    // // 校验输入的数据有效性
    // QString value = item->text();                                       // 获取 item对象的内容
    // bool ok;
    // double num =0;
    // if (column == 0) {                                                  // 第一列半径
    //     // 半径列：用原有的toDouble验证
    //     // bool ok;
    //     num=value.toDouble(&ok);                                        // 转换成功，ok=true
    //     if (!ok) {                                                      // 转换失败
    //         QMessageBox::warning(this, "输入错误", "请输入有效的数字");
    //         updateTableItem(row, column);                               // 将输入错误的数据还原为修改前的数据
    //         return;
    //     }
    // } else if (column == 1) {                                           // 第二列二维圆心坐标
    //     QStringList parts = value.split(',');                           // 按逗号，把字符串拆分成字符串列表
    //     parts[0].replace('(', ' ').trimmed().toDouble(&ok);             // 先将(替换为空格，再去掉首尾的所有空格，最后转换为浮点数
    //     if (!ok || parts.size() != 2) {
    //         QMessageBox::warning(this, "输入错误", "请输入有效的数字");
    //         updateTableItem(row, column);
    //         return;
    //     }
    //     parts[1].replace(')', ' ').trimmed().toDouble(&ok);
    //     if (!ok) {
    //         QMessageBox::warning(this, "输入错误", "请输入有效的数字");
    //         updateTableItem(row, column);
    //         return;
    //     }
    // }

    // // 修改对应单元格数据
    // Hole &hole = weldHoles[row];                                        // 根据列索引更新 weldHoles中对应的数据
    // switch (column) {
    // case 0:{                                                            // 半径
    //     hole.radius = num;
    //     break;}
    // case 1:{                                                            // 圆心坐标，需要特殊处理
    //     QStringList parts = value.split(',');                           // 解析 "X, Y"格式
    //     if (parts.size() == 2) {
    //         double x = parts[0].replace('(', ' ').trimmed().toDouble(&ok);          // 去除括号和空格，转换为 X坐标
    //         if (ok) {
    //             double y = parts[1].replace(')', ' ').trimmed().toDouble(&ok);      // 去除括号和空格，转换为 Y坐标
    //             if (ok) {
    //                 hole.center.setX(x);
    //                 hole.center.setY(y);
    //             }
    //         }
    //     }
    //     item->setText(QString("(%1, %2)")
    //                       .arg(hole.center.x(), 0, 'f', 2)
    //                       .arg(hole.center.y(), 0, 'f', 2));         // 重新格式化显示：保证坐标始终是"(X, Y)"且保留 2位小数，统一格式
    //     break;
    // }
    // case 2:{
    //     QString str = value;
    //     str.remove('(').remove(')');
    //     QStringList parts = str.split(',');
    //     if (parts.size() == 3) {
    //         double x = parts[0].trimmed().toDouble(&ok);
    //         if (ok) {
    //             double y = parts[1].trimmed().toDouble(&ok);
    //             if (ok) {
    //                 double z = parts[2].trimmed().toDouble(&ok);
    //                 if (ok) {
    //                     hole.center3D = QVector3D(x, y, z);
    //                     // 如果修改了三维坐标，同步更新二维坐标（投影）
    //                     hole.center = QPointF(x, y);
    //                     // 更新二维坐标单元格
    //                     dataTable->item(row, 2)->setText(QString("(%1, %2)")
    //                                                          .arg(x, 0, 'f', 2)
    //                                                          .arg(y, 0, 'f', 2));
    //                 }
    //             }
    //         }
    //     }
    //     item->setText(QString("(%1, %2, %3)")
    //                       .arg(hole.center3D.x(), 0, 'f', 2)
    //                       .arg(hole.center3D.y(), 0, 'f', 2)
    //                       .arg(hole.center3D.z(), 0, 'f', 2));
    //     break;}
    // }

    // // 更新绘图区
    // renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, false);
}

// ----------------------------------------------------
// 功能：旋转矩阵
// ----------------------------------------------------
void MainWindow::applyRotationMatrix()
{
    RotationMatrixDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        rotationMatrix = dialog.getRotationMatrix();

        // 应用旋转矩阵到所有孔洞
        for (auto& hole : weldHoles) {
            // 初始三维坐标 (x, y, 0)
            QVector3D original(hole.center.x(), hole.center.y(), 0.0);

            // 应用旋转
            QVector3D transformed = rotationMatrix.map(original);

            // 更新三维坐标
            hole.center3D = transformed;
        }

        disconnect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);
        for (int i = 0; i < weldHoles.size(); ++i) {
            const Hole& hole = weldHoles[i];
            QString center3DStr = QString("(%1, %2, %3)")
                                      .arg(hole.center3D.x(), 0, 'f', 2)
                                      .arg(hole.center3D.y(), 0, 'f', 2)
                                      .arg(hole.center3D.z(), 0, 'f', 2);
            dataTable->setItem(i, 3, new QTableWidgetItem(center3DStr));
        }
        connect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);

        QMessageBox::information(this, "成功", "旋转矩阵已应用");
    }
}

// ----------------------------------------------------
// 功能：建立用户坐标系向导
// ----------------------------------------------------
void MainWindow::setupCoordinateWizard()
{
    // 如果没有管孔数据，弹窗提示并返回
    if (weldHoles.isEmpty()) {
        QMessageBox::warning(this, "操作提示", "当前没有管孔数据，无法建立坐标系。\n请先点击“文件 -> 导入DXF”加载图纸。");
        return;
    }

    if (m_coordManager && m_coordManager->isSetupComplete()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this,
                                      "重新建立",
                                      "检测到当前已建立用户坐标系，是否重新建立？\n（重新建立将覆盖原有坐标系数据）",
                                      QMessageBox::Yes | QMessageBox::Cancel,
                                      QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QDialog* dialog = new QDialog(this);
    // 添加自动销毁属性，避免内存泄漏
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("用户坐标系建立向导");
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);

    // 标题标签
    QLabel* titleLabel = new QLabel("请按照提示操作机器人", dialog);
    QFont font = titleLabel->font();
    font.setBold(true);
    font.setPointSize(14);
    titleLabel->setFont(font);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel* infoLabel = new QLabel("初始化中...", dialog);
    infoLabel->setWordWrap(true);
    infoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLabel->setStyleSheet(R"(
        QLabel {
            color: blue;
            font-size: 14px;
            padding: 10px; /* 标签内部上下左右内边距，避免文本贴边 */
            line-height: 1.5; /* 行间距1.5倍，文本不拥挤 */
        }
    )");
    infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    infoLabel->setMinimumHeight(60);
    layout->addWidget(infoLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* confirmBtn = new QPushButton("确定 (当前步骤完成)", dialog);
    QPushButton* cancelBtn = new QPushButton("取消", dialog);
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // 使用定时器同步 Manager 的状态文字到对话框
    QTimer* syncTimer = new QTimer(dialog);
    connect(syncTimer, &QTimer::timeout, this, [this, infoLabel]() {
        if (m_statusLabel) infoLabel->setText(m_statusLabel->text());
    });
    syncTimer->start(200);

    // 确认按钮逻辑
    connect(confirmBtn, &QPushButton::clicked, this, [this, dialog]() {
        m_coordManager->confirmCurrentStep();
        if (m_coordManager->isSetupComplete()) {
            dialog->accept();
            QMessageBox::information(this, "成功", "用户坐标系已建立！\n三维坐标已更新。");
            m_toggleCoordBtn->setVisible(true); // 按照要求，完成后显示控制按钮
            m_toggleCoordBtn->setChecked(true);
        }
    });

    // 取消按钮逻辑
    connect(cancelBtn, &QPushButton::clicked, this, [this, dialog]() {
        m_coordManager->cancelCurrentStep();
        dialog->reject();
    });

    // 右上角叉号，执行取消逻辑
    connect(dialog, &QDialog::finished, this, [this](int result) {

        if (result == QDialog::Rejected) {
            m_coordManager->cancelCurrentStep();
        }
    });

    // 启动向导
    m_coordManager->startSetup();

    dialog->adjustSize();
    dialog->move(this->geometry().center() - dialog->rect().center());

    dialog->exec();
}

// ----------------------------------------------------
// 功能：打开焊接工艺管理对话框
// ----------------------------------------------------
void MainWindow::onManageWeldingProcess()
{
    // 创建对话框，并传入当前工艺列表的指针
    WeldingProcessDialog dialog(&m_weldingProcesses, this);
    dialog.exec(); // 模态显示
}

// ----------------------------------------------------
// 功能：启动自动加载数据
// ----------------------------------------------------
void MainWindow::loadWeldingProcesses()
{
    // 路径：exe 同级目录下的 welding_processes.json
    QString path = QCoreApplication::applicationDirPath() + "/welding_processes.json";
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        return;                                             // 文件不存在（第一次运行），直接返回，不做任何事
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    m_weldingProcesses.clear(); // 清空默认值
    const QJsonArray arr = doc.array();

    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        WeldingProcess p;
        p.id = obj["id"].toString();
        p.strategy = obj["strategy"].toString();
        p.remark = obj["remark"].toString();
        m_weldingProcesses.append(p);
    }
}

// =============================================================================
// 连接设置与逻辑
// =============================================================================
void MainWindow::onConnectTriggered()
{
    ConnectionDialog dlg(this);
    dlg.setIp(m_lastIp);
    dlg.setPort(m_lastPort);

    if (dlg.exec() == QDialog::Accepted) {
        int action = dlg.getAction();

        if (action == 1) {
            m_lastIp = dlg.getIp();
            m_lastPort = dlg.getPort();

            QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
            settings.setValue("RobotIP", m_lastIp);
            settings.setValue("RobotPort", m_lastPort);

            m_modbusManager->connectToRobot(m_lastIp, m_lastPort);

            // 👇【完美照抄官方 Demo，删掉所有多余的路径设置】
            int ret = RobotAPI::ConnectRobot(m_lastIp.toStdString().c_str(), m_currentDevId, true);

            if (ret == 0 || ret == 10021) {
                RobotAPI::SelectRobot(m_currentDevId);
                showAndSaveLog(QString("SDK连接就绪！当前操作设备ID为: %1").arg(m_currentDevId));
            } else {
                showAndSaveLog(QString("SDK连接失败！错误码: %1").arg(ret));
                QMessageBox::critical(this, "SDK 错误", QString("无法建立 SDK 核心连接，错误码: %1").arg(ret));
                m_currentDevId = 0;
            }

        } else if (action == 2) {
            m_modbusManager->disconnectFromRobot();

            if (m_currentDevId != 0) {
                RobotAPI::DisconnectRobot(m_currentDevId);
                showAndSaveLog(QString("SDK连接已断开 (Id: %1)").arg(m_currentDevId));
                m_currentDevId = 0;
            }
        }
    }
}

void MainWindow::onModbusStateChanged(int state)
{
    if (state == 2) { // 已连接 (绿色)
        m_statusIconLabel->setStyleSheet("background-color: #4CAF50; border-radius: 8px;");
        m_statusTextLabel->setText("已连接");
        m_statusLabel->setText(QString("已连接到机器人: %1:%2").arg(m_lastIp).arg(m_lastPort));

        m_startBtn->setVisible(true);
        m_pauseBtn->setVisible(false);
        m_resetBtn->setVisible(true);
    }
    else if (state == 1) { // 正在连接 (黄色)
        m_statusIconLabel->setStyleSheet("background-color: #FFC107; border-radius: 8px;");
        m_statusTextLabel->setText("正在连接...");
        m_statusLabel->setText("尝试连接中...");

        m_startBtn->setVisible(false);
        m_pauseBtn->setVisible(false);
        m_resetBtn->setVisible(false);
    }
    else { // 未连接 / 断开 (红色)
        m_statusIconLabel->setStyleSheet("background-color: #F44336; border-radius: 8px;");
        m_statusTextLabel->setText("未连接");
        m_statusLabel->setText("未连接 / 连接断开");

        m_startBtn->setVisible(false);
        m_pauseBtn->setVisible(false);
        m_resetBtn->setVisible(false);
    }
}

// =============================================================================
// 机器人控制逻辑
// =============================================================================
void MainWindow::onStartClicked()
{
    if (m_startBtn->text() == "预约") {
        // 触发启动信号 (40129.9) - 跑步骤1和2
        m_modbusManager->prepareAndStart();
    }
    else if (m_startBtn->text() == "启动") {
        // 到达启动之后，不再跑步骤2，而是持续跑步骤3！
        if (weldHoles.isEmpty()) {
            QMessageBox::warning(this, "警告", "没有管孔数据可供焊接！");
            return;
        }

        m_currentWeldIndex = 0;
        m_isWeldingProcessRunning = true;
        m_startBtn->setText("连续焊接中...");
        m_startBtn->setEnabled(false); // 防止中途乱点

        m_pauseBtn->setVisible(true);
        m_pauseBtn->setChecked(false); // 确保按钮处于没被按下的状态
        m_pauseBtn->setText("暂停");

        m_modbusManager->startWeldingProcess();
    }
}

// ----------------------------------------------------
// 暂停按钮：处理暂停和恢复
// ----------------------------------------------------
void MainWindow::onPauseClicked()
{
    bool isPaused = m_pauseBtn->isChecked(); // 按钮按下状态为 true (暂停中)

    if (isPaused) {
        m_pauseBtn->setText("继续");
        m_modbusManager->setPause(true); // 发送暂停信号
    } else {
        m_pauseBtn->setText("暂停");
        m_modbusManager->setPause(false); // 清除暂停并触发启动信号
    }
}

// ----------------------------------------------------
// 复位按钮
// ----------------------------------------------------
void MainWindow::onResetClicked()
{
    m_modbusManager->resetAlarm();
}

// ----------------------------------------------------
// 窗口关闭事件，保证安全退出下发指令并断开 TCP 连接
// ----------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isShuttingDown || !m_modbusManager || !m_modbusManager->isConnected()) {
        if (m_modbusManager) {
            m_modbusManager->disconnectFromRobot();
        }
        event->accept();
        return;
    }

    event->ignore();
    m_isShuttingDown = true;
    m_statusLabel->setText("正在安全关闭机器人并清理状态...");

    // 监听底层的“关机完成”信号。收到信号后，立刻重新触发关闭
    connect(m_modbusManager, &ModbusManager::shutdownFinished, this, [this]() {
        this->close();
    });

    // 调用底层的安全关闭逻辑
    m_modbusManager->safeShutdown();
}

// =============================================================================
// 更新伺服使能 UI 状态
// =============================================================================
void MainWindow::onServoStateChanged(bool enabled)
{
    if (enabled) {
        // 绿色：伺服已上电
        m_servoIconLabel->setStyleSheet("background-color: #4CAF50; border-radius: 8px;");
        m_servoTextLabel->setText("伺服使能");
    } else {
        // 灰色：伺服未上电/断开
        m_servoIconLabel->setStyleSheet("background-color: #9E9E9E; border-radius: 8px;");
        m_servoTextLabel->setText("伺服断开");
    }
}

// =============================================================================
// 更新自动/手动 UI 状态
// =============================================================================
void MainWindow::onAutoStateChanged(bool isAuto)
{
    if (isAuto) {
        m_autoTextLabel->setStyleSheet("color: #4CAF50;"); // 绿色文字
        m_autoTextLabel->setText("自动模式");
    } else {
        m_autoTextLabel->setStyleSheet("color: #FF9800;"); // 橙色文字
        m_autoTextLabel->setText("手动模式");
    }
}

// ----------------------------------------------------
// 功能：持续下发步骤三的数据 (从表格里依次取点)
// ----------------------------------------------------
void MainWindow::sendNextWeldHole()
{
    if (!m_isWeldingProcessRunning) return;

    if (m_currentWeldIndex >= weldHoles.size()) {

        m_statusLabel->setText("所有管孔焊接完成！下发全 0 数据...");
        m_modbusManager->sendWeldingFinished();

        m_isWeldingProcessRunning = false;
        m_startBtn->setText("预约");
        m_startBtn->setEnabled(true);
        m_pauseBtn->setVisible(false);

        QTimer::singleShot(500, this, [this](){
            QMessageBox::information(this, "完成", "恭喜，所有管孔已连续焊接完毕！");
        });
        return;
    }

    const Hole& hole = weldHoles[m_currentWeldIndex];

    WeldingData data;
    data.x = hole.center3D.x();
    data.y = hole.center3D.y();
    data.z = hole.center3D.z();
    data.r = hole.radius;
    data.processId = 1;

    dataTable->selectRow(m_currentWeldIndex);
    m_statusLabel->setText(QString("正在下发并焊接第 %1 / %2 个管孔...")
                               .arg(m_currentWeldIndex + 1)
                               .arg(weldHoles.size()));

    // 调用专属的管孔数据下发函数
    m_modbusManager->sendWeldHoleData(data);
}

void MainWindow::restoreDrawing()
{
    m_floatingToolWidget->btnRestore->clearFocus();
    m_floatingToolWidget->btnRestore->setDown(false);

    m_floatingToolWidget->btnLasso->setChecked(false);
    m_floatingToolWidget->btnEraser->setChecked(false);

    if (m_originalDisplayPaths.isEmpty() && m_originalWeldHoles.isEmpty()) return;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "还原图纸", "确定要还原图纸到初始状态吗？\n所有被橡皮擦除的线条都将恢复。",
                                  QMessageBox::Yes | QMessageBox::No);

    m_floatingToolWidget->btnRestore->update();

    if (reply == QMessageBox::Yes) {
        saveUndoState();
        m_displayPaths = m_originalDisplayPaths;
        weldHoles = m_originalWeldHoles;
        mainPlateHole = m_originalMainPlateHole;
        mainPlateContour = m_originalMainPlateContour;

        disconnect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);
        dataTable->setRowCount(0);
        for (int i = 0; i < m_displayPaths.size(); ++i) {
            dataTable->insertRow(i);
            QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
            indexItem->setTextAlignment(Qt::AlignCenter);
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(i, 0, indexItem);
            QTableWidgetItem* item = new QTableWidgetItem(m_displayPaths[i].type);
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(i, 1, item);
        }
        connect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);

        detailWidget->hide();
        renderArea->setHighlightedPathIndices(QList<int>());
        renderArea->setDisplayPaths(m_displayPaths);
        renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, false);
    }
}

void MainWindow::handleItemDeleted(const QPointF &pos)
{
    saveUndoState();
    double tolerance = 15.0 / renderArea->scaleFactor(); // 碰撞容差
    dataTable->clearSelection();
    // --- 1. 删除基础线条 (m_displayPaths) ---
    for (int i = m_displayPaths.size() - 1; i >= 0; --i) {
        bool hit = false;
        for (int j = 0; j < m_displayPaths[i].points.size() - 1; ++j) {
            // 这里调用你 renderarea 里的那个距离计算函数
            if (renderArea->distancePointToSegment(pos, m_displayPaths[i].points[j], m_displayPaths[i].points[j+1]) < tolerance) {
                hit = true; break;
            }
        }
        if (hit) {
            m_displayPaths.removeAt(i);
            dataTable->removeRow(i); // 同步删除表格
        }
    }
    updateTableIndices();
    // --- 2. 删除管孔 (weldHoles) ---
    for (int i = weldHoles.size() - 1; i >= 0; --i) {
        double d = std::abs(std::hypot(pos.x() - weldHoles[i].center.x(), pos.y() - weldHoles[i].center.y()) - weldHoles[i].radius);
        if (d < tolerance) {
            weldHoles.removeAt(i);
        }
    }

    // --- 3. 删除外框 (mainPlateContour / Hole) ---
    // 检查方形边框
    bool hitPlate = false;
    for (int j = 0; j < mainPlateContour.size() - 1; ++j) {
        if (renderArea->distancePointToSegment(pos, mainPlateContour[j], mainPlateContour[j+1]) < tolerance) {
            hitPlate = true; break;
        }
    }
    // 检查圆形边框
    double dPlateCircle = std::abs(std::hypot(pos.x() - mainPlateHole.center.x(), pos.y() - mainPlateHole.center.y()) - mainPlateHole.radius);
    if (hitPlate || dPlateCircle < tolerance) {
        mainPlateContour.clear();
        mainPlateHole.radius = 0;
    }

    // --- 4. 统一刷新界面 ---
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, false);
}

void MainWindow::handleBulkPathsDeleted(QList<int> indices)
{
    // 为了防止数组越界，必须将索引从大到小排序，从尾部开始删
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    saveUndoState();
    dataTable->clearSelection();
    disconnect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);

    for (int idx : indices) {
        if (idx >= 0 && idx < m_displayPaths.size()) {
            m_displayPaths.removeAt(idx);
            dataTable->removeRow(idx);
        }
    }
    connect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);
    updateTableIndices();
    dataTable->clearSelection();
    detailWidget->hide();
    renderArea->setHighlightedPathIndices(QList<int>());

    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, false);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_statusLabel && event->type() == QEvent::MouseButtonPress) {
        showLogViewer();
        return true;
    }
    if (watched == m_floatingToolWidget ||
        watched == m_toggleCoordBtn ||
        watched == m_startBtn ||
        watched == m_pauseBtn ||
        watched == m_resetBtn ||
        watched == m_powerBtn)
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            if (renderArea) {
                renderArea->update();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::saveUndoState() {
    DrawingState state;
    state.displayPaths = m_displayPaths;
    state.weldHoles = weldHoles;
    state.mainPlateHole = mainPlateHole;
    state.mainPlateContour = mainPlateContour;

    m_undoStack.append(state);

    if (m_undoStack.size() > 10) {
        m_undoStack.removeFirst();
    }
    m_floatingToolWidget->btnUndo->setEnabled(true);
}

void MainWindow::undo() {
    if (m_undoStack.isEmpty()) return;

    DrawingState state = m_undoStack.takeLast();

    for (int i = 0; i < state.displayPaths.size(); ++i) {
        if (state.displayPaths.size() == m_displayPaths.size()) {
            state.displayPaths[i].bevelAngle = m_displayPaths[i].bevelAngle;
            state.displayPaths[i].rootFace = m_displayPaths[i].rootFace;
        } else {
            for (int j = 0; j < m_displayPaths.size(); ++j) {
                if (state.displayPaths[i].type == m_displayPaths[j].type &&
                    state.displayPaths[i].points.first() == m_displayPaths[j].points.first() &&
                    state.displayPaths[i].points.last() == m_displayPaths[j].points.last())
                {
                    state.displayPaths[i].bevelAngle = m_displayPaths[j].bevelAngle;
                    state.displayPaths[i].rootFace = m_displayPaths[j].rootFace;
                    break;
                }
            }
        }
    }

    m_displayPaths = state.displayPaths;
    weldHoles = state.weldHoles;
    mainPlateHole = state.mainPlateHole;
    mainPlateContour = state.mainPlateContour;

    disconnect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);
    dataTable->setRowCount(0);
    for (int i = 0; i < m_displayPaths.size(); ++i) {
        dataTable->insertRow(i);
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 0, indexItem);
        QTableWidgetItem* item = new QTableWidgetItem(m_displayPaths[i].type);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 1, item);
    }
    connect(dataTable, &QTableWidget::cellChanged, this, &MainWindow::handleTableCellChanged);

    detailWidget->hide();
    renderArea->setHighlightedPathIndices(QList<int>());
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->setData(weldHoles, mainPlateHole, mainPlateContour, isRectangularPlate, false);

    if (m_undoStack.isEmpty()) {
        m_floatingToolWidget->btnUndo->setEnabled(false);
    }
}

// ========================================================
// 更新选中线条的坡口与顿边参数
// ========================================================
void MainWindow::onBevelParametersChanged()
{
    // 获取当前表格中所有被选中的行号 (支持按住 Ctrl 多选批量修改)
    QList<int> selectedRows;
    QList<QTableWidgetItem*> selectedItems = dataTable->selectedItems();
    for (auto item : std::as_const(selectedItems)) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    if (selectedRows.isEmpty()) return;

    double angle = m_bevelAngleSpin->value();
    double face = m_rootFaceSpin->value();

    for (int row : selectedRows) {
        if (row >= 0 && row < m_displayPaths.size()) {
            m_displayPaths[row].bevelAngle = angle;
            m_displayPaths[row].rootFace = face;
        }
    }
}

// ========================================================
// 日志查看器对话框实现
// ========================================================
LogViewerDialog::LogViewerDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("系统运行日志查看器");
    setMinimumSize(700, 450);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QHBoxLayout* layout = new QHBoxLayout(this);

    // 左侧：日历选择器
    m_calendar = new QCalendarWidget(this);
    m_calendar->setGridVisible(true);
    m_calendar->setMaximumWidth(350);
    m_calendar->setMinimumDate(findEarliestLogDate());
    m_calendar->setMaximumDate(QDate::currentDate());

    // 右侧：日志文本显示
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet("font-size: 13px; line-height: 1.5; background-color: #FAFAFA; border: 1px solid #CCC;");

    layout->addWidget(m_calendar);
    layout->addWidget(m_textEdit);

    connect(m_calendar, &QCalendarWidget::clicked, this, &LogViewerDialog::onDateChanged);

    // 打开时默认加载当天的日志
    loadLogForDate(QDate::currentDate());
}

void LogViewerDialog::onDateChanged(const QDate &date) {
    loadLogForDate(date);
}

void LogViewerDialog::loadLogForDate(const QDate& date) {
    QString yearMonth = date.toString("yyyy-MM");
    QString dateStr = date.toString("yyyy-MM-dd");

    // 拼接文件路径：程序的运行目录/log/yyyy-MM/yyyy-MM-dd.txt
    QString path = QCoreApplication::applicationDirPath() + "/log/" + yearMonth + "/" + dateStr + ".txt";
    QFile file(path);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        m_textEdit->setPlainText(in.readAll());
        file.close();
    } else {
        m_textEdit->setPlainText(QString("【 %1 】 没有查找到日志记录。").arg(dateStr));
    }
}

// ========================================================
// MainWindow 里的日志读写功能
// ========================================================
void MainWindow::appendLogToFile(const QString& msg) {
    QDateTime now = QDateTime::currentDateTime();
    QString yearMonth = now.toString("yyyy-MM");
    QString dateStr = now.toString("yyyy-MM-dd");
    QString timeStr = now.toString("HH:mm:ss");

    QDir dir(QCoreApplication::applicationDirPath());

    // 检查并创建根目录 log/
    if (!dir.exists("log")) dir.mkdir("log");
    dir.cd("log");

    // 检查并创建子目录 年月/
    if (!dir.exists(yearMonth)) dir.mkdir(yearMonth);
    dir.cd(yearMonth);

    // 以追加模式打开当天的 txt 文件
    QFile file(dir.absoluteFilePath(dateStr + ".txt"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "[" << timeStr << "] " << msg << "\n";
        file.close();
    }
}

void MainWindow::showAndSaveLog(const QString& msg) {
    if (m_statusLabel) {
        m_statusLabel->setText(msg);
    }
    appendLogToFile(msg);
}

void MainWindow::showLogViewer() {
    if (!m_logViewerDialog) {
        m_logViewerDialog = new LogViewerDialog(this);
    }

    m_logViewerDialog->show();
    m_logViewerDialog->raise();
    m_logViewerDialog->activateWindow();
}

// ========================================================
// 自动扫描 log 文件夹，找出最早的日志文件日期
// ========================================================
QDate LogViewerDialog::findEarliestLogDate() {
    QDir logDir(QCoreApplication::applicationDirPath() + "/log");
    if (!logDir.exists()) return QDate::currentDate();

    // 获取按名称排序的月份文件夹列表
    QStringList monthDirs = logDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (monthDirs.isEmpty()) return QDate::currentDate();

    QString earliestMonth = monthDirs.first();
    QDir monthDir(logDir.absoluteFilePath(earliestMonth));

    // 获取该月份下按名称排序的 txt 文件列表
    QStringList logFiles = monthDir.entryList(QStringList() << "*.txt", QDir::Files, QDir::Name);
    if (logFiles.isEmpty()) return QDate::currentDate();

    QString earliestFile = logFiles.first();
    earliestFile.chop(4);

    QDate earliestDate = QDate::fromString(earliestFile, "yyyy-MM-dd");
    if (earliestDate.isValid()) {
        return earliestDate;
    }
    return QDate::currentDate();
}

// ========================================================
// 右键菜单与移动行功能 (支持单行或多行选中)
// ========================================================
void MainWindow::showTableContextMenu(const QPoint &pos)
{
    QModelIndex index = dataTable->indexAt(pos);
    if (!index.isValid()) return;

    int row = index.row();
    // 如果右键点击的行不在当前的选中列表中，则清空其他选中，单独选中该行
    if (!dataTable->selectionModel()->isSelected(index)) {
        dataTable->clearSelection();
        dataTable->selectRow(row);
    }

    QMenu menu(this);
    QAction* moveUpAct = menu.addAction("上移一行");
    QAction* moveDownAct = menu.addAction("下移一行");
    menu.addSeparator();
    QAction* moveToTopAct = menu.addAction("移动到表头 (置顶)");
    QAction* moveToBottomAct = menu.addAction("移动到表尾 (置底)");

    QAction* res = menu.exec(dataTable->viewport()->mapToGlobal(pos));
    if (!res) return;

    if (res == moveUpAct) moveSelectedRowsUp();
    else if (res == moveDownAct) moveSelectedRowsDown();
    else if (res == moveToTopAct) moveSelectedRowsToTop();
    else if (res == moveToBottomAct) moveSelectedRowsToBottom();
}

void MainWindow::updateTableIndices()
{
    for (int i = 0; i < dataTable->rowCount(); ++i) {
        if (QTableWidgetItem* item = dataTable->item(i, 0)) {
            item->setText(QString::number(i + 1));
        }
    }
}

void MainWindow::moveSelectedRowsUp()
{
    QList<int> rows;
    for (auto item : dataTable->selectedItems()) {
        if (item->column() == 0 && !rows.contains(item->row())) rows.append(item->row());
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) return;

    saveUndoState(); // 保存状态，支持撤销
    disconnect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);

    int minAvailableRow = 0;
    for (int row : rows) {
        if (row > minAvailableRow) {
            int targetRow = row - 1;
            m_displayPaths.swapItemsAt(row, targetRow); // 更新底层数据

            // 交换 UI 表格文本
            QString typeTarget = dataTable->item(targetRow, 1)->text();
            QString typeCurrent = dataTable->item(row, 1)->text();
            dataTable->item(targetRow, 1)->setText(typeCurrent);
            dataTable->item(row, 1)->setText(typeTarget);

            // 交换 UI 选中状态
            dataTable->item(targetRow, 0)->setSelected(true);
            dataTable->item(targetRow, 1)->setSelected(true);
            dataTable->item(row, 0)->setSelected(false);
            dataTable->item(row, 1)->setSelected(false);

            minAvailableRow = targetRow + 1;
        } else {
            minAvailableRow = row + 1;
        }
    }

    updateTableIndices(); // 刷新序号列
    connect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->update();
    handleTableSelectionChanged();
}

void MainWindow::moveSelectedRowsDown()
{
    QList<int> rows;
    for (auto item : dataTable->selectedItems()) {
        if (item->column() == 0 && !rows.contains(item->row())) rows.append(item->row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>()); // 从下往上遍历
    if (rows.isEmpty()) return;

    saveUndoState();
    disconnect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);

    int maxAvailableRow = m_displayPaths.size() - 1;
    for (int row : rows) {
        if (row < maxAvailableRow) {
            int targetRow = row + 1;
            m_displayPaths.swapItemsAt(row, targetRow);

            QString typeTarget = dataTable->item(targetRow, 1)->text();
            QString typeCurrent = dataTable->item(row, 1)->text();
            dataTable->item(targetRow, 1)->setText(typeCurrent);
            dataTable->item(row, 1)->setText(typeTarget);

            dataTable->item(targetRow, 0)->setSelected(true);
            dataTable->item(targetRow, 1)->setSelected(true);
            dataTable->item(row, 0)->setSelected(false);
            dataTable->item(row, 1)->setSelected(false);

            maxAvailableRow = targetRow - 1;
        } else {
            maxAvailableRow = row - 1;
        }
    }

    updateTableIndices();
    connect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->update();
    handleTableSelectionChanged();
}

void MainWindow::moveSelectedRowsToTop()
{
    QList<int> rows;
    for (auto item : dataTable->selectedItems()) {
        if (item->column() == 0 && !rows.contains(item->row())) rows.append(item->row());
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) return;

    saveUndoState();
    disconnect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);

    int insertPos = 0;
    for (int row : rows) {
        if (row > insertPos) {
            Contour c = m_displayPaths.takeAt(row);
            m_displayPaths.insert(insertPos, c);

            QString typeStr = dataTable->item(row, 1)->text();
            dataTable->removeRow(row);
            dataTable->insertRow(insertPos);

            QTableWidgetItem* indexItem = new QTableWidgetItem();
            indexItem->setTextAlignment(Qt::AlignCenter);
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(insertPos, 0, indexItem);

            QTableWidgetItem* item = new QTableWidgetItem(typeStr);
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(insertPos, 1, item);

            dataTable->item(insertPos, 0)->setSelected(true);
            dataTable->item(insertPos, 1)->setSelected(true);
        }
        insertPos++;
    }

    updateTableIndices();
    connect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->update();
    handleTableSelectionChanged();
}

void MainWindow::moveSelectedRowsToBottom()
{
    QList<int> rows;
    for (auto item : dataTable->selectedItems()) {
        if (item->column() == 0 && !rows.contains(item->row())) rows.append(item->row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    if (rows.isEmpty()) return;

    saveUndoState();
    disconnect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);

    int insertPos = m_displayPaths.size() - 1;
    for (int row : rows) {
        if (row < insertPos) {
            Contour c = m_displayPaths.takeAt(row);
            m_displayPaths.insert(insertPos, c);

            QString typeStr = dataTable->item(row, 1)->text();
            dataTable->removeRow(row);
            dataTable->insertRow(insertPos);

            QTableWidgetItem* indexItem = new QTableWidgetItem();
            indexItem->setTextAlignment(Qt::AlignCenter);
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(insertPos, 0, indexItem);

            QTableWidgetItem* item = new QTableWidgetItem(typeStr);
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(insertPos, 1, item);

            dataTable->item(insertPos, 0)->setSelected(true);
            dataTable->item(insertPos, 1)->setSelected(true);
        }
        insertPos--;
    }

    updateTableIndices();
    connect(dataTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleTableSelectionChanged);
    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->update();
    handleTableSelectionChanged();
}

void MainWindow::toggleRobotPower()
{
    // 如果 ID 还是 0，说明压根还没点“建立连接”
    if (m_currentDevId == 0) {
        QMessageBox::warning(this, "未连接", "请先在上方菜单中建立机器人连接！");
        return;
    }

    if (!m_isRobotPoweredOn) {
        showAndSaveLog(QString("正在尝试为机器人(Id: %1)上电...").arg(m_currentDevId));

        // 👇 传入刚才 ConnectRobot 拿到的那个 m_currentDevId
        int ret = RobotAPI::PowerOn();

        if (ret == 0) {
            m_isRobotPoweredOn = true;
            m_powerBtn->setText("机器人断电");
            m_powerBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #E53935; color: white; border: 1px solid #D32F2F; border-radius: 4px; padding: 5px 15px;"
                "}"
                "QPushButton:hover { background-color: #EF5350; }"
                );
            showAndSaveLog("机器人上电成功！");
        } else {
            showAndSaveLog(QString("上电失败！错误码: %1").arg(ret));
        }
    } else {
        showAndSaveLog(QString("正在尝试关闭机器人(Id: %1)伺服...").arg(m_currentDevId));

        // 👇 同样传入获取到的 ID
        int ret = RobotAPI::PowerOff();

        if (ret == 0) {
            m_isRobotPoweredOn = false;
            m_powerBtn->setText("机器人上电");
            m_powerBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #2196F3; color: white; border: 1px solid #1E88E5; border-radius: 4px; padding: 5px 15px;"
                "}"
                "QPushButton:hover { background-color: #42A5F5; }"
                );
            showAndSaveLog("机器人断电成功！");
        } else {
            showAndSaveLog(QString("断电失败！错误码: %1").arg(ret));
        }
    }
}
