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
#include <QToolButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLabel>

// ========================================================
// 1. 数据模型与轮廓生成
// ========================================================
QPainterPath PositioningBlock::getPath() const {
    QTransform t;
    t.translate(x, y);
    t.rotate(angle);

    QPainterPath localPath;
    switch(type) {
    case PosBlockType::Line:
        localPath.addRect(-width/2.0, -length/2.0, width, length);
        break;
    case PosBlockType::Point:
        localPath.moveTo(0, 20); localPath.lineTo(0, 0); localPath.lineTo(20, 0);
        localPath.lineTo(20, -5); localPath.lineTo(-5, -5); localPath.lineTo(-5, 20);
        localPath.closeSubpath();
        break;
    case PosBlockType::Arc: {
        double L = radius * 1.25;
        localPath.moveTo(0, -radius);
        localPath.lineTo(0, -L);
        localPath.lineTo(L, -L);
        localPath.lineTo(L, 0);
        localPath.lineTo(radius, 0);
        localPath.arcTo(-radius, -radius, radius * 2, radius * 2, 0, 90);
        localPath.closeSubpath();
        break;
    }
    case PosBlockType::Circle:
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
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PreviewArea::autoFit() {
    if (m_blocks.isEmpty()) return;

    QRectF bounds;
    for (const auto& b : std::as_const(m_blocks)) bounds = bounds.united(b.getPath().boundingRect());

    double sx = width() / (bounds.width() + 40);
    double sy = height() / (bounds.height() + 40);
    m_scaleFactor = qMin(sx, sy);
    if (m_scaleFactor > 5.0) m_scaleFactor = 5.0;

    m_panOffset = QPointF(-bounds.center().x() * m_scaleFactor, bounds.center().y() * m_scaleFactor);
    update();
}

void PreviewArea::addBlock(const PositioningBlock& block) {
    m_blocks.append(block);
    m_selectedIndex = m_blocks.size() - 1;
    autoFit();
}

void PreviewArea::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_blocks.isEmpty()) return;

    if (m_firstPaint) {
        autoFit();
        m_firstPaint = false;
    }

    painter.translate(width() / 2.0 + m_panOffset.x(), height() / 2.0 + m_panOffset.y());
    painter.scale(m_scaleFactor, -m_scaleFactor);

    m_transform = painter.transform();

    for (int i = 0; i < m_blocks.size(); ++i) {
        QPainterPath p = m_blocks[i].getPath();
        if (i == m_selectedIndex) {
            painter.setPen(QPen(Qt::red, 2.0 / m_scaleFactor));
            painter.setBrush(QColor(255, 0, 0, 50));
        } else {
            painter.setPen(QPen(Qt::blue, 2.0 / m_scaleFactor));
            painter.setBrush(QColor(0, 0, 255, 20));
        }
        painter.drawPath(p);
    }
}

void PreviewArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
        m_isPanning = false;

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
}

void PreviewArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        m_isPanning = true;
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffset += delta;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        update();
    }
}

void PreviewArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_isPanning) {
            setCursor(Qt::ArrowCursor);
            m_isPanning = false;
        }
    }
}

