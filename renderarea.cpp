#include "renderarea.h"
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <limits>
#include <algorithm>
#include <QPolygonF>
#include <QtMath>
#include <QPainterPath>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <QStyleOption>
#include <QStyle>
#include <QTimer>
#include <QMessageBox>
#include <QListWidget>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>

using std::min;
using std::max;

// ----------------------------------------------------
// 功能：构造函数，初始化控件属性
// 讲解：RenderArea是 QWidget的子类，作为独立的绘图区域
// 核心：初始化变换参数的默认值，设置控件的视觉和交互属性，为后续绘图和交互做准备
// ----------------------------------------------------
RenderArea::RenderArea(QWidget *parent): QWidget(parent),
    m_scaleFactor(1.0),                                 // 缩放因子（首次加载时自动计算，滚轮事件中修改）
    m_panOffsetDXF(0.0, 0.0)                           // 用户拖拽产生的平移量（DXF坐标单位）
{
    setBackgroundRole(QPalette::Base);                  // 设置背景色为控件默认的基础色
    setAutoFillBackground(true);                        // 自动填充背景，避免透明
    setFocusPolicy(Qt::StrongFocus);                    // 获得焦点，支持键盘事件（此处未用，预留）
    setCursor(Qt::OpenHandCursor);                      // 设置鼠标样式，提醒用户可以拖拽
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_eraserPixmap = QPixmap(":/img/images/eraser2.png");

    m_moveInputWidget = new QWidget(this);
    m_moveInputWidget->setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    m_moveInputWidget->setStyleSheet(
        "QWidget { background-color: rgba(250, 250, 250, 245); border: 2px solid #0078D7; border-radius: 6px; }"
        "QLineEdit { border: 1px solid #ccc; border-radius: 3px; padding: 2px; background-color: white; }"
        "QLineEdit:focus { border: 1px solid #0078D7; }"
        );

    m_moveInputWidget->setMinimumWidth(150);
    m_moveInputWidget->setMinimumHeight(30);

    QHBoxLayout *hLayout = new QHBoxLayout(m_moveInputWidget);
    hLayout->setContentsMargins(6, 4, 6, 4);
    hLayout->setSpacing(5);

    m_editMoveX = new QLineEdit();
    m_editMoveY = new QLineEdit();
    m_editMoveX->setFixedWidth(45);
    m_editMoveY->setFixedWidth(45);

    QDoubleValidator *validator = new QDoubleValidator(this);
    m_editMoveX->setValidator(validator);
    m_editMoveY->setValidator(validator);

    QLabel *lblX = new QLabel("X:");
    lblX->setStyleSheet("border: none; font-weight: bold; color: #333; font-size: 12px;");
    QLabel *lblY = new QLabel("Y:");
    lblY->setStyleSheet("border: none; font-weight: bold; color: #333; font-size: 12px;");

    hLayout->addWidget(lblX);
    hLayout->addWidget(m_editMoveX);
    hLayout->addWidget(lblY);
    hLayout->addWidget(m_editMoveY);

    m_moveInputWidget->hide();

    connect(m_editMoveX, &QLineEdit::returnPressed, this, &RenderArea::executeMove);
    connect(m_editMoveY, &QLineEdit::returnPressed, this, &RenderArea::executeMove);

    m_rotateInputWidget = new QWidget(this);
    m_rotateInputWidget->setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    m_rotateInputWidget->setStyleSheet(
        "QWidget { background-color: rgba(250, 250, 250, 245); border: 2px solid #0078D7; border-radius: 6px; }"
        "QLineEdit { border: 1px solid #ccc; border-radius: 3px; padding: 2px; background-color: white; }"
        "QLineEdit:focus { border: 1px solid #0078D7; }"
        );

    m_rotateInputWidget->setMinimumWidth(120);
    m_rotateInputWidget->setMinimumHeight(30);

    QHBoxLayout *hLayout2 = new QHBoxLayout(m_rotateInputWidget);
    hLayout2->setContentsMargins(6, 4, 6, 4);
    hLayout2->setSpacing(5);

    m_editRotateAngle = new QLineEdit();
    m_editRotateAngle->setFixedWidth(60);

    QDoubleValidator *validator2 = new QDoubleValidator(this);
    m_editRotateAngle->setValidator(validator2);

    QLabel *lblAngle = new QLabel("旋转角度(度):");
    lblAngle->setStyleSheet("border: none; font-weight: bold; color: #333; font-size: 12px;");

    hLayout2->addWidget(lblAngle);
    hLayout2->addWidget(m_editRotateAngle);

    m_rotateInputWidget->hide();

    connect(m_editRotateAngle, &QLineEdit::returnPressed, this, &RenderArea::executeRotate);

    setFocusPolicy(Qt::StrongFocus);
}

void RenderArea::setUCSSelectionMode(int mode) {
    m_ucsSelectMode = mode;
    if (mode != 0) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::OpenHandCursor);
        m_hasHoveredFeature = false;
    }
    update();
}

void RenderArea::setUCS(const UserCoordSystem& ucs) {
    m_ucs = ucs;
    update();
}

// ----------------------------------------------------
// 功能：数据管理，负责接收外部数据并触发重绘
// 讲解：setData，加载孔洞和轮廓数据
// 核心：接收外部数据后，重置所有变换状态（避免旧数据的缩放/平移影响新数据），并触发重绘
// ----------------------------------------------------
void RenderArea::setData(const QVector<Hole> &h,const Hole &mPH,const QPolygonF &platePoly, bool isRect, bool resetView)
{
    weldHoles = h;                                      // 管孔数据（不包含主管板圆）
    mainPlateHole = mPH;                                // 管板圆
    m_mainPlatePolygon = platePoly;                     // 存多边形
    m_isRectangular = isRect;                           // 存标志位
    if (resetView) {
        m_firstPaint = true;
    }
    update();
}

// ----------------------------------------------------
// 功能：数据管理，负责接收外部数据并触发重绘
// 讲解：setHighlightedIndex，设置高亮孔洞索引
// 核心：实现 “点击列表高亮绘图区对应孔洞 ”的核心逻辑，仅当索引变化时才重绘，优化性能
// ----------------------------------------------------
void RenderArea::setHighlightedIndex(int index) {
    if (m_highlightIndex != index) {
        m_highlightIndex = index;
        update();
    }
}

