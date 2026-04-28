#include "positioningdialog.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMouseEvent>

// ========================================================
// 1. 数据模型与轮廓生成
// ========================================================
QPainterPath PositioningBlock::getPath() const {
    QPainterPath path;
    QTransform t;
    t.translate(x, y);
    t.rotate(angle);

    QPainterPath localPath;
    switch(type) {
    case PosBlockType::Line:
        localPath.addRect(-width/2.0, -length/2.0, width, length);
        break;
    case PosBlockType::Point:
        // 生成 L 形定位块 (硬编码一个美观的比例)
        localPath.moveTo(0, 20); localPath.lineTo(0, 0); localPath.lineTo(20, 0);
        localPath.lineTo(20, 5); localPath.lineTo(5, 5); localPath.lineTo(5, 20);
        localPath.closeSubpath();
        break;
    case PosBlockType::Arc:
        // 圆弧定位 (生成一个扇形切角块)
        localPath.moveTo(0, 0);
        localPath.arcTo(-radius, -radius, radius*2, radius*2, 0, 90);
        localPath.closeSubpath();
        break;
    case PosBlockType::Circle:
        // 外方内圆
        localPath.addRect(-radius*1.2, -radius*1.2, radius*2.4, radius*2.4);
        localPath.addEllipse(QPointF(0,0), radius, radius);
        break;
    }
    return t.map(localPath);
}

// ========================================================
// 2. 左侧预览绘图区
// ========================================================
PreviewArea::PreviewArea(QWidget *parent) : QWidget(parent) {
    setMinimumWidth(400);
    setStyleSheet("background-color: white; border: 1px solid #ccc;");
}

void PreviewArea::addBlock(const PositioningBlock& block) {
    m_blocks.append(block);
    m_selectedIndex = m_blocks.size() - 1; // 默认选中刚加的
    update();
}

void PreviewArea::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_blocks.isEmpty()) return;

    // 自动居中与缩放算法
    QRectF bounds;
    for (const auto& b : std::as_const(m_blocks)) bounds = bounds.united(b.getPath().boundingRect());

    double scale = qMin(width() / (bounds.width() + 40), height() / (bounds.height() + 40));
    if (scale > 5.0) scale = 5.0; // 防止只有小物件时放得太大

    painter.translate(width()/2.0, height()/2.0);
    painter.scale(scale, -scale);
    painter.translate(-bounds.center());

    m_transform = painter.transform(); // 记录给鼠标点击用

    // 绘制所有块
    for (int i = 0; i < m_blocks.size(); ++i) {
        QPainterPath p = m_blocks[i].getPath();
        if (i == m_selectedIndex) {
            painter.setPen(QPen(Qt::red, 2/scale));
            painter.setBrush(QColor(255, 0, 0, 50));
        } else {
            painter.setPen(QPen(Qt::blue, 2/scale));
            painter.setBrush(QColor(0, 0, 255, 20));
        }
        painter.drawPath(p);
    }
}

void PreviewArea::mousePressEvent(QMouseEvent *event) {
    QPointF pos = m_transform.inverted().map(QPointF(event->pos()));
    m_selectedIndex = -1;
    for (int i = m_blocks.size() - 1; i >= 0; --i) {
        if (m_blocks[i].getPath().contains(pos)) {
            m_selectedIndex = i;
            break;
        }
    }
    update();
}

void PreviewArea::contextMenuEvent(QContextMenuEvent *event) {
    if (m_selectedIndex < 0) return;

    QMenu menu(this);
    QAction *actDel = menu.addAction("删除");
    QAction *actMove = menu.addAction("移动");
    QAction *actRot = menu.addAction("旋转");

    QAction *res = menu.exec(event->globalPos());
    if (res == actDel) {
        m_blocks.removeAt(m_selectedIndex);
        m_selectedIndex = -1;
    } else if (res == actMove) {
        bool ok;
        double dx = QInputDialog::getDouble(this, "移动", "输入 X 轴偏移量:", 0, -10000, 10000, 2, &ok);
        if (ok) {
            double dy = QInputDialog::getDouble(this, "移动", "输入 Y 轴偏移量:", 0, -10000, 10000, 2, &ok);
            if (ok) { m_blocks[m_selectedIndex].x += dx; m_blocks[m_selectedIndex].y += dy; }
        }
    } else if (res == actRot) {
        bool ok;
        double a = QInputDialog::getDouble(this, "旋转", "输入增加的旋转角度:", 0, -360, 360, 2, &ok);
        if (ok) m_blocks[m_selectedIndex].angle += a;
    }
    update();
}

// ========================================================
// 3. 浮动参数设置窗口
// ========================================================
QDoubleSpinBox* PositioningDialog::createSpinBox(double min, double max, double val) {
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(min, max); sb->setValue(val); sb->setDecimals(2);
    return sb;
}