void PreviewArea::wheelEvent(QWheelEvent *event) {
    double zoomFactor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;

    QPointF mousePos = event->position();
    QPointF centerPos(width() / 2.0, height() / 2.0);
    QPointF offset = mousePos - centerPos - m_panOffset;

    m_panOffset -= offset * (zoomFactor - 1.0);
    m_scaleFactor *= zoomFactor;

    if (m_scaleFactor < 0.1) m_scaleFactor = 0.1;
    if (m_scaleFactor > 50.0) m_scaleFactor = 50.0;

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
    setWindowTitle("建立定位");
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setMinimumSize(900, 600);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0); // 紧凑布局
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame *leftFrame = new QFrame(this);
    leftFrame->setFrameShape(QFrame::StyledPanel);
    leftFrame->setStyleSheet("QFrame { background-color: #f0f0f0; border-right: 2px solid #dcdcdc; }");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);

    m_previewArea = new PreviewArea(leftFrame);
    leftLayout->addWidget(m_previewArea);
    mainLayout->addWidget(leftFrame, 2);

    QFrame *rightFrame = new QFrame(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightFrame);
    rightLayout->setContentsMargins(20, 20, 20, 20);
    rightLayout->setSpacing(20);

    QLabel *typeTitle = new QLabel("选择定位块类型");
    typeTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    rightLayout->addWidget(typeTitle);

    QGridLayout *gridSelector = new QGridLayout();
    gridSelector->setVerticalSpacing(15);
    gridSelector->setHorizontalSpacing(60);
    QStringList types = {"直线定位", "点定位", "圆弧定位", "圆定位"};
    QStringList icons = {":/img/images/line_pos.png", ":/img/images/point_pos.png",
                         ":/img/images/arc_pos.png", ":/img/images/circle_pos.png"};

    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->setExclusive(true);

    for (int i = 0; i < 4; ++i) {
        QVBoxLayout *itemLayout = new QVBoxLayout();
        itemLayout->setSpacing(8);
        itemLayout->setAlignment(Qt::AlignCenter);

        // 只包含图片的白框按钮
        QToolButton *btn = new QToolButton();
        btn->setIcon(QIcon(icons[i]));
        btn->setIconSize(QSize(70, 70)); // 图片大小
        btn->setCheckable(true);
        btn->setFixedSize(80, 80);       // 白框大小，图片会自动居中
        btn->setStyleSheet("QToolButton { border: 1px solid #ccc; border-radius: 6px; background: white; }"
                           "QToolButton:checked { border: 2px solid #2196F3; background: #e3f2fd; }");

        // 独立的外部文字标签
        QLabel *lbl = new QLabel(types[i]);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #333; font-size: 13px;");

        itemLayout->addWidget(btn, 0, Qt::AlignHCenter);
        itemLayout->addWidget(lbl, 0, Qt::AlignHCenter);

        btnGroup->addButton(btn, i);
        gridSelector->addLayout(itemLayout, i/2, i%2);

        connect(btn, &QToolButton::clicked, [this, i]() {
            m_stackedWidget->setCurrentIndex(i);
            m_currentType = static_cast<PosBlockType>(i);
            m_detailContainer->setVisible(true);
        });
    }
    QHBoxLayout *gridWrapper = new QHBoxLayout();
    gridWrapper->addStretch();
    gridWrapper->addLayout(gridSelector);
    gridWrapper->addStretch();
    rightLayout->addLayout(gridWrapper);

    m_detailContainer = new QWidget();
    QVBoxLayout *detailVBox = new QVBoxLayout(m_detailContainer);
    detailVBox->setContentsMargins(0, 0, 0, 0);

    QLabel *detailTitle = new QLabel("详细设置");
    detailTitle->setStyleSheet("color: #666; font-size: 12px; font-weight: bold;");
    detailVBox->addWidget(detailTitle);

    QFrame *detailFrame = new QFrame();
    detailFrame->setFrameShape(QFrame::StyledPanel);
    detailFrame->setStyleSheet("QFrame { background-color: #fafafa; border: 1px solid #ddd; border-radius: 4px; }");

    QVBoxLayout *innerDetailLayout = new QVBoxLayout(detailFrame);

    m_stackedWidget = new QStackedWidget();

    QWidget *pageLine = new QWidget(); QFormLayout *fl0 = new QFormLayout(pageLine);
    fl0->addRow("长度:", m_lineLen = createSpinBox(0, 10000, 50));
    fl0->addRow("宽度:", m_lineWidth = createSpinBox(0, 10000, 10));
    fl0->addRow("X 坐标:", m_lineX = createSpinBox());
    fl0->addRow("Y 坐标:", m_lineY = createSpinBox());
    fl0->addRow("旋转角度:", m_lineAngle = createSpinBox(-360, 360));
    m_stackedWidget->addWidget(pageLine);

    QWidget *pagePt = new QWidget(); QFormLayout *fl1 = new QFormLayout(pagePt);
    fl1->addRow("X 坐标:", m_ptX = createSpinBox());
    fl1->addRow("Y 坐标:", m_ptY = createSpinBox());
    fl1->addRow("旋转角度:", m_ptAngle = createSpinBox(-360, 360));
    m_stackedWidget->addWidget(pagePt);

    QWidget *pageArc = new QWidget(); QFormLayout *fl2 = new QFormLayout(pageArc);
    fl2->addRow("圆心 X:", m_arcX = createSpinBox());
    fl2->addRow("圆心 Y:", m_arcY = createSpinBox());
    fl2->addRow("圆弧半径:", m_arcR = createSpinBox(0, 10000, 20));
    fl2->addRow("旋转角度:", m_arcAngle = createSpinBox(-360, 360));
    m_stackedWidget->addWidget(pageArc);

    QWidget *pageCir = new QWidget(); QFormLayout *fl3 = new QFormLayout(pageCir);
    fl3->addRow("圆心 X:", m_cirX = createSpinBox());
    fl3->addRow("圆心 Y:", m_cirY = createSpinBox());
    fl3->addRow("半径:", m_cirR = createSpinBox(0, 10000, 20));
    m_stackedWidget->addWidget(pageCir);

    innerDetailLayout->addWidget(m_stackedWidget);

    QHBoxLayout *addBtnLayout = new QHBoxLayout();
    addBtnLayout->addStretch();
    QPushButton *btnAdd = new QPushButton("确认添加");
    btnAdd->setMinimumHeight(20);
    btnAdd->setMinimumWidth(80);
    btnAdd->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; border-radius: 4px;");
    connect(btnAdd, &QPushButton::clicked, this, &PositioningDialog::onAddClicked);
    addBtnLayout->addWidget(btnAdd);

    // 加入到详细面板的布局中
    innerDetailLayout->addLayout(addBtnLayout);

    detailVBox->addWidget(detailFrame);
    m_detailContainer->setVisible(false);

    rightLayout->addWidget(m_detailContainer);
    rightLayout->addStretch();

    QPushButton *btnConfirm = new QPushButton("完成并映射到主图纸");
    btnConfirm->setMinimumHeight(35);
    btnConfirm->setStyleSheet("background-color: #4CAF50; color: white; font-size: 14px; font-weight: bold; border-radius: 4px;");
    connect(btnConfirm, &QPushButton::clicked, this, &QDialog::accept);
    rightLayout->addWidget(btnConfirm);

    mainLayout->addWidget(rightFrame, 1);
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
        b.x = m_ptX->value(); b.y = m_ptY->value();b.angle = m_ptAngle->value();
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

void PreviewArea::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space) {
        autoFit();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}
