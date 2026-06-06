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
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>

// ========================================================
// 1. 数据模型与轮廓生成 (保持不变)
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
// 2. 左侧预览绘图区 (强化：特征点吸附与右键锚点物理计算)
// ========================================================
PreviewArea::PreviewArea(QWidget *parent) : QWidget(parent) {
    setMinimumWidth(400);
    setStyleSheet("background-color: white; border: 1px solid #ccc;");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// 👇 提取几何特征参考点算法
QList<QPointF> PreviewArea::getReferencePoints(const PositioningBlock& b) const {
    QList<QPointF> pts;
    QTransform t;
    t.translate(b.x, b.y);
    t.rotate(b.angle);

    if (b.type == PosBlockType::Line) {
        pts.append(QPointF(b.x, b.y)); // 矩形中心
        // 矩形四个边角端点
        pts.append(t.map(QPointF(-b.width/2.0, -b.length/2.0)));
        pts.append(t.map(QPointF(b.width/2.0, -b.length/2.0)));
        pts.append(t.map(QPointF(b.width/2.0, b.length/2.0)));
        pts.append(t.map(QPointF(-b.width/2.0, b.length/2.0)));
    } else {
        pts.append(QPointF(b.x, b.y)); // 圆心/点基准
    }
    return pts;
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
    // 自动选中新添加的块的中心特征点
    m_selectedBlockIdx = m_blocks.size() - 1;
    m_selectedPtIdx = 0;
    m_selectedPos = getReferencePoints(block).first();
    emit refPointSelected(m_selectedBlockIdx, m_selectedPtIdx, m_selectedPos);
    autoFit();
}

void PreviewArea::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_blocks.isEmpty()) return;

    if (m_firstPaint) { autoFit(); m_firstPaint = false; }

    painter.translate(width() / 2.0 + m_panOffset.x(), height() / 2.0 + m_panOffset.y());
    painter.scale(m_scaleFactor, -m_scaleFactor);
    m_transform = painter.transform();

    for (int i = 0; i < m_blocks.size(); ++i) {
        QPainterPath p = m_blocks[i].getPath();
        if (i == m_selectedBlockIdx) {
            painter.setPen(QPen(Qt::red, 2.0 / m_scaleFactor));
            painter.setBrush(QColor(255, 200, 150, 220)); // 选中时浅橙色加深
        } else {
            painter.setPen(QPen(QColor(255, 165, 0), 2.0 / m_scaleFactor));
            painter.setBrush(QColor(255, 200, 150, 150)); // 浅橙色
        }
        painter.drawPath(p);

        painter.save();
        painter.translate(m_blocks[i].x, m_blocks[i].y);
        painter.rotate(m_blocks[i].angle);
        painter.scale(1.0, -1.0);
        double fontSize = 10.0;
        bool rotateVertical = false;
        if (m_blocks[i].type == PosBlockType::Line) {
            fontSize = std::min(m_blocks[i].width, m_blocks[i].length) * 0.3;
            if (m_blocks[i].length > m_blocks[i].width) rotateVertical = true;
        } else if (m_blocks[i].type == PosBlockType::Circle || m_blocks[i].type == PosBlockType::Arc) {
            fontSize = m_blocks[i].radius * 0.25;
        }
        if (fontSize < 1.0) fontSize = 1.0;
        QFont f = painter.font();
        f.setPointSizeF(fontSize);
        painter.setFont(f);
        painter.setPen(Qt::black);
        if (rotateVertical) painter.rotate(90.0);
        QFontMetricsF fm(f);
        double tw = fm.horizontalAdvance(m_blocks[i].name);
        double th = fm.height();
        painter.drawText(QRectF(-tw/2.0, -th/2.0, tw, th), Qt::AlignCenter, m_blocks[i].name);
        painter.restore();

        // 绘制特征圆点
        auto pts = getReferencePoints(m_blocks[i]);
        for (int j = 0; j < pts.size(); ++j) {
            if (i == m_selectedBlockIdx && j == m_selectedPtIdx) {
                painter.setBrush(Qt::red); painter.setPen(Qt::NoPen);
                painter.drawEllipse(pts[j], 6.0/m_scaleFactor, 6.0/m_scaleFactor); // 选中变红
            } else if (i == m_hoveredBlockIdx && j == m_hoveredPtIdx) {
                painter.setBrush(Qt::cyan); painter.setPen(Qt::NoPen);
                painter.drawEllipse(pts[j], 6.0/m_scaleFactor, 6.0/m_scaleFactor); // 靠近变青
            }
        }
    }
}

void PreviewArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        m_isPanning = true;
        m_panOffset += event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else {
        // 👇 特征点悬浮捕捉逻辑
        QPointF dxfPos = m_transform.inverted().map(QPointF(event->pos()));
        m_hoveredBlockIdx = -1;
        m_hoveredPtIdx = -1;
        double minDist = 15.0 / m_scaleFactor;

        for (int i = 0; i < m_blocks.size(); ++i) {
            auto pts = getReferencePoints(m_blocks[i]);
            for (int j = 0; j < pts.size(); ++j) {
                double d = std::hypot(pts[j].x() - dxfPos.x(), pts[j].y() - dxfPos.y());
                if (d < minDist) {
                    minDist = d;
                    m_hoveredBlockIdx = i;
                    m_hoveredPtIdx = j;
                    m_hoveredPos = pts[j];
                }
            }
        }
    }
    update();
}

void PreviewArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
        m_isPanning = false;

        if (m_hoveredBlockIdx != -1) {
            m_selectedBlockIdx = m_hoveredBlockIdx;
            m_selectedPtIdx = m_hoveredPtIdx;
            m_selectedPos = m_hoveredPos;
            emit refPointSelected(m_selectedBlockIdx, m_selectedPtIdx, m_selectedPos); // 发送信号给面板
        } else {
            m_selectedBlockIdx = -1;
            m_selectedPtIdx = -1;
            emit backgroundClicked();
        }
        update();
    }
}

void PreviewArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_isPanning) {
        setCursor(Qt::ArrowCursor);
        m_isPanning = false;
    }
}

void PreviewArea::wheelEvent(QWheelEvent *event) {
    double zoomFactor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    QPointF offset = event->position() - QPointF(width() / 2.0, height() / 2.0) - m_panOffset;
    m_panOffset -= offset * (zoomFactor - 1.0);
    m_scaleFactor *= zoomFactor;
    if (m_scaleFactor < 0.1) m_scaleFactor = 0.1;
    if (m_scaleFactor > 50.0) m_scaleFactor = 50.0;
    update();
}

void PreviewArea::contextMenuEvent(QContextMenuEvent *event) {
    if (m_selectedBlockIdx < 0) {
        return;
    }

    QMenu menu(this);
    QAction *actDel = menu.addAction("删除此定位块");
    QAction *actMove = menu.addAction("移动 (基准:所选参考点)");
    QAction *actRot = menu.addAction("旋转 (绕锚点:所选参考点)");

    QAction *res = menu.exec(event->globalPos());
    if (res == actDel) {
        m_blocks.removeAt(m_selectedBlockIdx);
        m_selectedBlockIdx = -1; m_selectedPtIdx = -1;
        emit backgroundClicked();
    } else if (res == actMove) {
        QDialog dlg(this);
        dlg.setWindowTitle("移动定位块");
        dlg.setWindowModality(Qt::NonModal);

        QFormLayout layout(&dlg);
        QDoubleSpinBox sbX(&dlg);
        sbX.setRange(-10000, 10000); sbX.setDecimals(2); sbX.setValue(m_selectedPos.x());
        QDoubleSpinBox sbY(&dlg);
        sbY.setRange(-10000, 10000); sbY.setDecimals(2); sbY.setValue(m_selectedPos.y());

        layout.addRow("新 X 坐标:", &sbX);
        layout.addRow("新 Y 坐标:", &sbY);

        QDialogButtonBox btnBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        layout.addWidget(&btnBox);
        connect(&btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(&btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            double newX = sbX.value();
            double newY = sbY.value();
            double dx = newX - m_selectedPos.x();
            double dy = newY - m_selectedPos.y();
            m_blocks[m_selectedBlockIdx].x += dx;
            m_blocks[m_selectedBlockIdx].y += dy;
            m_selectedPos = QPointF(newX, newY);
            emit refPointSelected(m_selectedBlockIdx, m_selectedPtIdx, m_selectedPos); // 同步刷新右侧信息面板
        }
    } else if (res == actRot) {
        QDialog dlg(this);
        dlg.setWindowTitle("旋转定位块");
        dlg.setWindowModality(Qt::NonModal);

        QFormLayout layout(&dlg);
        QDoubleSpinBox sbAngle(&dlg);
        sbAngle.setRange(-360, 360); sbAngle.setDecimals(2); sbAngle.setValue(0);
        layout.addRow("绕【当前参考点】增加角度:", &sbAngle);

        QDialogButtonBox btnBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        layout.addWidget(&btnBox);
        connect(&btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(&btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            double angle = sbAngle.value();
            if (angle != 0) {
                double rad = angle * M_PI / 180.0;
                double cosA = std::cos(rad);
                double sinA = std::sin(rad);
                double dx = m_blocks[m_selectedBlockIdx].x - m_selectedPos.x();
                double dy = m_blocks[m_selectedBlockIdx].y - m_selectedPos.y();
                m_blocks[m_selectedBlockIdx].x = m_selectedPos.x() + (dx * cosA - dy * sinA);
                m_blocks[m_selectedBlockIdx].y = m_selectedPos.y() + (dx * sinA + dy * cosA);
                m_blocks[m_selectedBlockIdx].angle += angle;
            }
        }
    }
    update();
}

void PreviewArea::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space) { autoFit(); event->accept(); }
    else { QWidget::keyPressEvent(event); }
}