// ----------------------------------------------------
// 功能：Qt绘图的入口，负责重绘
// 讲解：paintEvent是 QWidget的虚函数，当控件需要重绘时（如 update()调用）自动触发
// ----------------------------------------------------
void RenderArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);                                         // 2D绘图类
    painter.setRenderHint(QPainter::Antialiasing);                  // 开启抗锯齿，让图形更平滑
    painter.setRenderHint(QPainter::TextAntialiasing);              // 文字抗锯齿，更清晰

    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
    if (m_firstPaint && width() > 10 && height() > 10) {
        resetView();
        m_firstPaint = false;
    }
    applyCurrentTransform(painter);

    painter.save();
    double safeScale = (m_scaleFactor > 0.001) ? m_scaleFactor : 1.0;
    for (int i = 0; i < m_posBlocks.size(); ++i) {
        const auto& block = m_posBlocks[i];
        QPainterPath path = block.getPath();

        if (i == m_highlightedBlockIndex) {
            painter.setPen(QPen(Qt::red, 4.0 / safeScale));
            painter.setBrush(QColor(255, 200, 150, 255));
        } else {
            painter.setPen(QPen(QColor(255, 165, 0), 2.0 / safeScale));
            painter.setBrush(QColor(255, 200, 150, 150));
        }

        painter.drawPath(path);

        painter.save();
        painter.translate(block.x, block.y);
        painter.rotate(block.angle);
        painter.scale(1.0, -1.0);
        double fontSize = 10.0;
        bool rotateVertical = false;
        if (block.type == PosBlockType::Line) {
            fontSize = std::min(block.width, block.length) * 0.3;
            if (block.length > block.width) {
                rotateVertical = true;
            }
        } else if (block.type == PosBlockType::Circle || block.type == PosBlockType::Arc) {
            fontSize = block.radius * 0.25;
        }
        if (fontSize < 1.0) fontSize = 1.0;
        if (fontSize > 50.0) fontSize = 50.0;
        QFont f = painter.font();
        f.setPointSizeF(fontSize);
        painter.setFont(f);
        painter.setPen(Qt::black);
        if (rotateVertical) {
            painter.rotate(90.0);
        }
        QFontMetricsF fm(f);
        double tw = fm.horizontalAdvance(block.name);
        double th = fm.height();
        painter.drawText(QRectF(-tw/2.0, -th/2.0, tw, th), Qt::AlignCenter, block.name);
        painter.restore();
    }
    painter.restore();
    painter.restore();

    for (int i = 0; i < m_displayPaths.size(); ++i) {
        const auto& contour = m_displayPaths[i];
        if (contour.points.size() > 1) {
            QPen pen;
            if (m_selectedPathIndices.contains(i)) {
                pen = QPen(Qt::red, 0);
                pen.setWidth(m_lineWidth);
            } else if (m_highlightPathIndices.contains(i)) {
                pen = QPen(Qt::yellow, 0);
                pen.setWidth(m_lineWidth+2);
            } else {
                pen = QPen(Qt::black, 0);
                pen.setWidth(m_lineWidth);
            }
            pen.setCosmetic(true);
            painter.setPen(pen);

            if (m_transformState == TS_DraggingToBlock && m_selectedPathIndices.contains(i)) {
                QPolygonF transformedPoly;
                for (const QPointF& pt : contour.points) {
                    transformedPoly << m_previewTransform.map(pt);
                }
                painter.drawPolyline(transformedPoly);
            } else {
                painter.drawPolyline(contour.points.data(), contour.points.size());
            }
        }
    }

    if ((m_transformState == TS_SelectShapeFeature || m_ucsSelectMode != 0) && m_hasHoveredFeature) {
        painter.save();
        QPen blinkPen((QTime::currentTime().msec() % 500 < 250) ? Qt::cyan : Qt::blue, 0);
        blinkPen.setWidth(m_lineWidth + 2);
        blinkPen.setCosmetic(true);
        painter.setPen(blinkPen);
        if (m_ucsSelectMode == 2 || (m_transformState == TS_SelectShapeFeature && m_alignTargetType == PosBlockType::Line)) {
            painter.drawLine(m_hoveredLine);
        } else {
            double safeScale = (m_scaleFactor > 0.001) ? m_scaleFactor : 1.0;
            double r = 10.0 / safeScale;
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(m_hoveredPoint, r, r);
        }
        painter.restore();
    }

    if (m_transformState == TS_DraggingToBlock && m_alignTargetType == PosBlockType::Line) {
        QPointF p1 = m_previewTransform.map(m_alignShapeLine.p1());
        QPointF p2 = m_previewTransform.map(m_alignShapeLine.p2());
        double len = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
        if (len > 1e-6) {
            double nx = (p2.x() - p1.x()) / len;
            double ny = (p2.y() - p1.y()) / len;
            // 沿线段两端各延长 5000 毫米作为虚拟辅助线
            QPointF extP1 = p1 - QPointF(nx, ny) * 5000.0;
            QPointF extP2 = p2 + QPointF(nx, ny) * 5000.0;

            painter.save();
            QPen extPen(Qt::magenta, 2, Qt::DashLine); // 品红色虚线
            extPen.setCosmetic(true);
            painter.setPen(extPen);
            painter.drawLine(extP1, extP2);

            // 将用户实际选中的那条边用加粗的品红色高亮！
            QPen solidPen(Qt::magenta, 4);
            solidPen.setCosmetic(true);
            painter.setPen(solidPen);
            painter.drawLine(p1, p2);
            painter.restore();
        }
    }

    if (m_ucsSelectMode != 0 || m_ucs.valid || m_ucs.originValid || m_ucs.xValid || m_ucs.yValid) {
        painter.save();
        QTransform transform = painter.transform();
        painter.setTransform(QTransform());
        if (!m_ucs.valid) {
            if (m_ucs.originValid) {
                QPoint screenOrigin = transform.map(m_ucs.origin).toPoint();
                painter.setBrush(Qt::blue);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(screenOrigin, 6, 6);
            }
            if (m_ucs.xValid) {
                QPoint p1 = transform.map(m_ucs.xLine.p1()).toPoint();
                QPoint p2 = transform.map(m_ucs.xLine.p2()).toPoint();
                QPen xPen(Qt::red, 2);
                painter.setPen(xPen);
                painter.drawLine(p1, p2);

                // 在所选线段的中心画方向箭头
                QPoint center = (p1 + p2) / 2;
                painter.save();
                painter.translate(center);
                // 屏幕坐标系的 Y 轴是向下的，所以需要对数学坐标的 Y 取反
                painter.rotate(std::atan2(-m_ucs.xAxis.y(), m_ucs.xAxis.x()) * 180.0 / M_PI);
                painter.setBrush(Qt::red);
                QPolygonF arrow; arrow << QPointF(0, 0) << QPointF(-12, 5) << QPointF(-12, -5);
                painter.drawPolygon(arrow);
                painter.restore();
            }
            if (m_ucs.yValid) {
                QPoint p1 = transform.map(m_ucs.yLine.p1()).toPoint();
                QPoint p2 = transform.map(m_ucs.yLine.p2()).toPoint();
                QPen yPen(Qt::green, 2);
                painter.setPen(yPen);
                painter.drawLine(p1, p2);

                // 在所选线段的中心画方向箭头
                QPoint center = (p1 + p2) / 2;
                painter.save();
                painter.translate(center);
                painter.rotate(std::atan2(-m_ucs.yAxis.y(), m_ucs.yAxis.x()) * 180.0 / M_PI);
                painter.setBrush(Qt::green);
                QPolygonF arrow; arrow << QPointF(0, 0) << QPointF(-12, 5) << QPointF(-12, -5);
                painter.drawPolygon(arrow);
                painter.restore();
            }
        }
        // 2. 最终坐标系建立完毕的显示
        else {
            QPoint screenOrigin = transform.map(m_ucs.origin).toPoint();
            double axisLen = 60.0; // 屏幕物理像素长度 (不随鼠标滚轮变化)

            // 画固定大小的原点
            painter.setBrush(Qt::blue);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(screenOrigin, 6, 6);

            QFont f = painter.font();
            f.setPointSize(12);
            f.setBold(true);
            painter.setFont(f);

            // 画 X 轴 (红色)
            if (m_ucs.xAxis.length() > 0.1) {
                QPointF xEndDxf = m_ucs.origin + QPointF(m_ucs.xAxis.x(), m_ucs.xAxis.y()) * (axisLen / m_scaleFactor);
                QPoint screenXEnd = transform.map(xEndDxf).toPoint();

                QPen xPen(Qt::red, 2);
                painter.setPen(xPen);
                painter.drawLine(screenOrigin, screenXEnd);

                painter.save();
                painter.translate(screenXEnd);
                painter.rotate(std::atan2(screenXEnd.y() - screenOrigin.y(), screenXEnd.x() - screenOrigin.x()) * 180.0 / M_PI);
                painter.setBrush(Qt::red);
                QPolygonF arrow; arrow << QPointF(0, 0) << QPointF(-12, 5) << QPointF(-12, -5);
                painter.drawPolygon(arrow);
                painter.restore();

                painter.setPen(Qt::red);
                painter.drawText(screenXEnd + QPoint(5, 5), "X");
            }

            // 画 Y 轴 (绿色)
            if (m_ucs.yAxis.length() > 0.1) {
                QPointF yEndDxf = m_ucs.origin + QPointF(m_ucs.yAxis.x(), m_ucs.yAxis.y()) * (axisLen / m_scaleFactor);
                QPoint screenYEnd = transform.map(yEndDxf).toPoint();

                QPen yPen(Qt::green, 2);
                painter.setPen(yPen);
                painter.drawLine(screenOrigin, screenYEnd);

                painter.save();
                painter.translate(screenYEnd);
                painter.rotate(std::atan2(screenYEnd.y() - screenOrigin.y(), screenYEnd.x() - screenOrigin.x()) * 180.0 / M_PI);
                painter.setBrush(Qt::green);
                QPolygonF arrow; arrow << QPointF(0, 0) << QPointF(-12, 5) << QPointF(-12, -5);
                painter.drawPolygon(arrow);
                painter.restore();

                painter.setPen(Qt::green);
                painter.drawText(screenYEnd + QPoint(5, 5), "Y");
            }
        }
        painter.restore();
    }

    if (m_isEraserMode && !m_isMiddlePanning) {
        QPoint localPos = mapFromGlobal(QCursor::pos());
        if (rect().contains(localPos) && childAt(localPos) == nullptr) {
            painter.save();
            painter.setTransform(QTransform());
            painter.drawPixmap(
                QRect(localPos.x() - m_eraserSize / 2,
                      localPos.y() - m_eraserSize / 2,
                      m_eraserSize, m_eraserSize),
                m_eraserPixmap
                );
            painter.restore();
        }
    }

    // 绘制套索矩形框
    if (m_isLassoDragging) {
        painter.save();
        painter.setTransform(QTransform()); // 使用屏幕像素坐标系
        QRect rect = QRect(m_lassoStartPos, m_lassoCurrentPos).normalized();
        painter.setPen(QPen(Qt::black, 1)); // 黑色细边框
        painter.setBrush(QColor(150, 150, 150, 80)); // 浅灰色，透明度 80
        painter.drawRect(rect);
        painter.restore();
    }

    if ((m_isMoveMode || m_isRotateMode || m_isMirrorMode) && (m_transformState == TS_BasePoint || m_transformState == TS_SecondPoint) && m_isSnapped) {
        painter.save();
        painter.setTransform(QTransform());
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::blue);
        painter.drawEllipse(m_snappedScreenPos, 6, 6);
        painter.restore();
    }

    if (m_isMirrorMode && m_transformState == TS_SecondPoint) {
        painter.save();
        painter.setTransform(QTransform());
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
        QPen axisPen(Qt::magenta, 1, Qt::DashLine);
        painter.setPen(axisPen);
        QPoint p1 = transform.map(m_mirrorAxisPoint1).toPoint();
        QPoint p2 = m_isSnapped ? m_snappedScreenPos : m_currentMousePos;
        painter.drawLine(p1, p2);
        painter.restore();
    }

    if (m_ucs.valid) {
        painter.save();
        // 确保使用无任何缩放/平移的纯净屏幕像素坐标系
        painter.setTransform(QTransform());

        // 构造要显示的信息文本
        QString ucsText = QString("UCS 状态: 激活\n原点坐标: ( %1 , %2 )")
                              .arg(m_ucs.origin.x(), 0, 'f', 2).arg(m_ucs.origin.y(), 0, 'f', 2);

        // 设置字体
        QFont hudFont = painter.font();
        hudFont.setPointSize(10);
        hudFont.setBold(true);
        hudFont.setFamily("Consolas");

        // 计算文字所占的宽高
        QFontMetrics fm(hudFont);
        int textWidth = 0;
        int textHeight = 0;
        QStringList lines = ucsText.split('\n');
        for (const QString& line : lines) {
            textWidth = std::max(textWidth, fm.horizontalAdvance(line));
            textHeight += fm.height() + 2;
        }

        // 定义面板的边距和右下角位置
        int padding = 12;
        int rectW = textWidth + padding * 2;
        int rectH = textHeight + padding * 2;
        int x = width() - rectW - 20;  // 距离右边缘 20 像素
        int y = height() - rectH - 20; // 距离下边缘 20 像素

        QRect bgRect(x, y, rectW, rectH);

        // 画一个半透明的黑色工业风背景板
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 30, 180)); // 180 意味着 70% 的不透明度
        painter.drawRoundedRect(bgRect, 6, 6);

        // 画出白色的文字
        painter.setPen(Qt::white);
        QRect textRect(x + padding, y + padding, textWidth, textHeight);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, ucsText);

        painter.restore();
    }

    if (m_highlightedConstraintIndex >= 0 && m_highlightedConstraintIndex < m_alignConstraints.size()) {
        auto& c = m_alignConstraints[m_highlightedConstraintIndex];
        painter.save();
        QTransform t;
        t.translate(width() / 2.0, height() / 2.0);
        t.scale(m_scaleFactor, -m_scaleFactor);
        t.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());

        painter.setTransform(QTransform()); // 回归屏幕坐标系画定宽粗线
        QPen hlPen(Qt::magenta, 4);
        painter.setPen(hlPen);
        painter.setBrush(Qt::NoBrush);

        if (c.type == PosBlockType::Line) {
            QPoint p1 = t.map(c.shapeLine.p1()).toPoint();
            QPoint p2 = t.map(c.shapeLine.p2()).toPoint();
            painter.drawLine(p1, p2);
        } else {
            QPoint p = t.map(c.shapePt).toPoint();
            painter.drawEllipse(p, 10, 10);
        }
        painter.restore();
    }
}

