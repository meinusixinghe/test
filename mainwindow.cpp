#include "mainwindow.h"
#include "EfortSdk.h"
#include "renderarea.h"
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
#include <QThread>
#include <QMetaObject>
#include <QShortcut>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QActionGroup>
#include <QCalendarWidget>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include "taskprogramdialog.h"

// 测试
#include "motiontestdialog.h"

namespace {
struct RobotConnectResult {
    int ret = -1;
    int apiRet = -1;
    unsigned int devId = 0;
};

struct RobotMotionResult {
    bool stopped = false;
    int failedIndex = -1;
    int ret = 0;
};
}

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
    btnRotate = new QPushButton();
    btnRotate->setIcon(QIcon(":/img/images/lasso.png"));
    btnRotate->setIconSize(QSize(16, 16));
    btnRotate->setFixedSize(36, 24);
    btnRotate->setToolTip("旋转线条 (框选后按回车选基准点，再输入角度)");
    btnRotate->setCheckable(true);
    btnRotate->setCursor(Qt::PointingHandCursor);
    btnMove = new QPushButton();
    btnMove->setIcon(QIcon(":/img/images/shift.png"));
    btnMove->setIconSize(QSize(16, 16));
    btnMove->setFixedSize(36, 24);
    btnMove->setToolTip("移动线条 (框选后按回车选基准点)");
    btnMove->setCheckable(true);
    btnMove->setCursor(Qt::PointingHandCursor);
    btnMirror = new QPushButton();
    btnMirror->setIcon(QIcon(":/img/images/mirror.png"));
    btnMirror->setIconSize(QSize(16, 16));
    btnMirror->setFixedSize(36, 24);
    btnMirror->setToolTip("镜像线条 (框选后依次点击两个点确定镜像轴)");
    btnMirror->setCheckable(true);
    btnMirror->setCursor(Qt::PointingHandCursor);

    tools->addWidget(btnRestore);
    tools->addWidget(btnUndo);
    tools->addWidget(btnRotate);
    tools->addWidget(btnMirror);
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
    setFixedWidth(280);
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
    setupUi();

    loadWeldingProcesses();  // 启动时自动加载焊接工艺数据

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusTimer);

    QDir::setCurrent(QCoreApplication::applicationDirPath());
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    m_lastIp = settings.value("RobotIP", "192.168.1.12").toString();
    if (m_lastIp.isEmpty()) {
        m_lastIp = "192.168.1.10";
    }
}

