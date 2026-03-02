#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

class ConnectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    QString getIp() const;
    int getPort() const;

    void setIp(const QString& ip);
    void setPort(int port);

private:
    QLineEdit* m_ipEdit;
    QSpinBox* m_portSpin;
};

#endif // CONNECTIONDIALOG_H
