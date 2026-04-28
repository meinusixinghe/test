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

using std::min;
using std::max;

// ----------------------------------------------------
// 功能：构造函数，初始化控件属性
// 讲解：RenderArea是 QWidget的子类，作为独立的绘图区域
// 核心：初始化变换参数的默认值，设置控件的视觉和交互属性，为后续绘图和交互做准备
// ----------------------------------------------------
RenderArea::RenderArea(QWidget *parent): QWidget(parent),
    m_scaleFactor(1.0),                                 // 缩放因子（首次加载时自动计算，滚轮事件中修改）
    m_panOffsetDXF(0.0, 0.0),                           // 用户拖拽产生的平移量（DXF坐标单位）
    m_initialContentOffset(0.0, 0.0)                    // 首次加载时，将内容几何中心移到原点的偏移量（实现居中的核心）
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
        m_scaleFactor = 1.0;
        m_panOffsetDXF = QPointF(0.0, 0.0);
        m_initialContentOffset = QPointF(0.0, 0.0);
        m_dxfMinBound = QPointF(0.0, 0.0);
        m_dxfMaxBound = QPointF(0.0, 0.0);
    }

    // 触发重绘
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
    QPainter painter(this);                                         // 2D绘图类
    painter.setRenderHint(QPainter::Antialiasing);                  // 开启抗锯齿，让图形更平滑
    painter.setRenderHint(QPainter::TextAntialiasing);              // 文字抗锯齿，更清晰

    // 首次加载：计算初始变换（适配窗口+居中）；后续：应用当前变换（缩放+平移）
    if (m_scaleFactor == 1.0 && m_initialContentOffset == QPointF(0.0, 0.0)) {
        calculateInitialTransform(painter);
    } else {
        applyCurrentTransform(painter);
    }

    painter.save();
    for (const auto& block : std::as_const(m_posBlocks)) {
        QPainterPath path = block.getPath();
        painter.setPen(QPen(QColor(255, 105, 180), 2 / m_scaleFactor));
        painter.setBrush(QColor(255, 182, 193, 150));

        painter.drawPath(path);
    }
    painter.restore();

    for (int i = 0; i < m_displayPaths.size(); ++i) {
        const auto& contour = m_displayPaths[i];
        if (contour.points.size() > 1) {
            QPen pen;
            if (m_selectedPathIndices.contains(i)) {
                pen = QPen(Qt::red, 0);
                pen.setWidth(3);
            }
            else if (m_highlightPathIndices.contains(i)) {
                pen = QPen(Qt::yellow, 0);
                pen.setWidth(6);
            }
            else {
                pen = QPen(Qt::black, 0);
                pen.setWidth(4);
            }
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.drawPolyline(contour.points.data(), contour.points.size());
        }
    }

    for (int i = 0; i < weldHoles.size(); ++i) {
        if (i == m_highlightIndex) {
            const Hole &hole = weldHoles[i];
            QPen highlightPen(Qt::red, 0);
            highlightPen.setCosmetic(true);
            highlightPen.setWidth(3);
            painter.setPen(highlightPen);
            painter.setBrush(QBrush(Qt::yellow, Qt::Dense6Pattern));
            painter.drawEllipse(hole.center, hole.radius, hole.radius);
        }
    }

    // 绘制用户坐标系
    if (m_showUserCoordinate) {
        painter.save();

        // 1. 动态计算参考大小 (依据管板实际物理尺寸边界)
        double contentW = m_dxfMaxBound.x() - m_dxfMinBound.x();
        double contentH = m_dxfMaxBound.y() - m_dxfMinBound.y();
        double refSize = std::max(contentW, contentH);
        if (refSize < 1.0) refSize = 100.0; // 防止异常数据的安全后备

        // 2. 根据管板大小动态计算各元素尺寸
        double originRadius = refSize * 0.01;  // 原点半径取 1%
        double arrowSize = refSize * 0.03;      // 箭头大小取 2%
        double fontSize = refSize * 0.025;       // 字体大小取 1.5%
        double textOffset = refSize * 0.02;

        // 绘制原点
        QPen originPen(Qt::yellow, 0);
        originPen.setCosmetic(true);
        originPen.setWidth(5);
        painter.setPen(originPen);
        painter.setBrush(Qt::yellow);
        painter.drawEllipse(m_userOrigin, originRadius, originRadius);

        // 绘制 X轴
        QPen xAxisPen(Qt::red, 0);
        xAxisPen.setCosmetic(true);
        xAxisPen.setWidth(2);
        painter.setPen(xAxisPen);
        painter.drawLine(m_userOrigin, m_userXAxis);

        // 绘制 X轴箭头
        QPointF dir = m_userXAxis - m_userOrigin;
        double length = std::hypot(dir.x(), dir.y());
        if (length > 0) dir /= length;
        QPointF arrowBase = m_userXAxis - dir * arrowSize * 0.5;
        QPointF perp(-dir.y(), dir.x());
        QPointF arrowP2 = arrowBase + perp * (arrowSize * 0.3);
        QPointF arrowP3 = arrowBase - perp * (arrowSize * 0.3);
        QPolygonF arrowHead;
        arrowHead << m_userXAxis << arrowP2 << arrowP3;
        painter.setBrush(Qt::red);
        painter.drawPolygon(arrowHead);
        painter.setBrush(Qt::NoBrush);

        // x轴文字
        painter.save();
        painter.scale(1.0, -1.0);
        QFont xFont = painter.font();
        xFont.setPointSizeF(fontSize);
        painter.setFont(xFont);
        painter.drawText(m_userXAxis.x() + textOffset, -(m_userXAxis.y() - textOffset), "X轴");
        painter.restore();

        // 绘制平面参考线(Y轴)
        QPen yAxisPen(Qt::darkGreen, 0);
        yAxisPen.setCosmetic(true);
        yAxisPen.setWidth(2);
        painter.setPen(yAxisPen);
        painter.drawLine(m_userOrigin, m_userPlanePoint);

        QPointF dirY = m_userPlanePoint - m_userOrigin;
        double lengthY = std::hypot(dirY.x(), dirY.y());
        if (lengthY > 0) dirY /= lengthY;
        QPointF arrowBaseY = m_userPlanePoint - dirY * arrowSize * 0.5;
        QPointF perpY(-dirY.y(), dirY.x());
        QPointF arrowP2Y = arrowBaseY + perpY * (arrowSize * 0.3);
        QPointF arrowP3Y = arrowBaseY - perpY * (arrowSize * 0.3);
        QPolygonF arrowHeadY;
        arrowHeadY << m_userPlanePoint << arrowP2Y << arrowP3Y;
        painter.setBrush(Qt::darkGreen);
        painter.drawPolygon(arrowHeadY);
        painter.setBrush(Qt::NoBrush);

        // y轴文字
        painter.save();
        painter.scale(1.0, -1.0);
        QFont yFont = painter.font();
        yFont.setPointSizeF(fontSize);
        painter.setFont(yFont);
        painter.drawText(m_userPlanePoint.x() + textOffset, -(m_userPlanePoint.y() - textOffset), "Y轴");
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

    if (m_isMoveMode && m_moveState == MS_BasePoint && m_isSnapped) {
        painter.save();
        painter.setTransform(QTransform());
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::blue);
        painter.drawEllipse(m_snappedScreenPos, 6, 6);
        painter.restore();
    }
}

