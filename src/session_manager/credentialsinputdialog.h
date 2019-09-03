#ifndef CREDENTIALSINPUTDIALOG_H
#define CREDENTIALSINPUTDIALOG_H

#include <QDialog>
#include "sessiontreeitem.h"

namespace Ui {
class CredentialsInputDialog;
}

class CredentialsInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CredentialsInputDialog(QWidget *parent = nullptr);
    ~CredentialsInputDialog();

    QString userName;
    QString password;

    void forSession(SessionTreeItem *item);

private:
    Ui::CredentialsInputDialog *ui;
};

#endif // CREDENTIALSINPUTDIALOG_H