MainWindow::~MainWindow() {

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

    QString btnStyle = R"(
        QPushButton { padding: 6px 12px; background-color: rgba(255, 255, 255, 0.95); border: 1px solid #ccc; border-radius: 4px;}
        QPushButton:checked { background-color: #2196F3; color: white; border-color: #2196F3;}
        QPushButton:hover {border-color: #2196F3; }
    )";

    m_startBtn = new QPushButton("生成运行程序", renderArea);
    m_startBtn->setStyleSheet("QPushButton { background-color: rgba(76, 175, 80, 0.95); color: white; border-radius: 4px; padding: 6px 12px; font-weight:bold; } QPushButton:hover { background-color: #45a049; }");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setVisible(false);

    m_powerBtn = new QPushButton("机器人上电", renderArea);
    m_powerBtn->setStyleSheet(btnStyle);
    m_powerBtn->setCursor(Qt::PointingHandCursor);

    bottomBtnLayout->addWidget(m_startBtn);
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
    // --- 提前创建好所有的功能 Action---
    loadAction = new QAction(QIcon(":/img/images/import.png"), "导入DXF", this);
    rotateAction = new QAction("应用旋转矩阵", this);
    m_manageProcessAction = new QAction(QIcon(":/img/images/icons4.png"), "工艺管理", this);
    m_imageProcessAction = new QAction(QIcon(":/img/images/DrawProcessing.png"), "图纸处理", this);
    QAction* m_positioningAction = new QAction("建立定位", this);
    m_connectAction = new QAction(QIcon(":/img/images/icons6.png"), "建立连接", this);
    m_robotParamAction = new QAction(QIcon(":/img/images/icons4.png"), "机器参数设置", this);
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


    // =================================================================
    // 👇【新增测试代码】：在“操作”菜单下注入测试 Action（极易查找、极易删除）
    // =================================================================
    QPushButton* mlinTestBtn = new QPushButton("测试界面", this);

    // 设置按钮的大小和绝对位置 (X:20, Y:100, 宽:200, 高:40)
    // 你可以根据你的主界面空缺位置，自己调整 20 和 100 这两个坐标数字
    mlinTestBtn->setGeometry(20, 100, 200, 40);

    // 给按钮加上醒目的黄色背景，方便测试完找到它并删掉
    mlinTestBtn->setStyleSheet("background-color: #FFEB3B; font-weight: bold; border-radius: 5px;");
    mlinTestBtn->show(); // 强制显示在最上层

    // 绑定点击事件
    connect(mlinTestBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentDevId == 0) {
            QMessageBox::warning(this, "通信断开", "请确保主界面已成功建立机器人通信连接！");
            return;
        }
        if (!m_motionTestDialog) {
            m_motionTestDialog = new MotionTestDialog(m_currentDevId, this);
            m_motionTestDialog->setWindowFlags(Qt::Tool);
        } else {
            m_motionTestDialog->setDevId(m_currentDevId);
        }
        m_motionTestDialog->show();       // 显示窗口
        m_motionTestDialog->raise();      // 提到最上层
        m_motionTestDialog->activateWindow(); // 激活焦点
    });
    // =================================================================


    // 5. 绑定选项卡点击逻辑，动态切换下方工具栏的内容
    auto switchTab = [=](QAction* currentTab) {
        toolBar->clear(); // 切换时先清空下方工具栏
        if (currentTab == tabFile) {
            toolBar->addAction(loadAction);
        } else if (currentTab == tabOperation) {
            toolBar->addAction(rotateAction);
            toolBar->addAction(m_manageProcessAction);
            toolBar->addAction(m_robotParamAction);
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
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setCursor(Qt::PointingHandCursor);
    m_modeCombo->addItem("手动低速", 0);   // 对应 ROBOX_MODE_MANUAL
    m_modeCombo->addItem("手动高速", 1);   // 对应 ROBOX_MODE_MANUFAST
    m_modeCombo->addItem("自动模式", 2); // 对应 ROBOX_MODE_AUTO
    m_modeCombo->setStyleSheet("QComboBox { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 3px; padding: 2px 6px; }");
    // 分隔符 3
    QLabel* separator3 = new QLabel(" | ", this);
    separator2->setStyleSheet("color: #999; font-weight: bold;");
    //
    m_permissionBtn = new QPushButton("获取控制权", this);
    m_permissionBtn->setCursor(Qt::PointingHandCursor);
    m_permissionBtn->setStyleSheet("background-color: #9E9E9E; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");
    // 按顺序添加到布局
    statusLayout->addWidget(m_statusIconLabel);
    statusLayout->addWidget(m_statusTextLabel);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(separator1);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(m_servoIconLabel);
    statusLayout->addWidget(m_servoTextLabel);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(separator2);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(m_modeCombo);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(separator3);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(m_permissionBtn);
    statusBar()->addPermanentWidget(statusContainer);
    statusLayout->setContentsMargins(5, 0, 5, 0);
    statusLayout->setSpacing(5);

    // 8. 信号槽连接
    connect(loadAction, &QAction::triggered, this, &MainWindow::importDxf);
    connect(dataTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::handleTableSelectionChanged);
    connect(dataTable, &QTableWidget::itemSelectionChanged,this, &MainWindow::handleTableSelectionChanged);
    connect(rotateAction, &QAction::triggered, this, &MainWindow::applyRotationMatrix);
    connect(m_robotParamAction, &QAction::triggered, this, &MainWindow::onRobotParameterSettings);

    // 焊接工艺
    connect(m_manageProcessAction, &QAction::triggered, this, &MainWindow::onManageWeldingProcess);

    m_floatingToolWidget = new FloatingToolWidget(renderArea);
    m_floatingToolWidget->hide();

    // 通信
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::onConnectTriggered);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
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
        m_floatingToolWidget->btnRotate->setChecked(false);
        m_floatingToolWidget->btnMirror->setChecked(false);
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
    connect(m_permissionBtn, &QPushButton::clicked, this, &MainWindow::onPermissionBtnClicked);

    connect(m_floatingToolWidget->sliderEraserSize, &QSlider::valueChanged, this, [this](int value){
        m_floatingToolWidget->lblEraserSize->setText(QString::number(value));
        renderArea->setEraserSize(value);
    });
    connect(m_floatingToolWidget->btnRotate, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnEraser->setChecked(false); m_floatingToolWidget->btnMove->setChecked(false); m_floatingToolWidget->btnMirror->setChecked(false); }
        renderArea->setRotateMode(checked);
    });
    connect(m_floatingToolWidget->btnMirror, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnEraser->setChecked(false); m_floatingToolWidget->btnMove->setChecked(false); m_floatingToolWidget->btnRotate->setChecked(false); }
        renderArea->setMirrorMode(checked);
    });
    connect(m_floatingToolWidget->btnEraser, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnRotate->setChecked(false); m_floatingToolWidget->btnMove->setChecked(false); m_floatingToolWidget->btnMirror->setChecked(false); }
        m_floatingToolWidget->setSliderVisible(checked);
        renderArea->setEraserMode(checked);
    });
    connect(m_floatingToolWidget->btnMove, &QPushButton::toggled, this, [this](bool checked){
        if (checked) { m_floatingToolWidget->btnRotate->setChecked(false); m_floatingToolWidget->btnEraser->setChecked(false); m_floatingToolWidget->btnMirror->setChecked(false); }
        renderArea->setMoveMode(checked);
    });
    // 连接批量删除信号
    connect(renderArea, &RenderArea::bulkPathsDeleted, this, &MainWindow::handleBulkPathsDeleted);
    connect(renderArea, &RenderArea::reorderPathsRequested, this, &MainWindow::reorderPathsGeo);
    connect(renderArea, &RenderArea::cancelModesRequested, this, [this](){
        m_floatingToolWidget->btnRotate->setChecked(false);
        m_floatingToolWidget->btnEraser->setChecked(false);
        m_floatingToolWidget->btnMirror->setChecked(false);
        m_floatingToolWidget->btnMove->setChecked(false);
        m_floatingToolWidget->setSliderVisible(false);
    });
    connect(renderArea, &RenderArea::pathsMoved, this, [this](const QVector<Contour> &updatedPaths){
        saveUndoState();
        this->m_displayPaths = updatedPaths;
    });

    connect(m_positioningAction, &QAction::triggered, this, [this](){
        PositioningDialog* dlg = new PositioningDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowFlags(Qt::Tool);
        dlg->setInitialBlocks(renderArea->getPositioningBlocks());
        connect(dlg, &PositioningDialog::accepted, this, [this, dlg]() {
            renderArea->setPositioningBlocks(dlg->getBlocks());
        });
        dlg->show();
    });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRoboxModeChanged);

    m_floatingToolWidget->installEventFilter(this);
    m_startBtn->installEventFilter(this);

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
            if (contour.type == "圆" && contour.points.size() >= 3) {
                QPointF firstPt = contour.points.first();
                QPointF lastPt = contour.points.last();
                if (std::hypot(firstPt.x() - lastPt.x(), firstPt.y() - lastPt.y()) > 1e-4) {
                    contour.points.append(firstPt);
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
    for (auto item : std::as_const(selectedItems)) {
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

        // --- 几何参数计算逻辑 ---
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

        } else if (c.type.contains("样条") || c.type.contains("拟合")) {
            int numSegments = (c.points.size() - 1) / 2;
            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 %1 参数 】</span><br>"
                               "控制点集:%2 &nbsp;&nbsp;&nbsp; 拟合段数:%3<br>")
                           .arg(c.type).arg(c.points.size()).arg(numSegments);

            for (int s = 0; s < numSegments; ++s) {
                if (s * 2 + 2 < c.points.size()) { // 防越界保护
                    QPointF p1 = c.points[s * 2];       // 起点
                    QPointF p2 = c.points[s * 2 + 1];   // 途经点
                    QPointF p3 = c.points[s * 2 + 2];   // 终点

                    // 利用简单的数学公式判断这3个点是直是弯（三点共线判定）
                    double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));
                    if (std::abs(D) < 1e-6) {
                        infoText += QString("<b>段%1 [直线]</b> 起点:(%2,%3) &nbsp;终点:(%4,%5) &nbsp;长度:%6<br>")
                                        .arg(s+1).arg(p1.x(), 0, 'f', 1).arg(p1.y(), 0, 'f', 1)
                                        .arg(p3.x(), 0, 'f', 1).arg(p3.y(), 0, 'f', 1)
                                        .arg(std::hypot(p3.x()-p1.x(), p3.y()-p1.y()), 0, 'f', 1);
                    } else {
                        double xc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p2.y() - p3.y()) +
                                     (p2.x()*p2.x() + p2.y()*p2.y())*(p3.y() - p1.y()) +
                                     (p3.x()*p3.x() + p3.y()*p3.y())*(p1.y() - p2.y())) / D;
                        double yc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p3.x() - p2.x()) +
                                     (p2.x()*p2.x() + p2.y()*p2.y())*(p1.x() - p3.x()) +
                                     (p3.x()*p3.x() + p3.y()*p3.y())*(p2.x() - p1.x())) / D;
                        double R = std::hypot(p1.x() - xc, p1.y() - yc);

                        infoText += QString("<b>段%1 [圆弧]</b> 起点:(%2,%3) &nbsp;终点:(%4,%5) &nbsp;半径:%6<br>")
                                        .arg(s+1).arg(p1.x(), 0, 'f', 1).arg(p1.y(), 0, 'f', 1)
                                        .arg(p3.x(), 0, 'f', 1).arg(p3.y(), 0, 'f', 1).arg(R, 0, 'f', 1);
                    }
                }
            }
        } else {
            infoText = QString("<span style='font-size:14px; font-weight:bold; color:#333;'>【 %1 】</span><br>包含 %2 个控制点")
                           .arg(c.type).arg(c.points.size());
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
// 功能：打开焊接工艺管理对话框
// ----------------------------------------------------
void MainWindow::onManageWeldingProcess()
{
    WeldingProcessDialog* dialog = new WeldingProcessDialog(&m_weldingProcesses, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlags(Qt::Tool);
    dialog->show();
}

// ----------------------------------------------------
// 功能：启动自动加载数据
// ----------------------------------------------------
void MainWindow::loadWeldingProcesses()
{
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
    if (m_isRobotCommandRunning) {
        showAndSaveLog("机器人连接操作正在执行，请稍候...");
        return;
    }

    ConnectionDialog dlg(this);
    dlg.setIp(m_lastIp);

    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.getAction() == 1) {
            m_lastIp = dlg.getIp();
            QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
            settings.setValue("RobotIP", m_lastIp);

            m_isRobotCommandRunning = true;
            m_connectAction->setEnabled(false);
            showAndSaveLog(QString("正在连接机器人 %1 ...").arg(m_lastIp));

            QThread* worker = QThread::create([this, ip = m_lastIp]() {
                RobotConnectResult result;
                QDir::setCurrent(QCoreApplication::applicationDirPath());
                result.ret = RobotAPI::ConnectRobot(ip.toStdString(), result.devId, true);
                if (result.ret == 0 || result.ret == 10021) {
                    RobotAPI::SelectRobot(result.devId);
                    result.apiRet = RobotAPI::EnableApiControl(true, result.devId);
                }

                QMetaObject::invokeMethod(this, [this, result]() {
                    m_isRobotCommandRunning = false;
                    m_connectAction->setEnabled(true);

                    if (result.ret == 0 || result.ret == 10021) {
                        m_currentDevId = result.devId;
                        showAndSaveLog(QString("SDK连接就绪！分配ID为: %1").arg(m_currentDevId));
                        if (result.apiRet == 0) {
                            showAndSaveLog("已使能当前机器人外部 API 控制权限。");
                        } else {
                            showAndSaveLog(QString("警告：使能 API 权限失败，错误码: %1").arg(result.apiRet));
                        }

                        m_statusTimer->start(200); // 启动状态轮询
                    } else {
                        showAndSaveLog(QString("SDK连接失败！错误码: %1").arg(result.ret));
                        m_currentDevId = 0;
                    }
                }, Qt::QueuedConnection);
            });
            connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            worker->start();
        } else if (dlg.getAction() == 2) {
            if (m_currentDevId != 0) {
                m_isRobotCommandRunning = true;
                m_connectAction->setEnabled(false);
                m_statusTimer->stop(); // 停止轮询
                unsigned int devId = m_currentDevId;
                m_currentDevId = 0;
                showAndSaveLog(QString("正在断开机器人连接(Id: %1)...").arg(devId));

                QThread* worker = QThread::create([this, devId]() mutable {
                    RobotAPI::DisconnectRobot(devId);

                    QMetaObject::invokeMethod(this, [this]() {
                        m_isRobotCommandRunning = false;
                        m_connectAction->setEnabled(true);
                        showAndSaveLog("机器人连接已断开。");
                        onStatusTimer();
                    }, Qt::QueuedConnection);
                });
                connect(worker, &QThread::finished, worker, &QObject::deleteLater);
                worker->start();
            }
        }
    }
}