// ----------------------------------------------------
// 功能：鼠标滚轮缩放
// 核心：通过滚轮角度计算缩放因子，限制缩放范围，避免过度缩放
// ----------------------------------------------------
void RenderArea::wheelEvent(QWheelEvent *event)
{
    const double scaleStep = 1.15;              // 缩放步长（滚轮每滚一格，缩放15%）
    const double MIN_SCALE = 0.1;               // 最小缩放（防止缩太小看不见）
    const double MAX_SCALE = 2;                 // 最大缩放（防止缩太大）
    int degrees = event->angleDelta().y();      // 滚轮滚动的角度（正为向上，负为向下）

    // 计算新的缩放因子
    if (degrees > 0) {
        m_scaleFactor *= scaleStep;                           // 向上滚：放大
    }else if(degrees < 0) {m_scaleFactor /= scaleStep;}       // 向下滚：缩小

    // 限制缩放范围
    if (m_scaleFactor < MIN_SCALE) {
        m_scaleFactor = MIN_SCALE;
    }else if(m_scaleFactor > MAX_SCALE)m_scaleFactor = MAX_SCALE;

    update();
    event->accept();                            // 标记事件已处理
}

// ----------------------------------------------------
// 功能：左键按下时，记录鼠标初始位置，改变鼠标样式为 “闭手”
// ----------------------------------------------------
void RenderArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_lastMousePos = event->pos();
        m_isMiddlePanning = true;
        setCursor(Qt::ClosedHandCursor); // 拖拽时变为握紧的手
        update();
        event->accept();
        return;
    }

    if (m_isMoveMode && event->button() == Qt::LeftButton) {
        if (m_moveState == MS_Select) {
            m_isLassoDragging = true;
            m_lassoStartPos = event->pos();
            m_lassoCurrentPos = event->pos();
            m_selectedPathIndices.clear();
            update();
            event->accept();
            return;
        } else if (m_moveState == MS_BasePoint && m_isSnapped) {
            m_moveState = MS_Input;

            m_moveInputWidget->adjustSize();
            int widgetW = m_moveInputWidget->width();
            int widgetH = m_moveInputWidget->height();

            int targetX = m_snappedScreenPos.x() + 10;
            int targetY = m_snappedScreenPos.y() + 10;
            if (targetX + widgetW > width()) {
                targetX = m_snappedScreenPos.x() - widgetW - 10;
            }
            if (targetY + widgetH > height()) {
                targetY = m_snappedScreenPos.y() - widgetH - 10;
            }
            targetX = std::max(0, targetX);
            targetY = std::max(0, targetY);

            m_moveInputWidget->move(targetX, targetY);

            m_editMoveX->clear();
            m_editMoveY->clear();
            m_moveInputWidget->show();
            m_editMoveX->setFocus();
            update();
            event->accept();
            return;
        }
    }

    if (m_isLassoMode && event->button() == Qt::LeftButton) {
        m_isLassoDragging = true;
        m_lassoStartPos = event->pos();
        m_lassoCurrentPos = event->pos();
        m_selectedPathIndices.clear();
        update();
        event->accept();
        return;
    }
    if (m_isEraserMode && event->button() == Qt::LeftButton) {
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.scale(m_scaleFactor, -m_scaleFactor);
        transform.translate((m_initialContentOffset + m_panOffsetDXF).x(),
                            (m_initialContentOffset + m_panOffsetDXF).y());
        QPointF dxfPos = transform.inverted().map(QPointF(event->pos()));

        emit itemDeleted(dxfPos);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
        if (!m_isEraserMode && !m_isLassoMode && !m_isMoveMode) {
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
    }
}