// ----------------------------------------------------
// 功能：鼠标滚轮缩放
// ----------------------------------------------------
void RenderArea::wheelEvent(QWheelEvent *event)
{
    const double scaleStep = 1.15;              // 缩放步长（滚轮每滚一格，缩放15%）
    const double MAX_SCALE = 4;                 // 最大缩放（防止缩太大）
    int degrees = event->angleDelta().y();      // 滚轮滚动的角度（正为向上，负为向下）

    // 计算新的缩放因子
    if (degrees > 0) {
        m_scaleFactor *= scaleStep;                           // 向上滚：放大
    }else if(degrees < 0) {m_scaleFactor /= scaleStep;}       // 向下滚：缩小

    // 限制缩放范围
    if(m_scaleFactor > MAX_SCALE)m_scaleFactor = MAX_SCALE;

    update();
    event->accept();                            // 标记事件已处理
}

// ----------------------------------------------------
// 功能：左键按下时，记录鼠标初始位置，改变鼠标样式为闭手
// ----------------------------------------------------
void RenderArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        m_lastMousePos = event->pos();
        m_isMiddlePanning = true;
        setCursor(Qt::ClosedHandCursor);
        update();
        event->accept();
        return;
    }

    if (m_ucsSelectMode != 0 && event->button() == Qt::LeftButton) {
        if (m_hasHoveredFeature) {
            if (m_ucsSelectMode == 1) {
                emit ucsPointSelected(m_hoveredPoint);
            } else if (m_ucsSelectMode == 2) {
                emit ucsLineSelected(m_hoveredLine);
            }
        }
        event->accept();
        return;
    }

    if ((m_isMoveMode || m_isRotateMode || m_isMirrorMode) && event->button() == Qt::LeftButton) {
        if (m_transformState == TS_Select) {
            // 使用原本的套索选择逻辑框选线条
            m_isLassoDragging = true;
            m_lassoStartPos = event->pos();
            m_lassoCurrentPos = event->pos();
            if (event->modifiers() & Qt::ControlModifier) {
                m_baseSelectedIndices = m_selectedPathIndices;
            } else {
                m_selectedPathIndices.clear();
                m_baseSelectedIndices.clear();
            }
            update();
            event->accept();
            return;
        } else if (m_transformState == TS_BasePoint) {
            if (!m_isSnapped) {
                QTransform transform;
                transform.translate(width() / 2.0, height() / 2.0);
                transform.scale(m_scaleFactor, -m_scaleFactor);
                transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
                m_snappedDxfPos = transform.inverted().map(QPointF(event->pos()));
                m_snappedScreenPos = event->pos();
            }

            if (m_isMirrorMode) {
                m_mirrorAxisPoint1 = m_snappedDxfPos;
                m_transformState = TS_SecondPoint;
                update();
                event->accept();
                return;
            }

            m_transformState = TS_Input;
            QWidget* activeInputWidget = m_isMoveMode ? m_moveInputWidget : m_rotateInputWidget;

            activeInputWidget->adjustSize();
            int widgetW = activeInputWidget->width();
            int widgetH = activeInputWidget->height();

            // 智能避让弹窗位置
            int targetX = m_snappedScreenPos.x() + 10;
            int targetY = m_snappedScreenPos.y() + 10;
            if (targetX + widgetW > width()) targetX = m_snappedScreenPos.x() - widgetW - 10;
            if (targetY + widgetH > height()) targetY = m_snappedScreenPos.y() - widgetH - 10;

            activeInputWidget->move(std::max(0, targetX), std::max(0, targetY));

            if (m_isMoveMode) {
                m_editMoveX->clear();
                m_editMoveY->clear();
                m_editMoveX->setFocus();
            } else {
                m_editRotateAngle->clear();
                m_editRotateAngle->setFocus();
            }
            activeInputWidget->show();
            update();
            event->accept();
            return;

        } else if (m_transformState == TS_SecondPoint && m_isMirrorMode) {
            // 镜像模式的第二点点击！
            if (!m_isSnapped) {
                QTransform transform;
                transform.translate(width() / 2.0, height() / 2.0);
                transform.scale(m_scaleFactor, -m_scaleFactor);
                transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
                m_snappedDxfPos = transform.inverted().map(QPointF(event->pos()));
            }

            QPointF p1 = m_mirrorAxisPoint1;
            QPointF p2 = m_snappedDxfPos;

            // 防呆：两点不能重合
            if (std::hypot(p2.x() - p1.x(), p2.y() - p1.y()) > 1e-5) {
                double dx = p2.x() - p1.x();
                double dy = p2.y() - p1.y();
                // 直线方程 Ax + By + C = 0 的参数
                double A = -dy;
                double B = dx;
                double C = dy * p1.x() - dx * p1.y();
                double denominator = A * A + B * B;

                for (int idx : std::as_const(m_selectedPathIndices)) {
                    if (idx >= 0 && idx < m_displayPaths.size()) {
                        for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                            double x0 = m_displayPaths[idx].points[i].x();
                            double y0 = m_displayPaths[idx].points[i].y();

                            double nx = x0 - 2 * A * (A * x0 + B * y0 + C) / denominator;
                            double ny = y0 - 2 * B * (A * x0 + B * y0 + C) / denominator;

                            m_displayPaths[idx].points[i].setX(nx);
                            m_displayPaths[idx].points[i].setY(ny);
                        }
                    }
                }

                // 传回主界面并记录撤销
                emit pathsMoved(m_displayPaths);

                // 重置状态
                m_transformState = TS_Select;
                m_selectedPathIndices.clear();
                m_baseSelectedIndices.clear();
            }
            update();
            event->accept();
            return;
        }else if (m_transformState == TS_SelectShapeFeature) {
            if (m_hasHoveredFeature) {
                m_alignShapeLine = m_hoveredLine;
                m_alignShapePoint = m_hoveredPoint;
                m_transformState = TS_DraggingToBlock;
                m_previewTransform.reset();
                update();
            }
            event->accept();
            return;
        } else if (m_transformState == TS_DraggingToBlock) {
            if (m_isSnappedToBlock && m_snappedBlockIndex >= 0) {
                applyAlignmentConstraint(m_posBlocks[m_snappedBlockIndex]);
            } else {
                for (int idx : std::as_const(m_selectedPathIndices)) {
                    for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                        m_displayPaths[idx].points[i] = m_previewTransform.map(m_displayPaths[idx].points[i]);
                    }
                }
                emit pathsMoved(m_displayPaths);
                m_transformState = TS_Select;
                m_selectedPathIndices.clear();
                m_baseSelectedIndices.clear();
                update();
            }
            event->accept();
            return;
        }
    }

    if (m_isEraserMode && event->button() == Qt::LeftButton) {
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
        QPointF dxfPos = transform.inverted().map(QPointF(event->pos()));
        emit itemDeleted(dxfPos);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
        // 加入 Mirror 判断，防止鼠标图标错乱
        if (!m_isEraserMode && !m_isRotateMode && !m_isMoveMode && !m_isMirrorMode) {
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
    }
}

