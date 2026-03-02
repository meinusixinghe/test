#include "connectiondialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QRegularExpressionValidator>
#include <QLabel>

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("通讯连接设置");

    QFormLayout* layout = new QFormLayout(this);

    // IP 输入框
    m_ipEdit = new QLineEdit(this);
    // IPv4正则验证
    QRegularExpression ipRegex("^((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)$");
    m_ipEdit->setValidator(new QRegularExpressionValidator(ipRegex, this));
    m_ipEdit->setPlaceholderText("192.168.1.10");
    m_ipEdit->setText("127.0.0.1");
    m_ipEdit->setMinimumWidth(200);
    layout->addRow("机器人 IP:", m_ipEdit);

    // 端口输入框
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(502);
    m_portSpin->setMinimumWidth(200);
    layout->addRow("端口号:", m_portSpin);

    QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    adjustSize();
    setFixedSize(size());
}

QString ConnectionDialog::getIp() const { return m_ipEdit->text(); }
int ConnectionDialog::getPort() const { return m_portSpin->value(); }
void ConnectionDialog::setIp(const QString& ip) { m_ipEdit->setText(ip); }
void ConnectionDialog::setPort(int port) { m_portSpin->setValue(port); }
