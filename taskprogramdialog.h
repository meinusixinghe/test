#ifndef TASKPROGRAMDIALOG_H
#define TASKPROGRAMDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include "mainwindow.h"

class TaskProgramDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskProgramDialog(unsigned int devId, const QVector<Contour>& paths, const UserCoordSystem& ucs, QWidget *parent = nullptr);

private slots:
    void onAddRowClicked();
    void onRemoveRowClicked();
    void onSyncPosClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onResetClicked();
    void generateProgram();

private:
    void addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap, const QString& remark = "");

    unsigned int m_devId;
    QVector<Contour> m_paths;
    UserCoordSystem m_ucs;

    QTableWidget* m_table;
    QLabel* m_statusLabel;

    QComboBox* m_coordCombo;     // 几何基准
    QComboBox* m_robotToolCombo; // 机器人工具(Tool)
    QComboBox* m_robotUserCombo; // 机器人用户(Wobj)

    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_resetBtn;
};

#endif // TASKPROGRAMDIALOG_H
