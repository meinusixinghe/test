#ifndef RENDERAREA_H
#define RENDERAREA_H

#include <QWidget>
#include <QVector>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include "mainwindow.h"
#include <QSet>
#include <QPixmap>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QLabel>
#include "positioningdialog.h"

struct Hole;
struct Contour;

class RenderArea : public QWidget
{
    Q_OBJECT
public:
    explicit RenderArea(QWidget *parent = nullptr);

    void setData(const QVector<Hole> &h,const Hole &mPH,const QPolygonF &platePoly, bool isRect, bool resetView = true);

    void setHighlightedIndex(int index);

    void setUserCoordinatePoints(const QPointF& origin, const QPointF& xAxis, const QPointF& planePoint);
    void setShowUserCoordinate(bool show);

    void setDisplayPaths(const QVector<Contour>& paths);
    void setHighlightedPathIndices(const QList<int> &indices);
    void setEraserMode(bool enabled);
    double scaleFactor() const { return m_scaleFactor; }
    double distancePointToSegment(const QPointF& p, const QPointF& p1, const QPointF& p2);
    void setEraserSize(int size);
    void setLassoMode(bool enabled);
    void clearSelection();

    enum MoveState { MS_Select, MS_BasePoint, MS_Input };
    void setMoveMode(bool enabled);
    void findSnapPoint(const QPoint &pos);

    void setPositioningBlocks(const QList<PositioningBlock> &blocks);
    void resetView();
    QList<PositioningBlock> getPositioningBlocks() const { return m_posBlocks; }
public slots:
    void executeMove();
protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    // 用于平移的鼠标事件
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;
signals:
    void itemDeleted(const QPointF &dxfPos);
    void bulkPathsDeleted(QList<int> indices);
    void cancelModesRequested();
    void pathsMoved(const QVector<Contour> &updatedPaths);
private:
    // 管板数据
    QVector<Hole> weldHoles;                                // 仅焊接管孔（不含主体圆）
    Hole mainPlateHole;                                     // 管板主体圆（最大半径)
    QVector<Contour> contours;
    QPolygonF m_mainPlatePolygon;                           // 存储方形轮廓
    bool m_isRectangular = false;                           // 标记

    // 缩放和平移参数
    double m_scaleFactor;                                   // 总缩放比例 (包含初始自适应)
    QPointF m_panOffsetDXF;                                 // 用户累积的平移量 (DXF坐标单位)

    // DXF数据的边界 (用于限制平移)
    QPointF m_dxfMinBound;
    QPointF m_dxfMaxBound;

    // 当前要高亮显示的孔洞索引
    QList<int> m_highlightPathIndices;

    // 鼠标拖拽
    QPoint m_lastMousePos;                                  // 记录鼠标上一次的位置 (Widget像素单位)
    bool m_firstPaint = true;
    void applyCurrentTransform(QPainter &painter);          // 应用所有变换

    // 用户坐标系显示相关
    bool m_showUserCoordinate = false;                      // 显示/隐藏用户坐标系
    QPointF m_userOrigin;                                   // 用户坐标系原点
    QPointF m_userXAxis;                                    // 用户坐标系 x轴
    QPointF m_userPlanePoint;                               // 用户坐标系 y轴

    QVector<Contour> m_displayPaths;
    int m_highlightPathIndex = -1;
    int m_highlightIndex = -1;

    bool m_isEraserMode = false;
    QPoint m_currentMousePos;
    int m_eraserSize = 20;
    void updateLassoSelection();
    bool m_isLassoMode = false;
    bool m_isLassoDragging = false;
    QPoint m_lassoStartPos;
    QPoint m_lassoCurrentPos;
    QSet<int> m_selectedPathIndices;
    QSet<int> m_baseSelectedIndices;
    QPixmap m_eraserPixmap;
    bool m_isMiddlePanning = false;

    bool m_isMoveMode = false;
    MoveState m_moveState = MS_Select;
    bool m_isSnapped = false;
    QPointF m_snappedDxfPos;
    QPoint m_snappedScreenPos;

    QWidget *m_moveInputWidget;
    QLineEdit *m_editMoveX;
    QLineEdit *m_editMoveY;

    QList<PositioningBlock> m_posBlocks;

    int m_lineWidth = 2;
};

#endif // RENDERAREA_H