// ----------------------------------------------------
// 功能：左键按住时，计算鼠标移动的像素差，转换为 DXF坐标的平移量，应用平移限制后更新平移量
// ----------------------------------------------------
void RenderArea::mouseMoveEvent(QMouseEvent *event) {
    m_currentMousePos = event->pos();
    if (event->buttons() & Qt::MiddleButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffsetDXF += QPointF(delta.x() / m_scaleFactor, -delta.y() / m_scaleFactor);
        m_lastMousePos = event->pos();
        update();
        event->accept();
        return;
    }

    if (m_ucsSelectMode != 0) {
        m_hasHoveredFeature = false;
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
        QPointF dxfPos = transform.inverted().map(QPointF(event->pos()));

        double minDist = 15.0 / m_scaleFactor;

        for (int idx = 0; idx < m_displayPaths.size(); ++idx) {
            const auto& contour = m_displayPaths[idx];

            if (m_ucsSelectMode == 2) { // 选线
                if (contour.type == "直线" || contour.type.contains("多段线") || contour.type.contains("矩形")) {
                    for (int i = 0; i < contour.points.size() - 1; ++i) {
                        QPointF p1 = contour.points[i];
                        QPointF p2 = contour.points[i+1];
                        double dist = distancePointToSegment(dxfPos, p1, p2);
                        if (dist < minDist) {
                            minDist = dist;
                            m_hasHoveredFeature = true;
                            m_hoveredLine = QLineF(p1, p2);
                            m_hoveredPoint = (p1 + p2) / 2.0;
                        }
                    }
                }
            } else if (m_ucsSelectMode == 1) { // 选点
                if (contour.type == "圆" || contour.type.contains("弧") || contour.type.contains("拟合") || contour.type.contains("样条")) {
                    QPointF center = getShapeCenter(contour);
                    double dist = std::hypot(center.x() - dxfPos.x(), center.y() - dxfPos.y());
                    if (dist < minDist) {
                        minDist = dist; m_hasHoveredFeature = true; m_hoveredPoint = center;
                    }
                } else {
                    for (const QPointF& pt : contour.points) {
                        double dist = std::hypot(pt.x() - dxfPos.x(), pt.y() - dxfPos.y());
                        if (dist < minDist) {
                            minDist = dist; m_hasHoveredFeature = true; m_hoveredPoint = pt;
                        }
                    }
                }
            }
        }
        update();
        event->accept();
        return;
    }

    if (m_isMoveMode && m_transformState == TS_SelectShapeFeature) {
        m_hasHoveredFeature = false;
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
        QPointF dxfPos = transform.inverted().map(QPointF(event->pos()));

        double minDist = 15.0 / m_scaleFactor;

        for (int idx : std::as_const(m_selectedPathIndices)) {
            if (idx < 0 || idx >= m_displayPaths.size()) continue;
            const auto& contour = m_displayPaths[idx];

            if (m_alignTargetType == PosBlockType::Line) {
                if (contour.type == "圆" || contour.type.contains("弧") || contour.type.contains("拟合") || contour.type.contains("样条")) {
                    continue;
                }
                // 直线对齐：只吸附图元里的直线段
                for (int i = 0; i < contour.points.size() - 1; ++i) {
                    QPointF p1 = contour.points[i];
                    QPointF p2 = contour.points[i+1];
                    double dx = p2.x() - p1.x();
                    double dy = p2.y() - p1.y();
                    double len = std::hypot(dx, dy);
                    if (len < 1e-6) continue;
                    double distToSeg = distancePointToSegment(dxfPos, p1, p2);
                    if (distToSeg < minDist) {
                        minDist = distToSeg;
                        m_hasHoveredFeature = true;
                        m_hoveredLine = QLineF(p1, p2);
                        // 将抓取点严格设定为鼠标在无限长直线上的垂直投影点，实现平滑抓取！
                        double t = ((dxfPos.x() - p1.x()) * dx + (dxfPos.y() - p1.y()) * dy) / (len * len);
                        m_hoveredPoint = QPointF(p1.x() + t * dx, p1.y() + t * dy);
                    }
                }
            } else if (m_alignTargetType == PosBlockType::Point || m_alignTargetType == PosBlockType::Arc || m_alignTargetType == PosBlockType::Circle) {
                // 点对齐/圆对齐：智能抛弃圆弧上的碎点
                if (contour.type == "圆" || contour.type.contains("弧") || contour.type.contains("拟合") || contour.type.contains("样条")) {
                    QPointF center = getShapeCenter(contour);
                    double dist = std::hypot(center.x() - dxfPos.x(), center.y() - dxfPos.y());
                    if (dist < minDist) {
                        minDist = dist; m_hasHoveredFeature = true; m_hoveredPoint = center;
                    }
                } else if (m_alignTargetType == PosBlockType::Point) {
                    // 折线只吸附端点
                    for (const QPointF& pt : contour.points) {
                        double dist = std::hypot(pt.x() - dxfPos.x(), pt.y() - dxfPos.y());
                        if (dist < minDist) {
                            minDist = dist; m_hasHoveredFeature = true; m_hoveredPoint = pt;
                        }
                    }
                }
            }
        }

        // 允许额外吸附：所有选中零件构成的物理包围盒中心
        if (m_alignTargetType == PosBlockType::Point) {
            double selMinX = std::numeric_limits<double>::max(); double selMaxX = std::numeric_limits<double>::lowest();
            double selMinY = std::numeric_limits<double>::max(); double selMaxY = std::numeric_limits<double>::lowest();
            bool hasSel = false;
            for (int idx : std::as_const(m_selectedPathIndices)) {
                if (idx < 0 || idx >= m_displayPaths.size()) continue;
                for (const QPointF& pt : m_displayPaths[idx].points) {
                    if (pt.x() < selMinX) selMinX = pt.x(); if (pt.x() > selMaxX) selMaxX = pt.x();
                    if (pt.y() < selMinY) selMinY = pt.y(); if (pt.y() > selMaxY) selMaxY = pt.y();
                }
                hasSel = true;
            }
            if (hasSel) {
                QPointF center((selMinX + selMaxX)/2.0, (selMinY + selMaxY)/2.0);
                double dist = std::hypot(center.x() - dxfPos.x(), center.y() - dxfPos.y());
                if (dist < minDist) {
                    minDist = dist; m_hasHoveredFeature = true; m_hoveredPoint = center;
                }
            }
        }
        update();
        event->accept();
        return;
    }

    else if (m_isMoveMode && m_transformState == TS_DraggingToBlock) {
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
        QPointF dxfPos = transform.inverted().map(QPointF(event->pos()));

        m_isSnappedToBlock = false;
        m_snappedBlockIndex = -1;
        double minBlockDist = 30.0 / m_scaleFactor;

        for (int i = 0; i < m_posBlocks.size(); ++i) {
            const auto& block = m_posBlocks[i];
            if (block.type == m_alignTargetType) {
                double dist = 0.0;
                if (block.type == PosBlockType::Line) {
                    QTransform blockT;
                    blockT.translate(block.x, block.y);
                    blockT.rotate(block.angle);
                    QPointF localPos = blockT.inverted().map(dxfPos);
                    dist = std::min(std::abs(localPos.x() - block.width / 2.0), std::abs(localPos.x() + block.width / 2.0));
                } else {
                    dist = std::hypot(block.x - dxfPos.x(), block.y - dxfPos.y());
                }

                if (dist < minBlockDist) {
                    minBlockDist = dist;
                    m_isSnappedToBlock = true;
                    m_snappedBlockIndex = i;
                }
            }
        }

        m_previewTransform.reset();

        QPointF targetWorldPos = dxfPos;
        if (m_isSnappedToBlock) {
            const auto& block = m_posBlocks[m_snappedBlockIndex];
            if (block.type == PosBlockType::Line) {
                QTransform blockT;
                blockT.translate(block.x, block.y);
                blockT.rotate(block.angle);
                QPointF localPos = blockT.inverted().map(dxfPos);
                double snapLx = (localPos.x() > 0) ? (block.width / 2.0) : (-block.width / 2.0);
                targetWorldPos = blockT.map(QPointF(snapLx, localPos.y()));
            } else {
                targetWorldPos = QPointF(block.x, block.y);
            }
        }

        if (m_alignConstraints.size() == 1) {
            auto c1 = m_alignConstraints[0];
            if (c1.type == PosBlockType::Line) {
                double rad = c1.block.angle * M_PI / 180.0;
                QPointF D(-std::sin(rad), std::cos(rad)); // 沿着第一条挡板滑动的方向向量

                if (m_isSnappedToBlock && m_posBlocks[m_snappedBlockIndex].type == PosBlockType::Line) {
                    const auto& block2 = m_posBlocks[m_snappedBlockIndex];
                    double rad2 = block2.angle * M_PI / 180.0;
                    QPointF N2(std::cos(rad2), std::sin(rad2)); // 目标挡板的法向量

                    QTransform blockT;
                    blockT.translate(block2.x, block2.y);
                    blockT.rotate(block2.angle);
                    QPointF localPos = blockT.inverted().map(dxfPos);
                    double snapLx = (localPos.x() > 0) ? (block2.width / 2.0) : (-block2.width / 2.0);
                    QPointF P2 = blockT.map(QPointF(snapLx, 0)); // 目标挡板边缘上的一点

                    // 计算两条直线的几何交界点，并滑动过去
                    double denom = D.x() * N2.x() + D.y() * N2.y();
                    if (std::abs(denom) > 1e-4) {
                        double t = ((P2.x() - m_alignShapePoint.x()) * N2.x() + (P2.y() - m_alignShapePoint.y()) * N2.y()) / denom;
                        m_previewTransform.translate(t * D.x(), t * D.y());
                    } else {
                        // 平行退化处理
                        QPointF T(targetWorldPos.x() - m_alignShapePoint.x(), targetWorldPos.y() - m_alignShapePoint.y());
                        double proj = T.x() * D.x() + T.y() * D.y();
                        m_previewTransform.translate(proj * D.x(), proj * D.y());
                    }
                } else {
                    QPointF T(targetWorldPos.x() - m_alignShapePoint.x(), targetWorldPos.y() - m_alignShapePoint.y());
                    double proj = T.x() * D.x() + T.y() * D.y();
                    m_previewTransform.translate(proj * D.x(), proj * D.y());
                }
            } else {
                double angleShape = std::atan2(m_alignShapePoint.y() - c1.block.y, m_alignShapePoint.x() - c1.block.x);
                double angleMouse = std::atan2(targetWorldPos.y() - c1.block.y, targetWorldPos.x() - c1.block.x);
                double angleDiff = (angleMouse - angleShape) * 180.0 / M_PI;
                m_previewTransform.translate(c1.block.x, c1.block.y);
                m_previewTransform.rotate(angleDiff);
                m_previewTransform.translate(-c1.block.x, -c1.block.y);
            }
        } else {
            // 没有约束时：自由平移和寻点
            if (m_isSnappedToBlock) {
                const auto& block = m_posBlocks[m_snappedBlockIndex];
                if (block.type == PosBlockType::Line) {
                    QTransform blockT;
                    blockT.translate(block.x, block.y);
                    blockT.rotate(block.angle);
                    QPointF localPos = blockT.inverted().map(dxfPos); // 将鼠标转入挡块内部坐标系

                    // 找到靠得最近的那条长边 (左边缘或右边缘)
                    double snapLx = (localPos.x() > 0) ? (block.width / 2.0) : (-block.width / 2.0);
                    double ly = localPos.y(); // 允许纵向滑动
                    QPointF targetWorldPos = blockT.map(QPointF(snapLx, ly));

                    // 处理角度贴合
                    double shapeAngle = std::atan2(m_alignShapeLine.dy(), m_alignShapeLine.dx()) * 180.0 / M_PI;
                    double targetAngle = block.angle + 90.0;
                    double angleDiff = targetAngle - shapeAngle;
                    while (angleDiff > 90.0) angleDiff -= 180.0;
                    while (angleDiff <= -90.0) angleDiff += 180.0;

                    m_previewTransform.translate(targetWorldPos.x(), targetWorldPos.y());
                    m_previewTransform.rotate(angleDiff);
                    m_previewTransform.translate(-m_alignShapePoint.x(), -m_alignShapePoint.y());
                } else {
                    // 圆/点定位：同心直接贴合
                    m_previewTransform.translate(block.x - m_alignShapePoint.x(), block.y - m_alignShapePoint.y());
                }
            } else {
                m_previewTransform.translate(dxfPos.x() - m_alignShapePoint.x(), dxfPos.y() - m_alignShapePoint.y());
            }
        }
        update();
        event->accept();
        return;
    }

    if (m_isMoveMode || m_isRotateMode || m_isMirrorMode) {
        if (m_transformState == TS_Select && m_isLassoDragging) {
            m_lassoCurrentPos = event->pos();
            updateLassoSelection();
            update();
            event->accept();
            return;
        } else if (m_transformState == TS_BasePoint || m_transformState == TS_SecondPoint) {
            findSnapPoint(event->pos());
            update();
            event->accept();
            return;
        }
    }
    if (m_isEraserMode) {
        setCursor(Qt::BlankCursor);
        update();
        return;
    }
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        double dx = delta.x() / m_scaleFactor;
        double dy = -delta.y() / m_scaleFactor;
        m_panOffsetDXF += QPointF(dx, dy);
        m_lastMousePos = event->pos();
        update();
        event->accept();
    }
}

