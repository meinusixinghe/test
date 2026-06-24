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
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>

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
    setMinimumWidth(300);
    setStyleSheet("background-color: white; border: 1px solid #ccc;");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// 提取几何特征参考点算法
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
        } else if (m_blocks[i].type == PosBlockType::Circle) {
            fontSize = m_blocks[i].radius * 0.25;
        } else if (m_blocks[i].type == PosBlockType::Arc) {
            fontSize = m_blocks[i].radius * 0.25;
        } else if (m_blocks[i].type == PosBlockType::Point) {
            fontSize = m_blocks[i].radius * 0.25;
        }
        double minVisualSize = 10.0 / m_scaleFactor;
        double maxVisualSize = 25.0 / m_scaleFactor;
        if (fontSize < minVisualSize) fontSize = minVisualSize;
        if (fontSize > maxVisualSize) fontSize = maxVisualSize;
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
        // 特征点悬浮捕捉逻辑
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
        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle("移动定位块");
        dlg->setWindowModality(Qt::NonModal);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        QFormLayout* layout = new QFormLayout(dlg);
        QDoubleSpinBox* sbX = new QDoubleSpinBox(dlg);
        sbX->setRange(-10000, 10000); sbX->setDecimals(2); sbX->setValue(m_selectedPos.x());
        QDoubleSpinBox* sbY = new QDoubleSpinBox(dlg);
        sbY->setRange(-10000, 10000); sbY->setDecimals(2); sbY->setValue(m_selectedPos.y());

        layout->addRow("新 X 坐标:", sbX);
        layout->addRow("新 Y 坐标:", sbY);

        QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        // 使用 Lambda 表达式处理确定逻辑
        connect(btnBox, &QDialogButtonBox::accepted, dlg, [this, dlg, sbX, sbY]() {
            double newX = sbX->value();
            double newY = sbY->value();
            double dx = newX - m_selectedPos.x();
            double dy = newY - m_selectedPos.y();
            m_blocks[m_selectedBlockIdx].x += dx;
            m_blocks[m_selectedBlockIdx].y += dy;
            m_selectedPos = QPointF(newX, newY);
            emit refPointSelected(m_selectedBlockIdx, m_selectedPtIdx, m_selectedPos);
            update();
            dlg->accept();
        });
        dlg->show();
    } else if (res == actRot) {
        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle("旋转定位块");
        dlg->setWindowModality(Qt::NonModal);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        QFormLayout* layout = new QFormLayout(dlg);
        QDoubleSpinBox* sbAngle = new QDoubleSpinBox(dlg);
        sbAngle->setRange(-360, 360); sbAngle->setDecimals(2); sbAngle->setValue(0);
        layout->addRow("绕【当前参考点】增加角度:", sbAngle);

        QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        connect(btnBox, &QDialogButtonBox::accepted, dlg, [this, dlg, sbAngle]() {
            double angle = sbAngle->value();
            if (angle != 0) {
                double rad = angle * M_PI / 180.0;
                double cosA = std::cos(rad);
                double sinA = std::sin(rad);
                double dx = m_blocks[m_selectedBlockIdx].x - m_selectedPos.x();
                double dy = m_blocks[m_selectedBlockIdx].y - m_selectedPos.y();
                m_blocks[m_selectedBlockIdx].x = m_selectedPos.x() + (dx * cosA - dy * sinA);
                m_blocks[m_selectedBlockIdx].y = m_selectedPos.y() + (dx * sinA + dy * cosA);
                m_blocks[m_selectedBlockIdx].angle += angle;
                update(); // 刷新画布
            }
            dlg->accept();
        });
        dlg->show();
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
    sb->setMinimumWidth(75);
    sb->setMaximumWidth(95);
    sb->setStyleSheet("font-size: 10px; min-height: 18px; padding: 1px;");
    return sb;
}