PositioningDialog::PositioningDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("建立定位 (浮动设置)");
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setMinimumSize(800, 500);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    m_previewArea = new PreviewArea(this);
    mainLayout->addWidget(m_previewArea, 2); // 左侧占比 2

    // 右侧设置区
    QVBoxLayout *rightLayout = new QVBoxLayout();

    // 顶部类别按钮
    QHBoxLayout *typeLayout = new QHBoxLayout();
    QPushButton *btnLine = new QPushButton("直线定位");
    QPushButton *btnPt = new QPushButton("点定位");
    QPushButton *btnArc = new QPushButton("圆弧定位");
    QPushButton *btnCir = new QPushButton("圆定位");
    typeLayout->addWidget(btnLine); typeLayout->addWidget(btnPt);
    typeLayout->addWidget(btnArc); typeLayout->addWidget(btnCir);
    rightLayout->addLayout(typeLayout);

    // 堆叠的参数输入区
    m_stackedWidget = new QStackedWidget(this);

    // Page 0: Line
    QWidget *pageLine = new QWidget(); QFormLayout *fl0 = new QFormLayout(pageLine);
    fl0->addRow("长度:", m_lineLen = createSpinBox(0, 10000, 50));
    fl0->addRow("宽度:", m_lineWidth = createSpinBox(0, 10000, 10));
    fl0->addRow("X 坐标:", m_lineX = createSpinBox());
    fl0->addRow("Y 坐标:", m_lineY = createSpinBox());
    fl0->addRow("旋转角度:", m_lineAngle = createSpinBox(-360, 360));
    m_stackedWidget->addWidget(pageLine);

    // Page 1: Point
    QWidget *pagePt = new QWidget(); QFormLayout *fl1 = new QFormLayout(pagePt);
    fl1->addRow("X 坐标:", m_ptX = createSpinBox());
    fl1->addRow("Y 坐标:", m_ptY = createSpinBox());
    m_stackedWidget->addWidget(pagePt);

    // Page 2: Arc
    QWidget *pageArc = new QWidget(); QFormLayout *fl2 = new QFormLayout(pageArc);
    fl2->addRow("圆心 X:", m_arcX = createSpinBox());
    fl2->addRow("圆心 Y:", m_arcY = createSpinBox());
    fl2->addRow("圆弧半径:", m_arcR = createSpinBox(0, 10000, 20));
    fl2->addRow("旋转角度:", m_arcAngle = createSpinBox(-360, 360));
    m_stackedWidget->addWidget(pageArc);

    // Page 3: Circle
    QWidget *pageCir = new QWidget(); QFormLayout *fl3 = new QFormLayout(pageCir);
    fl3->addRow("圆心 X:", m_cirX = createSpinBox());
    fl3->addRow("圆心 Y:", m_cirY = createSpinBox());
    fl3->addRow("半径:", m_cirR = createSpinBox(0, 10000, 20));
    m_stackedWidget->addWidget(pageCir);

    rightLayout->addWidget(m_stackedWidget);

    // 按钮切换逻辑
    connect(btnLine, &QPushButton::clicked, [this](){ m_stackedWidget->setCurrentIndex(0); m_currentType=PosBlockType::Line; });
    connect(btnPt,   &QPushButton::clicked, [this](){ m_stackedWidget->setCurrentIndex(1); m_currentType=PosBlockType::Point; });
    connect(btnArc,  &QPushButton::clicked, [this](){ m_stackedWidget->setCurrentIndex(2); m_currentType=PosBlockType::Arc; });
    connect(btnCir,  &QPushButton::clicked, [this](){ m_stackedWidget->setCurrentIndex(3); m_currentType=PosBlockType::Circle; });

    // 底部添加/确认按钮
    rightLayout->addStretch();
    QPushButton *btnAdd = new QPushButton("添加到左侧预览", this);
    btnAdd->setStyleSheet("background-color: #2196F3; color: white; padding: 10px; font-weight:bold;");
    connect(btnAdd, &QPushButton::clicked, this, &PositioningDialog::onAddClicked);
    rightLayout->addWidget(btnAdd);

    QPushButton *btnConfirm = new QPushButton("确定 (映射到主绘图区)", this);
    btnConfirm->setStyleSheet("background-color: #4CAF50; color: white; padding: 10px; font-weight:bold;");
    connect(btnConfirm, &QPushButton::clicked, this, &QDialog::accept);
    rightLayout->addWidget(btnConfirm);

    mainLayout->addLayout(rightLayout, 1); // 右侧占比 1
}

void PositioningDialog::onAddClicked() {
    PositioningBlock b;
    b.type = m_currentType;
    switch(m_currentType) {
    case PosBlockType::Line:
        b.length = m_lineLen->value(); b.width = m_lineWidth->value();
        b.x = m_lineX->value(); b.y = m_lineY->value(); b.angle = m_lineAngle->value();
        break;
    case PosBlockType::Point:
        b.x = m_ptX->value(); b.y = m_ptY->value();
        break;
    case PosBlockType::Arc:
        b.x = m_arcX->value(); b.y = m_arcY->value(); b.radius = m_arcR->value(); b.angle = m_arcAngle->value();
        break;
    case PosBlockType::Circle:
        b.x = m_cirX->value(); b.y = m_cirY->value(); b.radius = m_cirR->value();
        break;
    }
    m_previewArea->addBlock(b);
}