// ----------------------------------------------------
// 功能：左键松开时，恢复鼠标样式为 “开手 ”
// ----------------------------------------------------
void RenderArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        m_isMiddlePanning = false;
        if (m_isRotateMode || m_isMoveMode || m_isMirrorMode) setCursor(Qt::CrossCursor); // 👈【修复】: 加入镜像判断
        else if (m_isEraserMode) setCursor(Qt::BlankCursor);
        else setCursor(Qt::OpenHandCursor);
        update();
        event->accept();
        return;
    }
    if (m_isLassoDragging && event->button() == Qt::LeftButton) {
        m_isLassoDragging = false;
        updateLassoSelection();
        update();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (!m_isEraserMode && !m_isRotateMode && !m_isMoveMode && !m_isMirrorMode) {
            setCursor(Qt::OpenHandCursor);
        }
        event->accept();
    }
}

void RenderArea::setDisplayPaths(const QVector<Contour>& paths) {
    m_displayPaths = paths;
}

void RenderArea::resetView()
{
    // 初始化 DXF边界
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    auto updateBounds = [&](const QPointF& p) {
        if (p.x() < minX) minX = p.x();
        if (p.x() > maxX) maxX = p.x();
        if (p.y() < minY) minY = p.y();
        if (p.y() > maxY) maxY = p.y();
    };

    for(const auto& h : std::as_const(weldHoles)) updateBounds(h.center);
    if (m_isRectangular) {
        for (const auto& p : std::as_const(m_mainPlatePolygon)) updateBounds(p);
    } else {
        if (mainPlateHole.radius > 0) {
            updateBounds(QPointF(mainPlateHole.center.x() - mainPlateHole.radius, mainPlateHole.center.y()));
            updateBounds(QPointF(mainPlateHole.center.x() + mainPlateHole.radius, mainPlateHole.center.y()));
            updateBounds(QPointF(mainPlateHole.center.x(), mainPlateHole.center.y() - mainPlateHole.radius));
            updateBounds(QPointF(mainPlateHole.center.x(), mainPlateHole.center.y() + mainPlateHole.radius));
        }
    }
    for (const auto& contour : std::as_const(m_displayPaths)) {
        for (const auto& p : contour.points) updateBounds(p);
    }

    if (minX > maxX) {
        m_scaleFactor = 1.0;
        m_panOffsetDXF = QPointF(0, 0);
        return;
    }

    m_dxfMinBound = QPointF(minX, minY);
    m_dxfMaxBound = QPointF(maxX, maxY);

    double contentW = std::max(maxX - minX, 1.0);
    double contentH = std::max(maxY - minY, 1.0);
    double padding = 20.0;

    if (width() > 10 && height() > 10) {
        double scaleX = (width() - 2 * padding) / contentW;
        double scaleY = (height() - 2 * padding) / contentH;
        m_scaleFactor = std::min(scaleX, scaleY);
    } else {
        m_scaleFactor = 1.0;
    }

    if (m_scaleFactor < 0.0001) {
        m_scaleFactor = 0.0001;
    }
    m_panOffsetDXF = QPointF(-(minX + contentW / 2.0), -(minY + contentH / 2.0));
}