// ----------------------------------------------------
// 功能：左键按住时，计算鼠标移动的像素差，转换为 DXF坐标的平移量，应用平移限制后更新平移量
// ----------------------------------------------------
void RenderArea::mouseMoveEvent(QMouseEvent *event)
{
    m_currentMousePos = event->pos();
    if (event->buttons() & Qt::MiddleButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffsetDXF += QPointF(delta.x() / m_scaleFactor, -delta.y() / m_scaleFactor);
        m_lastMousePos = event->pos();
        update();
        event->accept();
        return;
    }
    if (m_isMoveMode) {
        if (m_moveState == MS_Select && m_isLassoDragging) {
            m_lassoCurrentPos = event->pos();
            updateLassoSelection();
            update();
            event->accept();
            return;
        } else if (m_moveState == MS_BasePoint) {
            findSnapPoint(event->pos()); // 寻找附近吸附点
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
    if (m_isLassoDragging) {
        m_lassoCurrentPos = event->pos();
        updateLassoSelection();
        update();
        event->accept();
        return;
    }
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - m_lastMousePos;                       // 像素移动量

        // 1.将像素移动量转换为 DXF坐标单位
        double dx = delta.x() / m_scaleFactor;
        double dy = -delta.y() / m_scaleFactor;

        // 2.计算潜在的新平移量
        QPointF newPanOffset = m_panOffsetDXF + QPointF(dx, dy);

        // 计算管板内容的实际物理尺寸 (DXF单位)
        double contentW = m_dxfMaxBound.x() - m_dxfMinBound.x();
        double contentH = m_dxfMaxBound.y() - m_dxfMinBound.y();
        if (contentW < 1.0) contentW = 100.0;                               // 防止除零或空数据的安全检查
        if (contentH < 1.0) contentH = 100.0;
        // 定义限制范围：管板边界的 1.5 倍
        double limitX = contentW * 0.75;
        double limitY = contentH * 0.75;

        // 3.平移限制
        if (newPanOffset.x() > limitX) {
            newPanOffset.setX(limitX);
        }else if (newPanOffset.x() < -limitX) {
            newPanOffset.setX(-limitX);
        }
        if (newPanOffset.y() > limitY) {
            newPanOffset.setY(limitY);
        }else if (newPanOffset.y() < -limitY) {
            newPanOffset.setY(-limitY);
        }

        // 4.应用新的平移量
        m_panOffsetDXF = newPanOffset;

        // 5.更新上次鼠标位置
        m_lastMousePos = event->pos();

        // 6.触发重绘
        update();
        event->accept();
    }
}

// ----------------------------------------------------
// 功能：左键松开时，恢复鼠标样式为 “开手 ”
// ----------------------------------------------------
void RenderArea::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_isMiddlePanning = false;
        // 根据当前模式恢复光标
        if (m_isLassoMode) setCursor(Qt::CrossCursor);
        else if (m_isEraserMode) setCursor(Qt::BlankCursor);
        else setCursor(Qt::OpenHandCursor);
        update();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (!m_isEraserMode && !m_isLassoMode && !m_isMoveMode) {
            setCursor(Qt::OpenHandCursor);
        }
        event->accept();
    }
    if (m_isLassoDragging && event->button() == Qt::LeftButton) {
        m_isLassoDragging = false;
        updateLassoSelection();
        update();
        event->accept();
        return;
    }
}