PositioningDialog::PositioningDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("建立定位");
    setMinimumSize(700, 450);

    loadTemplatesFromSettings();

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame *leftFrame = new QFrame(this);
    leftFrame->setFrameShape(QFrame::StyledPanel);
    leftFrame->setStyleSheet("QFrame { background-color: #f0f0f0; border-right: 2px solid #dcdcdc; }");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_previewArea = new PreviewArea(leftFrame);
    leftLayout->addWidget(m_previewArea);

    mainLayout->addWidget(leftFrame, 6);

    QFrame *rightFrame = new QFrame(this);

    QVBoxLayout *rightLayout = new QVBoxLayout(rightFrame);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    rightLayout->setSpacing(6);

    QGroupBox *templateGroup = new QGroupBox("定位模板管理");
    templateGroup->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #333; border: 1px solid #ccc; border-radius: 4px; margin-top: 6px; padding-top: 10px; }"
                                 "QGroupBox::title { subcontrol-origin: margin; left: 7px; top: -6px; }");
    QVBoxLayout *templateLayout = new QVBoxLayout(templateGroup);
    templateLayout->setContentsMargins(6, 4, 6, 6);
    templateLayout->setSpacing(4);

    QHBoxLayout *comboLayout = new QHBoxLayout();
    m_templateCombo = new QComboBox();
    m_templateCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_templateCombo->setMinimumHeight(22);
    m_templateCombo->setToolTip("双击修改模板名称");
    m_templateCombo->installEventFilter(this); // 拦截双击事件

    comboLayout->addWidget(new QLabel("选择模板:"));
    comboLayout->addWidget(m_templateCombo);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_btnSaveTemplate = new QPushButton("保存当前");
    m_btnDeleteTemplate = new QPushButton("删除模板");
    m_btnSaveTemplate->setCursor(Qt::PointingHandCursor);
    m_btnDeleteTemplate->setCursor(Qt::PointingHandCursor);
    m_btnSaveTemplate->setStyleSheet("background-color: #4CAF50; color: white; border-radius: 3px; padding: 4px 8px; font-size: 10px; font-weight: bold;");
    m_btnDeleteTemplate->setStyleSheet("background-color: #F44336; color: white; border-radius: 3px; padding: 4px 8px; font-size: 10px; font-weight: bold;");

    btnLayout->addWidget(m_btnSaveTemplate);
    btnLayout->addWidget(m_btnDeleteTemplate);

    templateLayout->addLayout(comboLayout);
    templateLayout->addLayout(btnLayout);

    rightLayout->addWidget(templateGroup); // 放置在右侧最上方

    // 模板联动与按钮逻辑
    reloadTemplateCombo();

    connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_isUpdatingCombo || index < 0) return;
        QString name = m_templateCombo->itemText(index);
        if (m_templates.contains(name)) {
            m_previewArea->setInitialBlocks(m_templates[name]);
        }
    });

    connect(m_btnSaveTemplate, &QPushButton::clicked, this, [this]() {
        QString currentName = m_templateCombo->currentText();
        if (currentName.isEmpty()) currentName = "新模板1";
        bool ok;
        QString name = QInputDialog::getText(this, "保存定位模板", "请输入模板名称:", QLineEdit::Normal, currentName, &ok);
        if (ok && !name.isEmpty()) {
            m_templates[name] = m_previewArea->getBlocks();
            saveTemplatesToSettings();
            reloadTemplateCombo();
            m_templateCombo->setCurrentText(name);
        }
    });

    connect(m_btnDeleteTemplate, &QPushButton::clicked, this, [this]() {
        QString currentName = m_templateCombo->currentText();
        if (currentName.isEmpty()) return;
        if (QMessageBox::question(this, "确认删除", QString("确定要删除定位模板 '%1' 吗？").arg(currentName)) == QMessageBox::Yes) {
            m_templates.remove(currentName);
            saveTemplatesToSettings();
            reloadTemplateCombo();
            if (m_templates.isEmpty()) m_previewArea->setInitialBlocks(QList<PositioningBlock>());
            else m_previewArea->setInitialBlocks(m_templates.first());
        }
    });

    QLabel *typeTitle = new QLabel("选择添加定位块类型");
    typeTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333;");
    rightLayout->addWidget(typeTitle);

    QGridLayout *gridSelector = new QGridLayout();
    gridSelector->setSpacing(5);
    gridSelector->setContentsMargins(0, 0, 0, 0);

    QStringList types = {"直线", "点", "圆弧", "圆"};
    QStringList icons = {":/img/images/line_pos.png", ":/img/images/point_pos.png", ":/img/images/arc_pos.png", ":/img/images/circle_pos.png"};

    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->setExclusive(true);

    for (int i = 0; i < 4; ++i) {
        QVBoxLayout *itemLayout = new QVBoxLayout();
        itemLayout->setSpacing(2);
        itemLayout->setAlignment(Qt::AlignCenter);

        QToolButton *btn = new QToolButton();
        btn->setIcon(QIcon(icons[i]));
        btn->setIconSize(QSize(45, 45));
        btn->setCheckable(true);
        btn->setFixedSize(55, 55);
        btn->setStyleSheet("QToolButton { border: 1px solid #ccc; border-radius: 6px; background: white; }"
                           "QToolButton:checked { border: 2px solid #2196F3; background: #e3f2fd; }");

        QLabel *lbl = new QLabel(types[i]);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #333; font-size: 11px;");

        itemLayout->addWidget(btn, 0, Qt::AlignHCenter);
        itemLayout->addWidget(lbl, 0, Qt::AlignHCenter);

        btnGroup->addButton(btn, i);
        gridSelector->addLayout(itemLayout, i / 2, i % 2);

        connect(btn, &QToolButton::clicked, [this, i]() {
            m_stackedWidget->setCurrentIndex(i);
            m_currentType = static_cast<PosBlockType>(i);
            m_infoContainer->setVisible(false);
            m_detailContainer->setVisible(true);
        });
    }
    rightLayout->addLayout(gridSelector);

    m_detailContainer = new QWidget();
    QVBoxLayout *detailVBox = new QVBoxLayout(m_detailContainer);
    detailVBox->setContentsMargins(0, 0, 0, 0);
    detailVBox->setSpacing(4);

    QLabel *detailTitle = new QLabel("详细设置");
    detailTitle->setStyleSheet("color: #666; font-size: 11px; font-weight: bold;");
    detailVBox->addWidget(detailTitle);

    QFrame *detailFrame = new QFrame();
    detailFrame->setStyleSheet("QFrame { background-color: #fafafa; border: 1px solid #ddd; border-radius: 4px; }");
    QVBoxLayout *innerDetailLayout = new QVBoxLayout(detailFrame);
    innerDetailLayout->setContentsMargins(6, 6, 6, 6);
    innerDetailLayout->setSpacing(4);

    m_stackedWidget = new QStackedWidget();

    auto makeLbl = [](const QString& txt) {
        QLabel* l = new QLabel(txt);
        l->setStyleSheet("font-size: 10px; color: #555;");
        return l;
    };

    // --- [直线] ---
    QWidget *pageLine = new QWidget();
    QVBoxLayout *vl0 = new QVBoxLayout(pageLine); vl0->setContentsMargins(0,0,0,0); vl0->setSpacing(2);
    QHBoxLayout *hl0_1 = new QHBoxLayout();
    hl0_1->addWidget(makeLbl("长:")); hl0_1->addWidget(m_lineLen = createSpinBox(0, 10000, 50));
    hl0_1->addWidget(makeLbl("宽:")); hl0_1->addWidget(m_lineWidth = createSpinBox(0, 10000, 10));
    QHBoxLayout *hl0_2 = new QHBoxLayout();
    hl0_2->addWidget(makeLbl("X:")); hl0_2->addWidget(m_lineX = createSpinBox());
    hl0_2->addWidget(makeLbl("Y:")); hl0_2->addWidget(m_lineY = createSpinBox());
    QHBoxLayout *hl0_3 = new QHBoxLayout();
    hl0_3->addWidget(makeLbl("角度:")); hl0_3->addWidget(m_lineAngle = createSpinBox(-360, 360)); hl0_3->addStretch();
    vl0->addLayout(hl0_1); vl0->addLayout(hl0_2); vl0->addLayout(hl0_3);
    m_stackedWidget->addWidget(pageLine);

    // --- [点] ---
    QWidget *pagePt = new QWidget();
    QVBoxLayout *vl1 = new QVBoxLayout(pagePt); vl1->setContentsMargins(0,0,0,0); vl1->setSpacing(2);
    QHBoxLayout *hl1_1 = new QHBoxLayout();
    hl1_1->addWidget(makeLbl("X:")); hl1_1->addWidget(m_ptX = createSpinBox());
    hl1_1->addWidget(makeLbl("Y:")); hl1_1->addWidget(m_ptY = createSpinBox());
    QHBoxLayout *hl1_2 = new QHBoxLayout();
    hl1_2->addWidget(makeLbl("角度:")); hl1_2->addWidget(m_ptAngle = createSpinBox(-360, 360)); hl1_2->addStretch();
    vl1->addLayout(hl1_1); vl1->addLayout(hl1_2);
    m_stackedWidget->addWidget(pagePt);

    // --- [圆弧] ---
    QWidget *pageArc = new QWidget();
    QVBoxLayout *vl2 = new QVBoxLayout(pageArc); vl2->setContentsMargins(0,0,0,0); vl2->setSpacing(2);
    QHBoxLayout *hl2_1 = new QHBoxLayout();
    hl2_1->addWidget(makeLbl("圆心X:")); hl2_1->addWidget(m_arcX = createSpinBox());
    hl2_1->addWidget(makeLbl("Y:")); hl2_1->addWidget(m_arcY = createSpinBox());
    QHBoxLayout *hl2_2 = new QHBoxLayout();
    hl2_2->addWidget(makeLbl("半径:")); hl2_2->addWidget(m_arcR = createSpinBox(0, 10000, 20));
    hl2_2->addWidget(makeLbl("角度:")); hl2_2->addWidget(m_arcAngle = createSpinBox(-360, 360));
    vl2->addLayout(hl2_1); vl2->addLayout(hl2_2);
    m_stackedWidget->addWidget(pageArc);

    // --- [圆] ---
    QWidget *pageCir = new QWidget();
    QVBoxLayout *vl3 = new QVBoxLayout(pageCir); vl3->setContentsMargins(0,0,0,0); vl3->setSpacing(2);
    QHBoxLayout *hl3_1 = new QHBoxLayout();
    hl3_1->addWidget(makeLbl("圆心X:")); hl3_1->addWidget(m_cirX = createSpinBox());
    hl3_1->addWidget(makeLbl("Y:")); hl3_1->addWidget(m_cirY = createSpinBox());
    QHBoxLayout *hl3_2 = new QHBoxLayout();
    hl3_2->addWidget(makeLbl("半径:")); hl3_2->addWidget(m_cirR = createSpinBox(0, 10000, 20)); hl3_2->addStretch();
    vl3->addLayout(hl3_1); vl3->addLayout(hl3_2);
    m_stackedWidget->addWidget(pageCir);

    innerDetailLayout->addWidget(m_stackedWidget);

    QHBoxLayout *addBtnLayout = new QHBoxLayout();
    addBtnLayout->addStretch();

    QPushButton *btnAdd = new QPushButton("确认添加");
    btnAdd->setFixedSize(65, 22);
    btnAdd->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; font-size: 10px; border-radius: 3px;");
    connect(btnAdd, &QPushButton::clicked, this, &PositioningDialog::onAddClicked);
    addBtnLayout->addWidget(btnAdd);
    innerDetailLayout->addLayout(addBtnLayout);
    detailVBox->addWidget(detailFrame);
    m_detailContainer->setVisible(false);

    m_infoContainer = new QWidget();
    QVBoxLayout *infoVBox = new QVBoxLayout(m_infoContainer);
    infoVBox->setContentsMargins(0, 0, 0, 0);
    infoVBox->setSpacing(4);
    QLabel *infoTitle = new QLabel("参考点信息");
    infoTitle->setStyleSheet("color: #666; font-size: 11px; font-weight: bold;");
    infoVBox->addWidget(infoTitle);

    QFrame *infoFrame = new QFrame();
    infoFrame->setStyleSheet("QFrame { background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 4px; }");
    QVBoxLayout *innerInfoLayout = new QVBoxLayout(infoFrame);
    innerInfoLayout->setContentsMargins(6, 6, 6, 6);

    m_infoLabel = new QLabel("...");
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet("font-size: 11px; color: #333;");
    innerInfoLayout->addWidget(m_infoLabel);

    infoVBox->addWidget(infoFrame);
    infoVBox->addStretch();
    m_infoContainer->setVisible(false);

    rightLayout->addWidget(m_detailContainer);
    rightLayout->addWidget(m_infoContainer);
    rightLayout->addStretch();

    // 交互逻辑保持不变
    connect(m_previewArea, &PreviewArea::refPointSelected, this, [this, btnGroup](int bIdx, int ptIdx, QPointF pos) {
        if (btnGroup->checkedButton()) {
            btnGroup->setExclusive(false);
            btnGroup->checkedButton()->setChecked(false);
            btnGroup->setExclusive(true);
        }
        m_detailContainer->setVisible(false);
        m_infoContainer->setVisible(true);

        PositioningBlock b = m_previewArea->getBlocks()[bIdx];
        QString text = QString("<b>类型：</b> %1<br><br>").arg(
            b.type == PosBlockType::Line ? "直线定位" :
                b.type == PosBlockType::Point ? "点定位" :
                b.type == PosBlockType::Arc ? "圆弧定位" : "圆定位"
            );

        QString ptTypeStr = "圆心";
        if (b.type == PosBlockType::Line) {
            if (ptIdx == 0) ptTypeStr = "中心";
            else if (ptIdx == 1) ptTypeStr = "左上";
            else if (ptIdx == 2) ptTypeStr = "右上";
            else if (ptIdx == 3) ptTypeStr = "右下";
            else if (ptIdx == 4) ptTypeStr = "左下";
        }
        text += QString("<b>特征：</b> [%1]<br><b>X:</b> &nbsp;%2<br><b>Y:</b> &nbsp;%3<br>").arg(ptTypeStr).arg(pos.x(), 0, 'f', 2).arg(pos.y(), 0, 'f', 2);
        if (b.type == PosBlockType::Arc || b.type == PosBlockType::Circle) {
            text += QString("<br><b>约束半径：</b> %1").arg(b.radius, 0, 'f', 2);
        }
        m_infoLabel->setText(text);
    });

    connect(m_previewArea, &PreviewArea::backgroundClicked, this, [this]() {
        m_infoContainer->setVisible(false);
    });

    QPushButton *btnConfirm = new QPushButton("确认配置");
    btnConfirm->setMinimumHeight(28);
    btnConfirm->setStyleSheet("background-color: #4CAF50; color: white; font-size: 11px; font-weight: bold; border-radius: 4px;");
    connect(btnConfirm, &QPushButton::clicked, this, &QDialog::accept);
    rightLayout->addWidget(btnConfirm);

    mainLayout->addWidget(rightFrame, 4);
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