// ----------------------------------------------------
// 坐标变换：应用居缩放和用户平移量
// ----------------------------------------------------
void RenderArea::applyCurrentTransform(QPainter &painter)
{
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(m_scaleFactor, -m_scaleFactor);
    painter.translate(m_panOffsetDXF);
}

void RenderArea::setHighlightedPathIndices(const QList<int> &indices) {
    m_highlightPathIndices = indices;
    update();
}

void RenderArea::setEraserMode(bool enabled) {
    m_isEraserMode = enabled;
    if (enabled) {
        setCursor(Qt::BlankCursor);
    } else {
        setCursor(Qt::OpenHandCursor);
    }
    update();
}

double RenderArea::distancePointToSegment(const QPointF& p, const QPointF& p1, const QPointF& p2) {
    double l2 = std::pow(p1.x() - p2.x(), 2) + std::pow(p1.y() - p2.y(), 2);
    if (l2 == 0.0) return std::hypot(p.x() - p1.x(), p.y() - p1.y());
    double t = std::max(0.0, std::min(1.0, ((p.x() - p1.x()) * (p2.x() - p1.x()) + (p.y() - p1.y()) * (p2.y() - p1.y())) / l2));
    QPointF projection(p1.x() + t * (p2.x() - p1.x()), p1.y() + t * (p2.y() - p1.y()));
    return std::hypot(p.x() - projection.x(), p.y() - projection.y());
}

void RenderArea::setEraserSize(int size) {
    m_eraserSize = size;
    if (m_isEraserMode) {
        update(); // 如果当前正开着橡皮擦，立即刷新方块大小
    }
}

void RenderArea::clearSelection() {
    m_selectedPathIndices.clear();
    m_baseSelectedIndices.clear();
    m_isLassoDragging = false;
    m_alignConstraints.clear();
    update();
}

void RenderArea::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space) {
        resetView();
        event->accept();
        return;
    }

    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && !m_selectedPathIndices.isEmpty()) {
        if (m_transformState != TS_Input) {
            emit bulkPathsDeleted(m_selectedPathIndices.values());
            clearSelection();
        }
    }
    else if (event->key() == Qt::Key_Escape) {
        if (m_ucsSelectMode != 0) {
            setUCSSelectionMode(0);
            update();
        } else if (m_isMoveMode || m_isRotateMode || m_isMirrorMode) {
            if (m_transformState == TS_Input || m_transformState == TS_SecondPoint) {
                if (m_isMoveMode) m_moveInputWidget->hide();
                if (m_isRotateMode) m_rotateInputWidget->hide();
                m_transformState = TS_BasePoint; // 退回到选基准点状态
            } else if (m_transformState == TS_BasePoint) {
                m_transformState = TS_Select;
                clearSelection();
            } else {
                emit cancelModesRequested();
            }
            update();
        } else {
            emit cancelModesRequested();
        }
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_isMoveMode && m_transformState == TS_SelectShapeFeature) {
            if (m_hasHoveredFeature) {
                m_alignShapeLine = m_hoveredLine;
                m_alignShapePoint = m_hoveredPoint;
                m_transformState = TS_DraggingToBlock;
                m_previewTransform.reset();
                update();
            }
        }
        else if (m_isMoveMode && m_transformState == TS_DraggingToBlock) {
            if (m_isSnappedToBlock && m_snappedBlockIndex >= 0) {
                applyAlignmentConstraint(m_posBlocks[m_snappedBlockIndex]);
            } else {
                for (int idx : std::as_const(m_selectedPathIndices)) {
                    for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                        m_displayPaths[idx].points[i] = m_previewTransform.map(m_displayPaths[idx].points[i]);
                    }
                }
                emit pathsMoved(m_displayPaths);
                m_transformState = TS_Select;
                m_selectedPathIndices.clear();
                m_baseSelectedIndices.clear();
                update();
            }
        }
        else if (m_isMoveMode && m_transformState == TS_Select && !m_selectedPathIndices.isEmpty()) {
            QMenu moveMenu(this);
            QAction* actFree = moveMenu.addAction("1. 自由移动 (输入坐标偏移)");
            QAction* actBlock = moveMenu.addAction("2. 使用定位块对齐");
            if (m_posBlocks.isEmpty()) actBlock->setEnabled(false);

            QAction* res = moveMenu.exec(QCursor::pos());
            if (res == actFree) {
                m_transformState = TS_BasePoint;
                update();
            } else if (res == actBlock) {
                QMenu blockMenu(this);
                QAction* actLine = blockMenu.addAction("1. 直线定位块对齐");
                QAction* actPoint = blockMenu.addAction("2. 点定位块对齐");
                QAction* actArc = blockMenu.addAction("3. 圆弧定位块对齐");
                QAction* actCircle = blockMenu.addAction("4. 圆定位块对齐");

                bool hasLine=false, hasPt=false, hasArc=false, hasCir=false;
                for(const auto& b: std::as_const(m_posBlocks)) {
                    if(b.type == PosBlockType::Line) hasLine = true;
                    if(b.type == PosBlockType::Point) hasPt = true;
                    if(b.type == PosBlockType::Arc) hasArc = true;
                    if(b.type == PosBlockType::Circle) hasCir = true;
                }
                actLine->setEnabled(hasLine); actPoint->setEnabled(hasPt);
                actArc->setEnabled(hasArc); actCircle->setEnabled(hasCir);

                QAction* res2 = blockMenu.exec(QCursor::pos());
                if (res2) {
                    m_transformState = TS_SelectShapeFeature;
                    setCursor(Qt::ArrowCursor);
                    if (res2 == actLine) m_alignTargetType = PosBlockType::Line;
                    else if (res2 == actPoint) m_alignTargetType = PosBlockType::Point;
                    else if (res2 == actArc) m_alignTargetType = PosBlockType::Arc;
                    else if (res2 == actCircle) m_alignTargetType = PosBlockType::Circle;
                    update();
                }
            }
        }
        else if ((m_isRotateMode || m_isMirrorMode) && m_transformState == TS_Select && !m_selectedPathIndices.isEmpty()) {
            m_transformState = TS_BasePoint;
            update();
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

void RenderArea::updateLassoSelection() {
    QRect selectionRect = QRect(m_lassoStartPos, m_lassoCurrentPos).normalized();
    m_selectedPathIndices = m_baseSelectedIndices;

    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.scale(m_scaleFactor, -m_scaleFactor);
    transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());
    QPainterPath rectPath;
    rectPath.addRect(selectionRect);

    for (int i = 0; i < m_displayPaths.size(); ++i) {
        const auto& contour = m_displayPaths[i];
        bool isInside = false;
        for (int j = 0; j < contour.points.size() - 1; ++j) {
            QPointF p1 = transform.map(contour.points[j]);
            QPointF p2 = transform.map(contour.points[j+1]);
            if (selectionRect.contains(p1.toPoint()) || selectionRect.contains(p2.toPoint())) {
                isInside = true; break;
            }
            QPainterPath linePath;
            linePath.moveTo(p1); linePath.lineTo(p2);
            if (rectPath.intersects(linePath)) {
                isInside = true; break;
            }
        }
        if (isInside) m_selectedPathIndices.insert(i);
    }
}

