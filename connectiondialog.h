#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>

class ConnectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    QString getIp() const;
    void setIp(const QString& ip);

    int getAction() const;

private:
    QLineEdit* m_ipEdit;
    int m_action = 0;
};

#endif // CONNECTIONDIALOG_H
