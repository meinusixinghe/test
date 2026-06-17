#ifndef TASKPROGRAMDIALOG_H
#define TASKPROGRAMDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include "mainwindow.h"
#include <QTimer>
#include <QSpinBox>
#include <QDoubleSpinBox>

class TaskProgramDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, int platePos, double thickness, QWidget *parent = nullptr);
    void updateWorkpieceParams(int posIndex, double thickness);

signals:
    void workpieceParamsChanged(int posIndex, double thickness);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onAddRowClicked();
    void onRemoveRowClicked();
    void onSyncPosClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onResetClicked();
    void generateProgram();
    void updateRobotState();

private:
    void addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap, const QString& remark = "", int insertRowIndex = -1);

    unsigned int m_devId;
    QVector<Contour> m_paths;
    UserCoordSystem m_ucs;

    QTableWidget* m_table;
    QLabel* m_statusLabel;

    QLabel* m_robotStateLabel;
    QTimer* m_statusTimer;

    QComboBox* m_coordCombo;
    QComboBox* m_robotToolCombo;
    QComboBox* m_robotUserCombo;

    QComboBox* m_platePosCombo = nullptr;
    QDoubleSpinBox* m_thicknessSpin = nullptr;

    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_resetBtn;

    QThread* m_blockMoveThread = nullptr;
    bool m_blockMoveStopRequested = false;
    bool m_resetAfterBlockStop = false;

    void setBlockMoveRunning(bool running);

    QSpinBox* m_speedRatioSpinBox = nullptr;
};

#endif // TASKPROGRAMDIALOG_H