void RenderArea::setDisplayPaths(const QVector<Contour>& paths) {
    m_displayPaths = paths;
}

// ----------------------------------------------------
// 坐标变换：合并初始居中、缩放和用户平移
// 解决了 DXF坐标与屏幕坐标的映射问题
// ----------------------------------------------------
void RenderArea::calculateInitialTransform(QPainter &painter)
{
    // 初始化 DXF边界
    double minX = std::numeric_limits<double>::max();           // 返回其能表示的最大正有限值
    double maxX = std::numeric_limits<double>::lowest();        // 返回其能表示的最小正有限值
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    m_dxfMinBound = QPointF(minX, minY);
    m_dxfMaxBound = QPointF(maxX, maxY);

    // 遍历所有孔洞中心和轮廓点，更新边界框
    // 1.创建一个可复用的匿名函数，命名为 updateBounds，用于根据传入的坐标点 p更新绘图区域的边界范围
    // [&]表示按引用捕获外部作用域的所有变量、(const QPointF& p)：参数列表
    auto updateBounds = [&](const QPointF& p) {
        if (p.x() < minX) minX = p.x();
        if (p.x() > maxX) maxX = p.x();
        if (p.y() < minY) minY = p.y();
        if (p.y() > maxY) maxY = p.y();
    };
    // 2.遍历孔洞中心
    for(const auto& h : std::as_const(weldHoles)) updateBounds(h.center);
    if (m_isRectangular) {
        for (const auto& p : std::as_const(m_mainPlatePolygon))                    // 如果是方形，遍历多边形的所有顶点
            updateBounds(p);
    } else {
        // 如果是圆形，加入圆的四个极点
        if (mainPlateHole.radius > 0) {
            updateBounds(QPointF(mainPlateHole.center.x() - mainPlateHole.radius, mainPlateHole.center.y()));
            updateBounds(QPointF(mainPlateHole.center.x() + mainPlateHole.radius, mainPlateHole.center.y()));
            updateBounds(QPointF(mainPlateHole.center.x(), mainPlateHole.center.y() - mainPlateHole.radius));
            updateBounds(QPointF(mainPlateHole.center.x(), mainPlateHole.center.y() + mainPlateHole.radius));
        }
    }
    for (const auto& contour : std::as_const(m_displayPaths)) {
        for (const auto& p : contour.points) {
            updateBounds(p);
        }
    }
    if (minX > maxX) { applyCurrentTransform(painter); return; }    // 无有效点

    // 计算内容的宽高和适配窗口的缩放因子
    double contentW = maxX - minX;                                  // 宽度
    double contentH = maxY - minY;                                  // 高度
    double padding = 20.0;
    double scaleX = (width() - 2 * padding) / contentW;
    double scaleY = (height() - 2 * padding) / contentH;
    m_scaleFactor = min(scaleX, scaleY);                            // 取较小的缩放因子，保证内容完整显示

    // 计算初始居中偏移量：将内容几何中心移到原点（绝大部分情况是(0,0)）
    m_initialContentOffset = QPointF(-(minX + contentW / 2.0), -(minY + contentH / 2.0));

    // DXF边界（用于后续平移限制）
    m_dxfMinBound = QPointF(minX, minY);
    m_dxfMaxBound = QPointF(maxX, maxY);

    // 应用初始变换
    applyCurrentTransform(painter);
}

