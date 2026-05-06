#ifndef POSITIONINGDIALOG_H
#define POSITIONINGDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QPainterPath>
#include <QList>
#include <QTransform>
#include <QStackedWidget>
#include <QDoubleSpinBox>

// 定义定位块类型
enum class PosBlockType { Line, Point, Arc, Circle };

// 定位块数据结构
struct PositioningBlock {
    PosBlockType type;
    double x = 0, y = 0;       // 中心或基准点
    double length = 0, width = 0; // 直线专用
    double radius = 0;         // 圆/圆弧专用
    double angle = 0;          // 旋转角度

    QPainterPath getPath() const; // 生成绘图轮廓
};

// 左侧的预览绘图区
class PreviewArea : public QWidget {
    Q_OBJECT
public:
    explicit PreviewArea(QWidget *parent = nullptr);
    void addBlock(const PositioningBlock& block);
    QList<PositioningBlock> getBlocks() const { return m_blocks; }
    void setInitialBlocks(const QList<PositioningBlock>& blocks);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void autoFit();
    QList<PositioningBlock> m_blocks;
    int m_selectedIndex = -1;
    QTransform m_transform;

    double m_scaleFactor = 1.0;
    QPointF m_panOffset = QPointF(0, 0);
    QPoint m_lastMousePos;
    bool m_isPanning = false;
    bool m_firstPaint = true;
};

// 浮动主窗口
class PositioningDialog : public QDialog {
    Q_OBJECT
public:
    explicit PositioningDialog(QWidget *parent = nullptr);
    QList<PositioningBlock> getBlocks() const { return m_previewArea->getBlocks(); }
    void setInitialBlocks(const QList<PositioningBlock>& blocks);

private slots:
    void onAddClicked();

private:
    PreviewArea *m_previewArea;
    QStackedWidget *m_stackedWidget;
    PosBlockType m_currentType = PosBlockType::Line;

    // 参数输入框
    QDoubleSpinBox *m_lineLen, *m_lineWidth, *m_lineX, *m_lineY, *m_lineAngle;
    QDoubleSpinBox *m_ptX, *m_ptY, *m_ptAngle;
    QDoubleSpinBox *m_arcX, *m_arcY, *m_arcR, *m_arcAngle;
    QDoubleSpinBox *m_cirX, *m_cirY, *m_cirR;

    QDoubleSpinBox* createSpinBox(double min = -10000, double max = 10000, double val = 0);
    QWidget *m_detailContainer;
    void setupTypeButtons();
};

#endif
