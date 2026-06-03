#include "connectiondialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpressionValidator>

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Robot Connection");

    QFormLayout* layout = new QFormLayout(this);

    m_ipEdit = new QLineEdit(this);
    QRegularExpression ipRegex("^((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)$");
    m_ipEdit->setValidator(new QRegularExpressionValidator(ipRegex, this));
    m_ipEdit->setPlaceholderText("192.168.1.10");
    m_ipEdit->setMinimumWidth(200);
    layout->addRow("Robot IP:", m_ipEdit);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnConnect = new QPushButton("Connect", this);
    QPushButton* btnDisconnect = new QPushButton("Disconnect", this);

    btnConnect->setStyleSheet("background-color: #2196F3; color: white; padding: 5px 15px; border-radius: 4px;");
    btnDisconnect->setStyleSheet("background-color: #F44336; color: white; padding: 5px 15px; border-radius: 4px;");

    btnLayout->addStretch();
    btnLayout->addWidget(btnConnect);
    btnLayout->addWidget(btnDisconnect);
    layout->addRow(btnLayout);

    connect(btnConnect, &QPushButton::clicked, this, [this](){
        m_action = 1;
        accept();
    });

    connect(btnDisconnect, &QPushButton::clicked, this, [this](){
        m_action = 2;
        accept();
    });

    adjustSize();
    setFixedSize(size());
}

QString ConnectionDialog::getIp() const { return m_ipEdit->text(); }
void ConnectionDialog::setIp(const QString& ip) { m_ipEdit->setText(ip); }
int ConnectionDialog::getAction() const { return m_action; }