// ----------------------------------------------------
// 坐标变换：应用居缩放和用户平移量
// ----------------------------------------------------
void RenderArea::applyCurrentTransform(QPainter &painter)
{
    // 1. 平移到 Widget中心 (Widget坐标系)
    // 是 QPainter最核心的坐标变换方法，移动绘图坐标系的原点
    // width()：客户区宽度（像素）、height()：客户区高度（像素）
    painter.translate(width() / 2.0, height() / 2.0);

    // 2. 应用总缩放因子 (m_scaleFactor)和 Y轴翻转(从 DXF坐标 Y轴向上到屏幕 Y轴向下)
    painter.scale(m_scaleFactor, -m_scaleFactor);

    // 3. 应用总偏移量(初始居中 + 用户累积平移)
    QPointF totalOffset = m_initialContentOffset + m_panOffsetDXF;
    painter.translate(totalOffset);
}

// ----------------------------------------------------
// 设置用户坐标
// ----------------------------------------------------
void RenderArea::setUserCoordinatePoints(const QPointF& origin, const QPointF& xAxis, const QPointF& planePoint)
{
    m_userOrigin = origin;
    m_userXAxis = xAxis;
    m_userPlanePoint = planePoint;
    update();
}

// ----------------------------------------------------
// 是否显示用户坐标系
// ----------------------------------------------------
void RenderArea::setShowUserCoordinate(bool show)
{
    m_showUserCoordinate = show;
    update();
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

void RenderArea::setLassoMode(bool enabled) {
    m_isLassoMode = enabled;
    if (enabled) {
        setCursor(Qt::CrossCursor); // 开启套索：立即变为十字
        setFocus();
    } else {
        clearSelection();
        setCursor(Qt::OpenHandCursor); // 关闭套索：恢复为普通手型
    }
    update();
}

void RenderArea::clearSelection() {
    m_selectedPathIndices.clear();
    m_isLassoDragging = false;
    update();
}

void RenderArea::keyPressEvent(QKeyEvent *event) {
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && !m_selectedPathIndices.isEmpty()) {
        if (m_moveState != MS_Input) { // 防止在输入坐标时误删线条
            emit bulkPathsDeleted(m_selectedPathIndices.values());
            clearSelection();
        }
    }
    else if (event->key() == Qt::Key_Escape) {
        if (m_isMoveMode) { // 移动模式的逐级取消
            if (m_moveState == MS_Input) {
                m_moveInputWidget->hide();
                m_moveState = MS_BasePoint;
            } else if (m_moveState == MS_BasePoint) {
                m_moveState = MS_Select;
                clearSelection();
            } else {
                emit cancelModesRequested();
            }
            update();
        }
        else if (m_isLassoMode && !m_selectedPathIndices.isEmpty()) {
            clearSelection();
        } else {
            emit cancelModesRequested();
        }
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_isMoveMode && m_moveState == MS_Select && !m_selectedPathIndices.isEmpty()) {
            m_moveState = MS_BasePoint;
            update();
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

void RenderArea::updateLassoSelection() {
    QRect selectionRect = QRect(m_lassoStartPos, m_lassoCurrentPos).normalized();
    m_selectedPathIndices.clear();

    // 构建屏幕坐标到 DXF 坐标的映射矩阵
    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.scale(m_scaleFactor, -m_scaleFactor);
    transform.translate((m_initialContentOffset + m_panOffsetDXF).x(),
                        (m_initialContentOffset + m_panOffsetDXF).y());

    QPainterPath rectPath;
    rectPath.addRect(selectionRect);

    // 遍历所有线条，检查是否在矩形内或与矩形相交
    for (int i = 0; i < m_displayPaths.size(); ++i) {
        const auto& contour = m_displayPaths[i];
        bool isInside = false;
        for (int j = 0; j < contour.points.size() - 1; ++j) {
            QPointF p1 = transform.map(contour.points[j]);
            QPointF p2 = transform.map(contour.points[j+1]);

            // 只要端点在框内，或者线段与边框相交，就算选中
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
    m_moveState = MS_Select;
    if (m_moveInputWidget) m_moveInputWidget->hide();
    m_isSnapped = false;

    if (enabled) {
        setCursor(Qt::CrossCursor);
        setFocus();
    } else {
        clearSelection();
        setCursor(Qt::OpenHandCursor);
    }
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
    transform.translate((m_initialContentOffset + m_panOffsetDXF).x(),
                        (m_initialContentOffset + m_panOffsetDXF).y());

    for (int idx : std::as_const(m_selectedPathIndices)) {
        if (idx < 0 || idx >= m_displayPaths.size()) continue; // 安全检查

        const auto& contour = m_displayPaths[idx];
        if (contour.points.isEmpty()) continue;

        QList<QPointF> candidates;
        candidates << contour.points.first() << contour.points.last(); // 添加起点和终点

        if (contour.type == "圆" && contour.points.size() >= 3) {
            double minX = contour.points[0].x(), maxX = minX, minY = contour.points[0].y(), maxY = minY;
            for (const QPointF& p : contour.points) {
                if (p.x() < minX) minX = p.x(); if (p.x() > maxX) maxX = p.x();
                if (p.y() < minY) minY = p.y(); if (p.y() > maxY) maxY = p.y();
            }
            candidates << QPointF((minX + maxX)/2.0, (minY + maxY)/2.0);
        }

        // 判断距离最近的吸附点
        for (const QPointF& pt : candidates) {
            QPoint screenPt = transform.map(pt).toPoint();
            double dist = std::hypot(screenPt.x() - pos.x(), screenPt.y() - pos.y());
            if (dist < minDistance) {
                minDistance = dist;
                m_isSnapped = true;
                closestDxfPos = pt;
                closestScreenPos = screenPt;
            }
        }
    }

    if (m_isSnapped) {
        m_snappedDxfPos = closestDxfPos;
        m_snappedScreenPos = closestScreenPos;
    }
}

void RenderArea::executeMove() {
    if (m_moveState != MS_Input) return;

    bool okX, okY;
    double targetX = m_editMoveX->text().toDouble(&okX);
    double targetY = m_editMoveY->text().toDouble(&okY);

    if (!okX || !okY) return; // 输入非法则不移动

    // 计算移动增量
    double dx = targetX - m_snappedDxfPos.x();
    double dy = targetY - m_snappedDxfPos.y();

    // 更新选中的线条坐标
    for (int idx : m_selectedPathIndices) {
        if (idx >= 0 && idx < m_displayPaths.size()) {
            for (int i = 0; i < m_displayPaths[idx].points.size(); ++i) {
                m_displayPaths[idx].points[i].setX(m_displayPaths[idx].points[i].x() + dx);
                m_displayPaths[idx].points[i].setY(m_displayPaths[idx].points[i].y() + dy);
            }
        }
    }
    emit pathsMoved(m_displayPaths);
    // 移动完成，重置状态回选择模式
    m_moveInputWidget->hide();
    m_moveState = MS_Select;
    m_selectedPathIndices.clear();
    update();
}

void RenderArea::setPositioningBlocks(const QList<PositioningBlock> &blocks) {
    m_posBlocks = blocks;
    update();
}