void PreviewArea::setInitialBlocks(const QList<PositioningBlock>& blocks) {
    m_blocks = blocks;
    if (!m_blocks.isEmpty()) {
        m_selectedBlockIdx = m_blocks.size() - 1;
        m_selectedPtIdx = 0;
        m_selectedPos = getReferencePoints(m_blocks.last()).first();
        emit refPointSelected(m_selectedBlockIdx, m_selectedPtIdx, m_selectedPos);
        autoFit();
    }
    update();
}

// ========================================================
// 3. 浮动主窗口 UI 构建 (加入动态面板切换)
// ========================================================
QDoubleSpinBox* PositioningDialog::createSpinBox(double min, double max, double val) {
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(min, max); sb->setValue(val); sb->setDecimals(2);
    return sb;
}

PositioningDialog::PositioningDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("建立定位");
    setMinimumSize(900, 600);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
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

    QLabel *typeTitle = new QLabel("选择添加定位块类型");
    typeTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    rightLayout->addWidget(typeTitle);

    QGridLayout *gridSelector = new QGridLayout();
    gridSelector->setVerticalSpacing(15);
    gridSelector->setHorizontalSpacing(60);
    QStringList types = {"直线定位", "点定位", "圆弧定位", "圆定位"};
    QStringList icons = {":/img/images/line_pos.png", ":/img/images/point_pos.png", ":/img/images/arc_pos.png", ":/img/images/circle_pos.png"};

    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->setExclusive(true);

    for (int i = 0; i < 4; ++i) {
        QVBoxLayout *itemLayout = new QVBoxLayout();
        itemLayout->setSpacing(8);
        itemLayout->setAlignment(Qt::AlignCenter);

        QToolButton *btn = new QToolButton();
        btn->setIcon(QIcon(icons[i]));
        btn->setIconSize(QSize(70, 70));
        btn->setCheckable(true);
        btn->setFixedSize(80, 80);
        btn->setStyleSheet("QToolButton { border: 1px solid #ccc; border-radius: 6px; background: white; }"
                           "QToolButton:checked { border: 2px solid #2196F3; background: #e3f2fd; }");

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
            m_infoContainer->setVisible(false);
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
    btnAdd->setMinimumHeight(20); btnAdd->setMinimumWidth(80);
    btnAdd->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; border-radius: 4px;");
    connect(btnAdd, &QPushButton::clicked, this, &PositioningDialog::onAddClicked);
    addBtnLayout->addWidget(btnAdd);
    innerDetailLayout->addLayout(addBtnLayout);
    detailVBox->addWidget(detailFrame);
    m_detailContainer->setVisible(false);

    m_infoContainer = new QWidget();
    QVBoxLayout *infoVBox = new QVBoxLayout(m_infoContainer);
    infoVBox->setContentsMargins(0, 0, 0, 0);
    QLabel *infoTitle = new QLabel("参考点信息反馈");
    infoTitle->setStyleSheet("color: #666; font-size: 12px; font-weight: bold;");
    infoVBox->addWidget(infoTitle);

    QFrame *infoFrame = new QFrame();
    infoFrame->setStyleSheet("QFrame { background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 4px; }");
    QVBoxLayout *innerInfoLayout = new QVBoxLayout(infoFrame);
    m_infoLabel = new QLabel("...");
    m_infoLabel->setStyleSheet("font-size: 14px; color: #333; line-height: 1.5;");
    innerInfoLayout->addWidget(m_infoLabel);

    infoVBox->addWidget(infoFrame);
    infoVBox->addStretch();
    m_infoContainer->setVisible(false);

    // 将两个面板放入右侧布局，通过 Visible 控制轮流显示
    rightLayout->addWidget(m_detailContainer);
    rightLayout->addWidget(m_infoContainer);
    rightLayout->addStretch();

    connect(m_previewArea, &PreviewArea::refPointSelected, this, [this, btnGroup](int bIdx, int ptIdx, QPointF pos) {
        if (btnGroup->checkedButton()) {
            btnGroup->setExclusive(false);
            btnGroup->checkedButton()->setChecked(false); // 取消四个添加按钮的高亮
            btnGroup->setExclusive(true);
        }

        m_detailContainer->setVisible(false); // 隐藏添加面板
        m_infoContainer->setVisible(true);    // 呼出信息反馈面板

        PositioningBlock b = m_previewArea->getBlocks()[bIdx];
        QString text = QString("<b>块类型：</b> %1<br><br>").arg(
            b.type == PosBlockType::Line ? "直线定位 (挡板)" :
                b.type == PosBlockType::Point ? "点定位 (销钉)" :
                b.type == PosBlockType::Arc ? "圆弧定位 (槽)" : "圆定位 (法兰)"
            );

        QString ptTypeStr = "圆心基准";
        if (b.type == PosBlockType::Line) {
            if (ptIdx == 0) ptTypeStr = "矩形中心";
            else if (ptIdx == 1) ptTypeStr = "左上角点";
            else if (ptIdx == 2) ptTypeStr = "右上角点";
            else if (ptIdx == 3) ptTypeStr = "右下角点";
            else if (ptIdx == 4) ptTypeStr = "左下角点";
        }

        text += QString("<b>所选参考特征：</b> [%1]<br><b>全局 X:</b> &nbsp;%2<br><b>全局 Y:</b> &nbsp;%3<br>").arg(ptTypeStr).arg(pos.x(), 0, 'f', 2).arg(pos.y(), 0, 'f', 2);

        if (b.type == PosBlockType::Arc || b.type == PosBlockType::Circle) {
            text += QString("<br><b>约束半径：</b> %1").arg(b.radius, 0, 'f', 2);
        }
        m_infoLabel->setText(text);
    });

    connect(m_previewArea, &PreviewArea::backgroundClicked, this, [this]() {
        m_infoContainer->setVisible(false); // 点空白处时隐藏
    });

    QPushButton *btnConfirm = new QPushButton("确认定位配置");
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

    QString typeName;
    if (m_currentType == PosBlockType::Line) typeName = "直线定位块";
    else if (m_currentType == PosBlockType::Point) typeName = "点定位块";
    else if (m_currentType == PosBlockType::Arc) typeName = "圆弧定位块";
    else if (m_currentType == PosBlockType::Circle) typeName = "圆定位块";

    int count = 1;
    for (const auto& block : m_previewArea->getBlocks()) {
        if (block.type == m_currentType) count++;
    }
    b.name = QString("%1%2").arg(typeName).arg(count);

    m_previewArea->addBlock(b);
}

void PositioningDialog::setInitialBlocks(const QList<PositioningBlock>& blocks) {
    m_previewArea->setInitialBlocks(blocks);
}