// =============================================================================
// 生成运行程序界面
// =============================================================================
void MainWindow::onStartClicked()
{
    if (m_currentDevId == 0 || !RobotAPI::IsConnected(m_currentDevId)) {
        QMessageBox::warning(this, "未连接", "请先连接机器人。");
        return;
    }

    if (m_displayPaths.isEmpty()) {
        QMessageBox::warning(this, "无图纸", "当前没有可运行的加工线条，请先导入 DXF 图纸！");
        return;
    }

    TaskProgramDialog* dlg = new TaskProgramDialog(m_currentDevId, m_displayPaths, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ----------------------------------------------------
// 窗口关闭事件，保证安全退出下发指令并断开 TCP 连接
// ----------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isShuttingDown) {
        event->ignore();
        return;
    }

    if (m_isRobotCommandRunning) {
        if (m_statusLabel) {
            m_statusLabel->setText("机器人连接操作正在执行，请稍候再关闭。");
        }
        event->ignore();
        return;
    }

    if (m_statusTimer && m_statusTimer->isActive()) {
        m_statusTimer->stop();
    }

    if (m_currentDevId != 0 && RobotAPI::IsConnected(m_currentDevId)) {
        if (m_statusLabel) {
            m_statusLabel->setText("正在安全停止机器人并清理状态...");
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_isShuttingDown = true;
        m_isRobotCommandRunning = true;
        m_connectAction->setEnabled(false);
        unsigned int devId = m_currentDevId;
        m_currentDevId = 0;
        event->ignore();

        QThread* worker = QThread::create([this, devId]() mutable {
            RobotAPI::MOVEHOLD(devId);
            RobotAPI::MOVECLEAR(devId);
            RobotAPI::TerminateProgram(devId);
            RobotAPI::PowerOff(devId);
            RobotAPI::DisconnectRobot(devId);

            QMetaObject::invokeMethod(this, [this]() {
                QApplication::restoreOverrideCursor();
                m_isRobotCommandRunning = false;
                m_isShuttingDown = false;
                close();
            }, Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
        return;
    }

    event->accept();
}

void MainWindow::restoreDrawing()
{
    m_floatingToolWidget->btnRestore->clearFocus();
    m_floatingToolWidget->btnRestore->setDown(false);
    m_floatingToolWidget->btnRotate->setChecked(false);
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
        showAndSaveLog("图纸已还原到导入后的初始状态。");
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
        watched == m_startBtn ||
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

void MainWindow::onStatusTimer()
{
    if (m_currentDevId == 0 || !RobotAPI::IsConnected(m_currentDevId)) {
        // 如果断线了，UI 变红
        m_statusIconLabel->setStyleSheet("background-color: #F44336; border-radius: 8px;");
        m_statusTextLabel->setText("未连接");
        m_startBtn->setVisible(false);
        return;
    }

    // 正常连接时，UI 变绿
    m_statusIconLabel->setStyleSheet("background-color: #4CAF50; border-radius: 8px;");
    m_statusTextLabel->setText("已连接");
    m_startBtn->setVisible(true);

    // 1. 获取伺服状态
    bool servoStatus = false;
    RobotAPI::GetCurrentServoStatus(servoStatus, m_currentDevId);
    if (servoStatus) {
        m_servoIconLabel->setStyleSheet("background-color: #4CAF50; border-radius: 8px;");
        m_servoTextLabel->setText("伺服使能");
    } else {
        m_servoIconLabel->setStyleSheet("background-color: #9E9E9E; border-radius: 8px;");
        m_servoTextLabel->setText("伺服断开");
    }
    if (servoStatus != m_isRobotPoweredOn) {
        m_isRobotPoweredOn = servoStatus; // 同步标志位

        if (m_isRobotPoweredOn) {
            m_powerBtn->setText("机器人断电");
            m_powerBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #E53935; color: white; border: 1px solid #D32F2F; border-radius: 4px; padding: 5px 15px;"
                "}"
                "QPushButton:hover { background-color: #EF5350; }"
                );
        } else {
            m_powerBtn->setText("机器人上电");
            m_powerBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #2196F3; color: white; border: 1px solid #1E88E5; border-radius: 4px; padding: 5px 15px;"
                "}"
                "QPushButton:hover { background-color: #42A5F5; }"
                );
        }
    }

    // 2. 获取自动/手动模式
    RoboxKeyMode keyMode = RoboxKeyMode::ROBOX_MODE_MANUAL;
    RobotAPI::GetCurrentKeyMode(keyMode, m_currentDevId);

    if (m_modeCombo) {
        m_modeCombo->blockSignals(true);
        if (keyMode == RoboxKeyMode::ROBOX_MODE_AUTO) {
            m_modeCombo->setCurrentIndex(2);
            m_modeCombo->setStyleSheet("QComboBox { background-color: #E53935; color: white; border-radius: 3px; padding: 2px 6px; }");
        } else if (keyMode == RoboxKeyMode::ROBOX_MODE_MANUFAST) {
            m_modeCombo->setCurrentIndex(1);
            m_modeCombo->setStyleSheet("QComboBox { background-color: #FF9800; color: white; border-radius: 3px; padding: 2px 6px; }");
        } else {
            m_modeCombo->setCurrentIndex(0);
            m_modeCombo->setStyleSheet("QComboBox { background-color: #4CAF50; color: white; border-radius: 3px; padding: 2px 6px; }");
        }

        m_lastModeIndex = m_modeCombo->currentIndex(); // 记录正确的底层状态
        m_modeCombo->blockSignals(false); // 解除阻塞
    }

    // 3. 监控报警状态
    bool alarmStatus = false;
    RobotAPI::GetCurrentAlarmStatus(alarmStatus, m_currentDevId);
    if (alarmStatus) {
        m_statusLabel->setText("机器人当前存在报警，请复位！");
    }
}

// ----------------------------------------------------
// 功能：打开机器参数设置对话框
// ----------------------------------------------------
void MainWindow::onRobotParameterSettings()
{
    // 如果还没连接，拦住不让进
    if (m_currentDevId == 0) {
        QMessageBox::warning(this, "未连接", "请先在“连接”选项卡中建立机器人通信！");
        return;
    }

    RobotParameterDialog* dialog = new RobotParameterDialog(m_currentDevId, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlags(Qt::Tool);
    dialog->show();
}

// =================================================================
// 功能：示教器权限获取与释放 (异步无阻塞)
// =================================================================
void MainWindow::onPermissionBtnClicked()
{
    // 防呆拦截：必须先建立网络连接
    if (m_currentDevId == 0) {
        QMessageBox::warning(this, "未连接", "请先连接机器人，再尝试获取控制权！");
        return;
    }

    // 防止用户狂点导致指令堆叠，先禁用按钮并变色提示
    m_permissionBtn->setEnabled(false);
    m_permissionBtn->setText("正在切换权限...");
    m_permissionBtn->setStyleSheet("background-color: #FF9800; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");

    // 开启子线程去跟控制器通信
    QThread* worker = QThread::create([this]() {
        int ret = -1;
        bool targetState = !m_hasPermission; // 目标状态是当前状态的反转

        // 调用对应的 SDK 接口
        if (targetState) {
            ret = RobotAPI::GetPermission(m_currentDevId); // 去获取
        } else {
            ret = RobotAPI::ReleasePermission(m_currentDevId); // 去释放
        }

        // 通信完毕，安全切回主 UI 线程更新界面
        QMetaObject::invokeMethod(this, [this, ret, targetState]() {
            m_permissionBtn->setEnabled(true); // 恢复按钮可点

            if (ret == 0) {
                // 成功：更新本地标志位
                m_hasPermission = targetState;

                // 根据新状态，改变右下角按钮的颜色和文字
                if (m_hasPermission) {
                    m_permissionBtn->setText("释放控制权");
                    m_permissionBtn->setStyleSheet("background-color: #4CAF50; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");
                } else {
                    m_permissionBtn->setText("获取控制权");
                    m_permissionBtn->setStyleSheet("background-color: #9E9E9E; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");
                }
            } else {
                // 失败：报错并恢复原本的按钮样式
                QMessageBox::warning(this, "权限切换失败",
                                     QString("操作失败！\n错误码: %1\n\n提示：如果是获取失败，请检查示教器屏幕上的【权限状态灯】是否为黄色（已释放状态）。").arg(ret));

                if (m_hasPermission) {
                    m_permissionBtn->setText("释放控制权");
                    m_permissionBtn->setStyleSheet("background-color: #4CAF50; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");
                } else {
                    m_permissionBtn->setText("获取控制权");
                    m_permissionBtn->setStyleSheet("background-color: #9E9E9E; color: white; border-radius: 4px; padding: 3px 10px; font-weight: bold;");
                }
            }
        }, Qt::QueuedConnection);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// =================================================================
// 功能：异步切换机器人运行模式 (手动/自动)
// =================================================================
void MainWindow::onRoboxModeChanged(int index)
{
    // 防呆 1：未连接机器人
    if (m_currentDevId == 0) {
        QMessageBox::warning(this, "未连接", "请先连接机器人！");
        m_modeCombo->blockSignals(true);
        m_modeCombo->setCurrentIndex(m_lastModeIndex); // 强行把下拉框切回原来的样子
        m_modeCombo->blockSignals(false);
        return;
    }

    // 防呆 2：未获取控制权
    if (!m_hasPermission) {
        QMessageBox::warning(this, "无控制权", "切换运行模式极度危险！\n必须先在右侧点击【获取控制权】！");
        m_modeCombo->blockSignals(true);
        m_modeCombo->setCurrentIndex(m_lastModeIndex);
        m_modeCombo->blockSignals(false);
        return;
    }

    // 冻结下拉框，防止在网络通信期间用户乱拨
    m_modeCombo->setEnabled(false);

    // 取出对应的枚举值 (0, 1, 或 2)
    int modeValue = m_modeCombo->itemData(index).toInt();
    unsigned int devId = m_currentDevId;

    QThread* worker = QThread::create([this, modeValue, devId]() {

        // 调用底层 SDK 切换模式
        int ret = RobotAPI::SwitchRoboxMode(static_cast<RoboxKeyMode>(modeValue), devId);

        // 切回主 UI 线程处理结果
        QMetaObject::invokeMethod(this, [this, ret]() {
            m_modeCombo->setEnabled(true);

            if (ret == 0) {
                // 成功：立马刷新一下状态，让按钮变色
                showAndSaveLog(QString("运行模式切换成功！"));
                onStatusTimer();
            } else {
                // 失败：弹窗报错，并安全地把下拉框 UI 拨回到修改前的状态
                QMessageBox::warning(this, "模式切换失败",
                                     QString("控制器拒绝了模式切换请求！错误码: %1\n\n核心排查：请确保已拔掉或在后台断开了一切实体/虚拟示教器！").arg(ret));

                m_modeCombo->blockSignals(true);
                m_modeCombo->setCurrentIndex(m_lastModeIndex);
                m_modeCombo->blockSignals(false);
            }
        }, Qt::QueuedConnection);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// =============================================================================
// 空间几何排序
// =============================================================================
void MainWindow::reorderPathsGeo()
{
    if (m_displayPaths.isEmpty()) return;

    // saveUndoState(); // 如果你实现了撤销队列，保留此行以支持 Ctrl+Z

    QVector<Contour> original = m_displayPaths;
    QVector<Contour> reordered;
    QList<int> unvisited;
    for(int i = 0; i < original.size(); ++i) {
        unvisited.append(i);
    }

    // 辅助函数：计算图元的“左下角评分”（数值越小，越靠近左下角）
    auto getBottomLeftScore = [](const Contour& c) {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        for (const QPointF& pt : c.points) {
            if (pt.x() < minX) minX = pt.x();
            if (pt.y() < minY) minY = pt.y(); // DXF坐标中 Y越小越靠下
        }
        double score = minX + minY;

        // 【核心】：如果是直线，给予极高的优先级权重，让它优先作为起刀点
        if (c.type.contains("直线")) {
            score -= 1000.0;
        }
        return score;
    };

    const double TOL = 0.5; // 首尾相连的吸附容差（0.5毫米）

    // 核心算法大循环：由外向内逐层剥离
    while (!unvisited.isEmpty()) {

        // 1. 寻找当前剩余图元中，最左下角（且优先直线）的图元，作为新一圈（层）的起点
        int bestStartIdx = -1;
        double bestScore = std::numeric_limits<double>::max();

        for (int idx : unvisited) {
            double score = getBottomLeftScore(original[idx]);
            if (score < bestScore) {
                bestScore = score;
                bestStartIdx = idx;
            }
        }

        // 2. 剥离并加入新一层的起点
        int currentIdx = bestStartIdx;
        unvisited.removeOne(currentIdx);
        reordered.append(original[currentIdx]);

        // 3. 贪心缝合：顺藤摸瓜，寻找首尾相连的下一根线，直到走完这一整圈闭环
        bool foundNext = true;
        while (foundNext && !unvisited.isEmpty()) {
            foundNext = false;
            QPointF currentEnd = reordered.last().points.last(); // 当前轨迹的物理终点

            int nextIdx = -1;
            bool needReverse = false;
            double bestDist = std::numeric_limits<double>::max();

            // 在未访问的图元中找最近的连接点
            for (int idx : unvisited) {
                const Contour& candidate = original[idx];
                if (candidate.points.isEmpty()) continue;

                QPointF candStart = candidate.points.first();
                QPointF candEnd = candidate.points.last();

                double distToStart = std::hypot(candStart.x() - currentEnd.x(), candStart.y() - currentEnd.y());
                double distToEnd = std::hypot(candEnd.x() - currentEnd.x(), candEnd.y() - currentEnd.y());

                // 情况A：下一根线的起点 连着 当前终点（标准正向）
                if (distToStart <= TOL && distToStart < bestDist) {
                    bestDist = distToStart;
                    nextIdx = idx;
                    needReverse = false;
                }
                // 情况B：下一根线的终点 连着 当前终点（说明画图时线画反了，记录翻转需求）
                if (distToEnd <= TOL && distToEnd < bestDist) {
                    bestDist = distToEnd;
                    nextIdx = idx;
                    needReverse = true;
                }
            }

            // 如果找到了相连的线，将其加入队列并继续顺藤摸瓜
            if (nextIdx != -1) {
                Contour nextContour = original[nextIdx];
                if (needReverse) {
                    // 【智能纠错】：自动翻转画反的节点顺序，保证机器人在交接点不抬刀！
                    QVector<QPointF> revPts;
                    for (int i = nextContour.points.size() - 1; i >= 0; --i) {
                        revPts.append(nextContour.points[i]);
                    }
                    nextContour.points = revPts;
                }
                reordered.append(nextContour);
                unvisited.removeOne(nextIdx);
                foundNext = true;
            }
        }
        // 如果 foundNext 为 false，说明这一圈首尾闭合了或者断开了。
        // While 循环会自动回到顶部，去剩下的内层图元里寻找下一个左下角起点！
    }

    // 4. 应用重新排序的数据
    m_displayPaths = reordered;

    // 清空并重新填充右侧表格
    dataTable->blockSignals(true);
    dataTable->setRowCount(0);

    for (int i = 0; i < m_displayPaths.size(); ++i) {
        dataTable->insertRow(i);

        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 0, indexItem);

        QTableWidgetItem *typeItem = new QTableWidgetItem(m_displayPaths[i].type);
        typeItem->setTextAlignment(Qt::AlignCenter);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        dataTable->setItem(i, 1, typeItem);
    }
    dataTable->blockSignals(false);

    // 清理状态并刷新视图
    renderArea->clearSelection();
    if (detailWidget) detailWidget->hide();

    renderArea->setDisplayPaths(m_displayPaths);
    renderArea->update();

    QMessageBox::information(this, "排序完成", "轨迹已按照【外层至内层、左下角起刀、连续首尾相连】的逻辑优化完毕！");
}