void RenderArea::setMoveMode(bool enabled) {
    m_isMoveMode = enabled;
    m_transformState = TS_Select;
    if (m_moveInputWidget) m_moveInputWidget->hide();
    m_isSnapped = false;
    if (enabled) { setCursor(Qt::CrossCursor); setFocus(); }
    else { clearSelection(); setCursor(Qt::OpenHandCursor); }
    update();
}

void RenderArea::setRotateMode(bool enabled) {
    m_isRotateMode = enabled;
    m_transformState = TS_Select;
    if (m_rotateInputWidget) m_rotateInputWidget->hide();
    m_isSnapped = false;
    if (enabled) { setCursor(Qt::CrossCursor); setFocus(); }
    else { clearSelection(); setCursor(Qt::OpenHandCursor); }
    update();
}

void RenderArea::setMirrorMode(bool enabled) {
    m_isMirrorMode = enabled;
    m_transformState = TS_Select;
    if (m_moveInputWidget) m_moveInputWidget->hide();
    if (m_rotateInputWidget) m_rotateInputWidget->hide();
    m_isSnapped = false;
    if (enabled) { setCursor(Qt::CrossCursor); setFocus(); }
    else { clearSelection(); setCursor(Qt::OpenHandCursor); }
    update();
}

void RenderArea::findSnapPoint(const QPoint &pos) {
    m_isSnapped = false;
    double minDistance = 15.0; // 鼠标靠近 15 像素以内自动吸附
    QPointF closestDxfPos;
    QPoint closestScreenPos;

    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.scale(m_scaleFactor, -m_scaleFactor);
    transform.translate(m_panOffsetDXF.x(), m_panOffsetDXF.y());

    QList<QPointF> candidates;

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    bool hasSelection = false;

    for (int idx : std::as_const(m_selectedPathIndices)) {
        if (idx < 0 || idx >= m_displayPaths.size()) continue;

        const auto& contour = m_displayPaths[idx];
        if (contour.points.isEmpty()) continue;

        hasSelection = true;

        // 1. 先计算该图元的局部包围盒
        double cMinX = contour.points[0].x(), cMaxX = cMinX, cMinY = contour.points[0].y(), cMaxY = cMinY;
        for (const QPointF& pt : contour.points) {
            if (pt.x() < cMinX) cMinX = pt.x();
            if (pt.x() > cMaxX) cMaxX = pt.x();
            if (pt.y() < cMinY) cMinY = pt.y();
            if (pt.y() > cMaxY) cMaxY = pt.y();

            // 顺便累计全局包围盒
            if (pt.x() < minX) minX = pt.x();
            if (pt.x() > maxX) maxX = pt.x();
            if (pt.y() < minY) minY = pt.y();
            if (pt.y() > maxY) maxY = pt.y();
        }

        if (contour.type == "圆" || contour.type.contains("弧") || contour.type.contains("拟合") || contour.type.contains("样条")) {
            candidates << getShapeCenter(contour);
        } else {
            for (const QPointF& pt : contour.points) {
                candidates << pt;
            }
        }
    }

    if (hasSelection) {
        candidates << QPointF((minX + maxX)/2.0, (minY + maxY)/2.0);
    }

    candidates << QPointF(0, 0);

    for (const QPointF& pt : std::as_const(candidates)) {
        QPoint screenPt = transform.map(pt).toPoint();
        double dist = std::hypot(screenPt.x() - pos.x(), screenPt.y() - pos.y());
        if (dist < minDistance) {
            minDistance = dist;
            m_isSnapped = true;
            closestDxfPos = pt;
            closestScreenPos = screenPt;
        }
    }

    if (m_isSnapped) {
        m_snappedDxfPos = closestDxfPos;
        m_snappedScreenPos = closestScreenPos;
    }
}

void RenderArea::executeMove() {
    if (m_transformState != TS_Input) return;
    bool okX, okY;
    double targetX = m_editMoveX->text().toDouble(&okX);
    double targetY = m_editMoveY->text().toDouble(&okY);

    if (!okX || !okY) return;

    double dx = targetX - m_snappedDxfPos.x();
    double dy = targetY - m_snappedDxfPos.y();

    for (int idx : std::as_const(m_selectedPathIndices)) {
        if (idx >= 0 && idx < m_displayPaths.size()) {
            for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                m_displayPaths[idx].points[i].setX(m_displayPaths[idx].points[i].x() + dx);
                m_displayPaths[idx].points[i].setY(m_displayPaths[idx].points[i].y() + dy);
            }
        }
    }
    emit pathsMoved(m_displayPaths);

    m_moveInputWidget->hide();
    m_transformState = TS_Select;
    m_selectedPathIndices.clear();
    m_baseSelectedIndices.clear();
    update();
}

void RenderArea::executeRotate() {
    if (m_transformState != TS_Input) return;

    bool okAngle;
    double angleDeg = m_editRotateAngle->text().toDouble(&okAngle);
    if (!okAngle) return;

    // DXF是数学坐标系逆时针为正，此处直接运用二维旋转矩阵
    double angleRad = angleDeg * M_PI / 180.0;
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);

    double cx = m_snappedDxfPos.x();
    double cy = m_snappedDxfPos.y();

    for (int idx : std::as_const(m_selectedPathIndices)) {
        if (idx >= 0 && idx < m_displayPaths.size()) {
            for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                double x = m_displayPaths[idx].points[i].x() - cx;
                double y = m_displayPaths[idx].points[i].y() - cy;

                // 经典仿射变换
                double nx = x * cosA - y * sinA;
                double ny = x * sinA + y * cosA;

                m_displayPaths[idx].points[i].setX(nx + cx);
                m_displayPaths[idx].points[i].setY(ny + cy);
            }
        }
    }

    // 利用原有的 pathsMoved 信号把状态传回给主窗体并记入Undo撤销堆栈
    emit pathsMoved(m_displayPaths);

    m_rotateInputWidget->hide();
    m_transformState = TS_Select;
    m_selectedPathIndices.clear();
    m_baseSelectedIndices.clear();
    update();
}

void RenderArea::setPositioningBlocks(const QList<PositioningBlock> &blocks) {
    m_posBlocks = blocks;
    update();
}

void RenderArea::contextMenuEvent(QContextMenuEvent *event) {
    if (m_isMoveMode || m_isRotateMode || m_isMirrorMode || m_isEraserMode) {
        event->ignore();
        return;
    }

    QMenu menu(this);
    QAction *actSetWidth = menu.addAction("设置线宽");
    QAction *actReorder = menu.addAction("重新按空间排序");

    QAction *actMoveUCS = nullptr;
    if (m_ucs.valid) {
        actMoveUCS = menu.addAction(QIcon(":/img/images/shift.png"), "移动用户坐标系");
    }

    QAction *actShowConstraints = nullptr;
    if (!m_alignConstraints.isEmpty()) {
        actShowConstraints = menu.addAction("显示参考约束条件");
    }

    // 在鼠标点击的位置弹出菜单
    QAction *res = menu.exec(event->globalPos());

    if (res == actSetWidth) {
        bool ok;
        int newWidth = QInputDialog::getInt(this, "设置线宽", "请输入图纸显示的线宽值:", m_lineWidth, 1, 50, 1, &ok);
        if (ok) {
            m_lineWidth = newWidth;
            update();
        }
    }
    else if (res == actReorder) {
        emit reorderPathsRequested();
    }
    else if (actMoveUCS && res == actMoveUCS) {
        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle("移动坐标系");
        dlg->setWindowModality(Qt::NonModal);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        QFormLayout* layout = new QFormLayout(dlg);
        QDoubleSpinBox* sbX = new QDoubleSpinBox(dlg);
        sbX->setRange(-10000, 10000); sbX->setDecimals(2); sbX->setValue(m_ucs.origin.x());
        QDoubleSpinBox* sbY = new QDoubleSpinBox(dlg);
        sbY->setRange(-10000, 10000); sbY->setDecimals(2); sbY->setValue(m_ucs.origin.y());

        layout->addRow("新原点 X 坐标:", sbX);
        layout->addRow("新原点 Y 坐标:", sbY);

        QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        connect(btnBox, &QDialogButtonBox::accepted, dlg, [this, dlg, sbX, sbY]() {
            m_ucs.origin = QPointF(sbX->value(), sbY->value());
            update();
            dlg->accept();
        });
        dlg->show();
    }else if (actShowConstraints && res == actShowConstraints) {
        QDialog dlg(this);
        dlg.setWindowTitle("查看当前的装配约束");
        dlg.setMinimumSize(350, 250);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        QLabel* info = new QLabel("点击列表中的约束条件，画布中将高亮显示：");
        layout->addWidget(info);

        QListWidget* list = new QListWidget(&dlg);
        for (int i = 0; i < m_alignConstraints.size(); ++i) {
            auto& c = m_alignConstraints[i];
            QString text = QString("约束 %1: %2").arg(i + 1).arg(c.block.name.isEmpty() ? "定位块" : c.block.name);
            list->addItem(text);
        }
        layout->addWidget(list);

        connect(list, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row < m_alignConstraints.size()) {
                m_highlightedConstraintIndex = row;

                // 查找对应的定位块索引进行高亮
                m_highlightedBlockIndex = -1;
                for (int i = 0; i < m_posBlocks.size(); i++) {
                    if (std::abs(m_posBlocks[i].x - m_alignConstraints[row].block.x) < 1e-4 &&
                        std::abs(m_posBlocks[i].y - m_alignConstraints[row].block.y) < 1e-4 &&
                        m_posBlocks[i].type == m_alignConstraints[row].block.type) {
                        m_highlightedBlockIndex = i;
                        break;
                    }
                }
                update(); // 触发绘图区高亮
            }
        });

        // 窗口关闭时恢复颜色
        connect(&dlg, &QDialog::finished, this, [this]() {
            m_highlightedConstraintIndex = -1;
            m_highlightedBlockIndex = -1;
            update();
        });

        dlg.exec();
    }
}

