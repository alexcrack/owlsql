#include "credentialsinputdialog.h"
#include "ui_credentialsinputdialog.h"

CredentialsInputDialog::CredentialsInputDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CredentialsInputDialog)
{
    ui->setupUi(this);
    ui->frame->setStyleSheet("background: white;");

    connect(this, &CredentialsInputDialog::accepted, this, [=]() {
        userName = ui->inputUsername->text();
        password = ui->inputPassword->text();
    });
}

CredentialsInputDialog::~CredentialsInputDialog()
{
    delete ui;
}

void CredentialsInputDialog::forSession(SessionTreeItem *item)
{
    ui->labelTitle->setText(QString(tr("Connecting to session \"%1\" (%2)")).arg(item->name()).arg(item->hostName()));
}
