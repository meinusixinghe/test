#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QCloseEvent>
#include <QDate>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QMatrix4x4>
#include <QMenu>
#include <QMouseEvent>
#include <QPointF>
#include <QPolygonF>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QVector>
#include <QVector3D>
#include <QVBoxLayout>
#include <QWidget>

#include "EfortSdk.h"
#include "weldingprocessdialog.h"

class QCalendarWidget;
class RenderArea;
class usercoordinatemanager;

class LogViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit LogViewerDialog(QWidget *parent = nullptr);
private slots:
    void onDateChanged(const QDate &date);
private:
    QCalendarWidget* m_calendar;
    QTextEdit* m_textEdit;
    void loadLogForDate(const QDate& date);
    QDate findEarliestLogDate();
};

struct Hole { QPointF center; QVector3D center3D; double radius = 0.0; };
struct Contour { QString type; QVector<QPointF> points; double bevelAngle = 0.0; double rootFace = 0.0; };
struct DrawingState {
    QVector<Contour> displayPaths;
    QVector<Hole> weldHoles;
    Hole mainPlateHole;
    QPolygonF mainPlateContour;
};

class FloatingToolWidget : public QWidget {
    Q_OBJECT
public:
    QPushButton *btnRestore;
    QPushButton *btnEraser;
    QPushButton *btnLasso;
    QPushButton *btnClose;
    QPushButton *btnMove;
    QPushButton *btnUndo;

    QSlider *sliderEraserSize;
    QLabel *lblEraserSize;

    explicit FloatingToolWidget(QWidget *parent = nullptr);
    void setSliderVisible(bool visible);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QPoint m_dragPosition;
    QWidget *sliderContainer;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void importDxf();
    void handleTableSelectionChanged();
    void handleTableCellChanged(int row, int column);
    void applyRotationMatrix();
    void setupCoordinateWizard();
    void onManageWeldingProcess();
    void onConnectTriggered();
    void onStartClicked();
    void onPauseClicked();
    void onResetClicked();
    void sendNextWeldHole();
    void restoreDrawing();
    void handleItemDeleted(const QPointF &pos);
    void handleBulkPathsDeleted(QList<int> indices);
    void onBevelParametersChanged();
    void showAndSaveLog(const QString& msg);
    void showLogViewer();
    void showTableContextMenu(const QPoint &pos);
    void moveSelectedRowsUp();
    void moveSelectedRowsDown();
    void moveSelectedRowsToTop();
    void moveSelectedRowsToBottom();
    void toggleRobotPower();
    void onStatusTimer();

private:
    void loadDrawingData(const QString &filePath);
    void setupUi();
    void loadWeldingProcesses();
    void updateTableIndices();

    QVector<Hole> allHoles;
    QVector<Hole> weldHoles;
    Hole mainPlateHole;
    QVector<Contour> contours;
    QPolygonF mainPlateContour;
    bool isRectangularPlate = false;

    RenderArea* renderArea;
    QTableWidget* dataTable;

    QSplitter* rightSplitter;
    QWidget* detailWidget;
    QVector<Contour> m_displayPaths;

    QAction* loadAction;
    QMenu* fileMenu;
    QToolBar* toolBar;

    QAction* rotateAction;
    QMatrix4x4 rotationMatrix;

    usercoordinatemanager* m_coordManager;
    QAction* m_setupCoordAction;
    QPushButton* m_toggleCoordBtn;
    QLabel* m_statusLabel;
    QMenu* m_operationMenu;

    QVector<WeldingProcess> m_weldingProcesses;
    QAction* m_manageProcessAction;

    QAction* m_connectAction;

    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resetBtn;

    QString m_lastIp = "192.168.1.10";
    QLabel* m_statusIconLabel;
    QLabel* m_statusTextLabel;

    QLabel* m_servoIconLabel;
    QLabel* m_servoTextLabel;

    QLabel* m_autoTextLabel;

    bool m_isShuttingDown = false;
    bool m_isRobotCommandRunning = false;

    int m_currentWeldIndex = 0;
    bool m_isWeldingProcessRunning = false;

    QAction* m_imageProcessAction;
    FloatingToolWidget* m_floatingToolWidget;

    QMenu* m_toolsMenu;
    QVector<Contour> m_originalDisplayPaths;
    QVector<Hole> m_originalWeldHoles;
    Hole m_originalMainPlateHole;
    QPolygonF m_originalMainPlateContour;
    QTextEdit* m_detailContentText;

    QList<DrawingState> m_undoStack;
    void saveUndoState();
    void undo();

    QDoubleSpinBox* m_bevelAngleSpin;
    QDoubleSpinBox* m_rootFaceSpin;

    void appendLogToFile(const QString& msg);
    LogViewerDialog* m_logViewerDialog = nullptr;

    QPushButton* m_powerBtn = nullptr;
    bool m_isRobotPoweredOn = false;
    unsigned int m_currentDevId = 0;

    QTimer* m_statusTimer;
};

#endif