// =========================================================================
// 核心：处理刚体对齐、旋转平移变换与过约束(Over-constraint)判定
// =========================================================================
void RenderArea::applyAlignmentConstraint(const PositioningBlock& block)
{
    if (m_alignConstraints.size() >= 2) {
        QMessageBox::warning(this, "过约束 (Over-Constrained)", "该零件已被两个定位块完全固定 (0自由度)！\n继续添加定位将产生过约束冲突！");
        m_transformState = TS_Select; update(); return;
    }

    if (m_alignConstraints.size() == 1) {
        auto c1 = m_alignConstraints[0];

        if (c1.type != PosBlockType::Line && block.type != PosBlockType::Line) {
            double d_shape = std::hypot(m_alignShapePoint.x() - c1.block.x, m_alignShapePoint.y() - c1.block.y);
            double d_block = std::hypot(block.x - c1.block.x, block.y - c1.block.y);

            if (std::abs(d_shape - d_block) > 1.0) { // 1.0mm 容差
                QMessageBox::warning(this, "过约束拦截",
                                     QString("无法吸附！\n零件上的特征间距为 %1 mm\n两个定位块的间距为 %2 mm\n物理尺寸不匹配，发生撕裂过约束！")
                                         .arg(d_shape, 0, 'f', 2).arg(d_block, 0, 'f', 2));
                m_transformState = TS_Select; update(); return;
            }
        }else if (c1.type == PosBlockType::Line && block.type == PosBlockType::Line) {
            double rad = c1.block.angle * M_PI / 180.0;
            QPointF D(-std::sin(rad), std::cos(rad));
            double rad2 = block.angle * M_PI / 180.0;
            QPointF N2(std::cos(rad2), std::sin(rad2));

            // 检查两条挡板是不是平行的
            double denom = D.x() * N2.x() + D.y() * N2.y();
            if (std::abs(denom) < 1e-4) {
                QMessageBox::warning(this, "过约束拦截", "两条定位边平行，无法完成二维完全固定！");
                m_transformState = TS_Select; update(); return;
            }

            // 校验角度强行扭曲是否超过 1 度
            double shapeAngle = std::atan2(m_alignShapeLine.dy(), m_alignShapeLine.dx()) * 180.0 / M_PI;
            double targetAngle = block.angle + 90.0;
            double angleDiff = targetAngle - shapeAngle;
            while (angleDiff > 90.0) angleDiff -= 180.0;
            while (angleDiff <= -90.0) angleDiff += 180.0;

            if (std::abs(angleDiff) > 1.0) {
                QMessageBox::warning(this, "过约束拦截",
                                     QString("无法吸附！\n将第二条边贴合需要强行扭转 %1 度\n这会破坏原有的第一条边贴合，引发过约束！")
                                         .arg(angleDiff, 0, 'f', 2));
                m_transformState = TS_Select; update(); return;
            }
        }else if (c1.type == PosBlockType::Line && block.type != PosBlockType::Line) {
            double rad = c1.block.angle * M_PI / 180.0;
            QPointF N(std::cos(rad), std::sin(rad));
            QPointF T(block.x - m_alignShapePoint.x(), block.y - m_alignShapePoint.y());

            double error = std::abs(T.x() * N.x() + T.y() * N.y());
            if (error > 1.0) {
                QMessageBox::warning(this, "过约束拦截",
                                     QString("无法吸附！\n将对齐点吸附需要垂直于第一条轨道强行拉扯 %1 mm\n这会破坏原有的直线贴合，引发过约束！")
                                         .arg(error, 0, 'f', 2));
                m_transformState = TS_Select; update(); return;
            }
        } else {
            QMessageBox::warning(this, "过约束警告", "该组合受限自由度过于复杂！\n叠加此定位块将导致内部发生过约束，已被拦截！");
            m_transformState = TS_Select; update(); return;
        }
    }

    for (int idx : std::as_const(m_selectedPathIndices)) {
        for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
            m_displayPaths[idx].points[i] = m_previewTransform.map(m_displayPaths[idx].points[i]);
        }
    }

    QPointF newShapePt = m_previewTransform.map(m_alignShapePoint);
    QLineF newShapeLine = QLineF(m_previewTransform.map(m_alignShapeLine.p1()), m_previewTransform.map(m_alignShapeLine.p2()));
    m_alignConstraints.append({block.type, newShapePt, newShapeLine, block});

    emit pathsMoved(m_displayPaths);

    if (m_alignConstraints.size() == 2) {
        m_transformState = TS_Select;
        m_selectedPathIndices.clear();
        m_baseSelectedIndices.clear();
        update();
        QMessageBox::information(this, "装配完成", "多点复合定位完成，该零件目前已被全固定装配 (0自由度)。");
    } else {
        m_transformState = TS_Select;
        update();
    }
}

// =========================================================================
// 核心：基于解析几何，利用曲线上三点推导真实的圆心位置 (解决圆弧圆心错误问题)
// =========================================================================
QPointF RenderArea::getShapeCenter(const Contour& contour) {
    if (contour.points.isEmpty()) return QPointF();

    // 如果是圆或圆弧，且拥有至少3个点，利用三点共圆公式求解真实的物理圆心
    if (contour.points.size() >= 3 && (contour.type == "圆" || contour.type.contains("弧"))) {
        int n = contour.points.size();
        QPointF p1 = contour.points[0];
        QPointF p2 = contour.points[n / 3];      // 取1/3处的点
        QPointF p3 = contour.points[2 * n / 3];  // 取2/3处的点

        double D = 2 * (p1.x()*(p2.y() - p3.y()) + p2.x()*(p3.y() - p1.y()) + p3.x()*(p1.y() - p2.y()));
        if (std::abs(D) > 1e-6) {
            double xc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p2.y() - p3.y()) +
                         (p2.x()*p2.x() + p2.y()*p2.y())*(p3.y() - p1.y()) +
                         (p3.x()*p3.x() + p3.y()*p3.y())*(p1.y() - p2.y())) / D;
            double yc = ((p1.x()*p1.x() + p1.y()*p1.y())*(p3.x() - p2.x()) +
                         (p2.x()*p2.x() + p2.y()*p2.y())*(p1.x() - p3.x()) +
                         (p3.x()*p3.x() + p3.y()*p3.y())*(p2.x() - p1.x())) / D;
            return QPointF(xc, yc);
        }
    }

    // 如果不是圆，或者三点共线(退化)，则采用矩形包围盒的中心作为保底方案
    double cMinX = contour.points[0].x(), cMaxX = cMinX, cMinY = contour.points[0].y(), cMaxY = cMinY;
    for (const QPointF& pt : contour.points) {
        if (pt.x() < cMinX) cMinX = pt.x(); if (pt.x() > cMaxX) cMaxX = pt.x();
        if (pt.y() < cMinY) cMinY = pt.y(); if (pt.y() > cMaxY) cMaxY = pt.y();
    }
    return QPointF((cMinX + cMaxX)/2.0, (cMinY + cMaxY)/2.0);
}
