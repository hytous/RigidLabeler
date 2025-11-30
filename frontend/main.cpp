#include "mainwindow.h"

#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QThread>

static QProcess *g_backendProcess = nullptr;

void startBackend()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString backendPath = appDir + "/backend/rigidlabeler_backend/rigidlabeler_backend.exe";
    
    // Check if backend executable exists (we're in installed mode)
    if (QFile::exists(backendPath)) {
        g_backendProcess = new QProcess();
        g_backendProcess->setWorkingDirectory(appDir + "/backend/rigidlabeler_backend");
        g_backendProcess->start(backendPath);
        // Wait a bit for backend to start
        QThread::msleep(1500);
    }
    // If not exists, assume backend is running separately (development mode)
}

void stopBackend()
{
    if (g_backendProcess) {
        g_backendProcess->terminate();
        if (!g_backendProcess->waitForFinished(3000)) {
            g_backendProcess->kill();
        }
        delete g_backendProcess;
        g_backendProcess = nullptr;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // Start backend if in installed mode
    startBackend();
    
    MainWindow w;
    w.show();
    int result = a.exec();
    
    // Stop backend when frontend closes
    stopBackend();
    
    return result;
}
