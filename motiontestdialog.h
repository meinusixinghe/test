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

    // 提供给主界面调用的设备 ID 更新接口
    void setDevId(unsigned int id) { m_devId = id; }

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
    void onMultiGetJointPosClicked();
    void onMultiGetCartPosClicked();
    void onExecuteMultiMoveClicked();

    // 👇【新增】：================== MultiMove2 专属槽函数 ==================
    void onMulti2AddRowClicked();
    void onMulti2RemoveRowClicked();
    void onMulti2GetJointPosClicked();
    void onMulti2GetCartPosClicked();
    void onExecuteMultiMove2Clicked();

private:
    void setupUi();
    QWidget* createMlinPage();
    QWidget* createMultiMovePage();
    QWidget* createMultiMove2Page(); // 👈【新增】

    void addRowToMlinTable(double* pos = nullptr, int speed = 100, double zone = 10.0);
    void addRowToMultiTable(int type = 2, double* pos = nullptr, int speed = 100, double overlap = 0.0);
    // 👇【新增】：MultiMove2 行添加函数，多了 posType, acc, dec
    void addRowToMulti2Table(int moveType = 2, int posType = 2, double* pos = nullptr, double speed = 100, double acc = 50, double dec = 50, double overlap = 0.0);

    unsigned int m_devId;

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

    // 👇【新增】：MultiMove2 页面组件
    QTableWidget* m_multi2Table;
    QLabel* m_multi2StatusLabel;
    QPushButton* m_multi2RunBtn;
};

#endif // MOTIONTESTDIALOG_H
