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
#include <QTime>

struct Hole;
struct Contour;

class RenderArea : public QWidget
{
    Q_OBJECT
public:
    explicit RenderArea(QWidget *parent = nullptr);

    void setData(const QVector<Hole> &h,const Hole &mPH,const QPolygonF &platePoly, bool isRect, bool resetView = true);

    void setHighlightedIndex(int index);

    void setDisplayPaths(const QVector<Contour>& paths);
    void setHighlightedPathIndices(const QList<int> &indices);
    void setEraserMode(bool enabled);
    double scaleFactor() const { return m_scaleFactor; }
    double distancePointToSegment(const QPointF& p, const QPointF& p1, const QPointF& p2);
    void setEraserSize(int size);
    void setRotateMode(bool enabled);
    void setMirrorMode(bool enabled);
    void clearSelection();
    void setUCSSelectionMode(int mode);
    void setUCS(const UserCoordSystem& ucs);

    enum TransformState { TS_Select, TS_BasePoint, TS_SecondPoint, TS_Input, TS_SelectShapeFeature, TS_DraggingToBlock };
    void setMoveMode(bool enabled);
    void findSnapPoint(const QPoint &pos);

    void setPositioningBlocks(const QList<PositioningBlock> &blocks);
    void resetView();
    QList<PositioningBlock> getPositioningBlocks() const { return m_posBlocks; }
public slots:
    void executeMove();
    void executeRotate();
protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;
signals:
    void itemDeleted(const QPointF &dxfPos);
    void bulkPathsDeleted(QList<int> indices);
    void cancelModesRequested();
    void pathsMoved(const QVector<Contour> &updatedPaths);
    void reorderPathsRequested();
    void ucsPointSelected(QPointF pt);
    void ucsLineSelected(QLineF line);
private:
    QVector<Hole> weldHoles;
    Hole mainPlateHole;
    QVector<Contour> contours;
    QPolygonF m_mainPlatePolygon;
    bool m_isRectangular = false;

    double m_scaleFactor;
    QPointF m_panOffsetDXF;

    QPointF m_dxfMinBound;
    QPointF m_dxfMaxBound;

    QList<int> m_highlightPathIndices;

    QPoint m_lastMousePos;
    bool m_firstPaint = true;
    void applyCurrentTransform(QPainter &painter);

    QVector<Contour> m_displayPaths;
    int m_highlightPathIndex = -1;
    int m_highlightIndex = -1;

    bool m_isEraserMode = false;
    QPoint m_currentMousePos;
    int m_eraserSize = 20;
    void updateLassoSelection();
    bool m_isRotateMode = false;
    bool m_isMirrorMode = false;
    bool m_isLassoDragging = false;
    QPoint m_lassoStartPos;
    QPoint m_lassoCurrentPos;
    QSet<int> m_selectedPathIndices;
    QSet<int> m_baseSelectedIndices;
    QPixmap m_eraserPixmap;
    bool m_isMiddlePanning = false;

    bool m_isMoveMode = false;
    TransformState m_transformState = TS_Select;
    QWidget *m_rotateInputWidget;
    QLineEdit *m_editRotateAngle;
    bool m_isSnapped = false;
    QPointF m_snappedDxfPos;
    QPoint m_snappedScreenPos;

    QWidget *m_moveInputWidget;
    QLineEdit *m_editMoveX;
    QLineEdit *m_editMoveY;

    QList<PositioningBlock> m_posBlocks;

    int m_lineWidth = 2;
    QPointF m_mirrorAxisPoint1;

    bool m_hasHoveredFeature = false;
    QPointF m_hoveredPoint;
    QLineF m_hoveredLine;

    bool m_isSnappedToBlock = false;
    int m_snappedBlockIndex = -1;
    QTransform m_previewTransform;

    PosBlockType m_alignTargetType;
    struct AlignConstraint {
        PosBlockType type;
        QPointF shapePt;
        QLineF shapeLine;
        PositioningBlock block;
    };
    QList<AlignConstraint> m_alignConstraints;
    QPointF m_alignShapePoint;
    QLineF m_alignShapeLine;

    void applyAlignmentConstraint(const PositioningBlock& block);

    int m_ucsSelectMode = 0;
    UserCoordSystem m_ucs;

    QPointF getShapeCenter(const Contour& contour);
};

#endif // RENDERAREA_H
