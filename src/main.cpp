#include <QApplication>
#include <QMetaType>
#include "core/GitTypes.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    qRegisterMetaType<Git::CommitNode>("Git::CommitNode");
    qRegisterMetaType<std::vector<Git::CommitNode>>("std::vector<Git::CommitNode>");
    qRegisterMetaType<Git::ConnectionPathType>("Git::ConnectionPathType");
    qRegisterMetaType<Git::CommitDetails>("Git::CommitDetails");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("GitVisualizer"));
    app.setOrganizationName(QStringLiteral("GitVisualizerDev"));

    // Qualify MainWindow with the Git namespace
    Git::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
