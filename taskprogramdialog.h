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
    explicit TaskProgramDialog(unsigned int devId, const QVector<Hole>& holes, QWidget *parent = nullptr);

private slots:
    void onAddRowClicked();
    void onRemoveRowClicked();
    void onSyncPosClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onResetClicked();

private:
    void addRow(int moveType, int posType, double* pos, double speed, double acc, double dec, double overlap);

    unsigned int m_devId;
    QTableWidget* m_table;
    QLabel* m_statusLabel;

    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_resetBtn;
};

#endif // TASKPROGRAMDIALOG_H
