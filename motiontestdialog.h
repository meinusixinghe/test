#ifndef MOTIONTESTDIALOG_H
#define MOTIONTESTDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class MotionTestDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MotionTestDialog(unsigned int devId, QWidget *parent = nullptr);

private slots:
    void switchPage(int index);

    // ================== MLIN 专属槽函数 ==================
    void onMlinAddRowClicked();
    void onMlinRemoveRowClicked();
    void onMlinGetPosClicked();
    void onExecuteMlinClicked();

    // ================== MultiMove 专属槽函数 ==================
    void onMultiAddRowClicked();
    void onMultiRemoveRowClicked();
    void onMultiGetJointPosClicked(); // 获取关节坐标 (Type 1)
    void onMultiGetCartPosClicked();  // 获取笛卡尔坐标 (Type 2)
    void onExecuteMultiMoveClicked();

private:
    void setupUi();
    QWidget* createMlinPage();
    QWidget* createMultiMovePage();

    // 辅助添加行的函数
    void addRowToMlinTable(double* pos = nullptr, int speed = 100, double zone = 10.0);
    void addRowToMultiTable(int type = 2, double* pos = nullptr, int speed = 100, double overlap = 0.0);

    unsigned int m_devId;

    // 左侧菜单与右侧容器
    QListWidget* m_navMenu;
    QStackedWidget* m_stackedWidget;

    // MLIN 页面组件
    QTableWidget* m_mlinTable;
    QLabel* m_mlinStatusLabel;
    QPushButton* m_mlinRunBtn;

    // MultiMove 页面组件
    QTableWidget* m_multiTable;
    QLabel* m_multiStatusLabel;
    QPushButton* m_multiRunBtn;
};

#endif // MOTIONTESTDIALOG_H