bool PositioningDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_templateCombo && event->type() == QEvent::MouseButtonDblClick) {
        if (m_templateCombo->count() > 0) {
            QString currentName = m_templateCombo->currentText();
            bool ok;
            QString newName = QInputDialog::getText(this, "修改模板名称", "请输入新的模板名称:", QLineEdit::Normal, currentName, &ok);
            if (ok && !newName.isEmpty() && newName != currentName) {
                renameTemplate(currentName, newName);
            }
        }
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void PositioningDialog::renameTemplate(const QString& oldName, const QString& newName) {
    if (m_templates.contains(newName)) {
        QMessageBox::warning(this, "重名", "该模板名称已存在！");
        return;
    }
    QList<PositioningBlock> blocks = m_templates.take(oldName);
    m_templates[newName] = blocks;
    saveTemplatesToSettings();
    reloadTemplateCombo();
    m_templateCombo->setCurrentText(newName);
}

void PositioningDialog::reloadTemplateCombo() {
    m_isUpdatingCombo = true;
    QString current = m_templateCombo->currentText();
    m_templateCombo->clear();
    m_templateCombo->addItems(m_templates.keys());
    if (!current.isEmpty() && m_templates.contains(current)) {
        m_templateCombo->setCurrentText(current);
    }
    m_isUpdatingCombo = false;
}

void PositioningDialog::saveTemplatesToSettings() {
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    QJsonObject root;
    for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
        QJsonArray blocksArr;
        for (const auto& b : it.value()) {
            QJsonObject bObj;
            bObj["type"] = static_cast<int>(b.type);
            bObj["x"] = b.x; bObj["y"] = b.y;
            bObj["length"] = b.length; bObj["width"] = b.width;
            bObj["radius"] = b.radius; bObj["angle"] = b.angle;
            bObj["name"] = b.name;
            blocksArr.append(bObj);
        }
        root[it.key()] = blocksArr;
    }
    settings.setValue("PositioningTemplates", QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PositioningDialog::loadTemplatesFromSettings() {
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    QByteArray data = settings.value("PositioningTemplates").toByteArray();
    m_templates.clear();
    if (!data.isEmpty()) {
        QJsonObject root = QJsonDocument::fromJson(data).object();
        for (const QString& key : root.keys()) {
            QList<PositioningBlock> blocks;
            QJsonArray blocksArr = root[key].toArray();
            for (int i = 0; i < blocksArr.size(); ++i) {
                QJsonObject bObj = blocksArr[i].toObject();
                PositioningBlock b;
                b.type = static_cast<PosBlockType>(bObj["type"].toInt());
                b.x = bObj["x"].toDouble(); b.y = bObj["y"].toDouble();
                b.length = bObj["length"].toDouble(); b.width = bObj["width"].toDouble();
                b.radius = bObj["radius"].toDouble(); b.angle = bObj["angle"].toDouble();
                b.name = bObj["name"].toString();
                blocks.append(b);
            }
            m_templates[key] = blocks;
        }
    }
}
