#include "mainwindow/mainwindow.h"
#include <QApplication>
#include "consts.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName(APP_DEVELOPER);
    QCoreApplication::setOrganizationDomain(APP_DOMAIN);

//    QFile f(":/qdarkstyle/style.qss");
//    if (!f.exists()) {
//        printf("Unable to set stylesheet, file not found\n");
//    } else {
//        f.open(QFile::ReadOnly | QFile::Text);
//        QTextStream ts(&f);
//        a.setStyleSheet(ts.readAll());
//    }

    MainWindow w;
    w.show();

    return a.exec();
}
