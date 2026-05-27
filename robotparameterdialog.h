#ifndef ROBOTPARAMETERDIALOG_H
#define ROBOTPARAMETERDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <functional>

class QLabel;

class RobotParameterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RobotParameterDialog(unsigned int devId, QWidget *parent = nullptr);

private slots:
    // 页面切换槽函数
    void switchPage(int index);

    // 速度相关的槽函数
    void onSetSpeedClicked();
    void onIncreasePressed();
    void onIncreaseReleased();
    void onDecreasePressed();
    void onDecreaseReleased();

private:
    unsigned int m_devId;

    // --- 核心布局组件 ---
    QListWidget* m_navMenu;           // 左侧导航菜单
    QStackedWidget* m_stackedWidget;  // 右侧层叠显示区域

    // --- 速度页面组件 ---
    QSpinBox* m_speedSpinBox;
    QPushButton* m_setSpeedBtn;
    QPushButton* m_increaseBtn;
    QPushButton* m_decreaseBtn;

    // --- 内部辅助函数 ---
    void setupUi();                   // 统筹搭建整体UI
    QWidget* createSpeedPage();       // 创建“全局速度”页面
    QWidget* createMotionPage();      // 创建“机器人运动”页面

    void handleJogButton(int axisIndex, bool positive, bool pressed, QLabel* statusLabel);
    void runAsyncCommand(std::function<void()> cmd);
};

#endif // ROBOTPARAMETERDIALOG_H
