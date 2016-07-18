#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}
class PeersView;
class TransfersView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    TransfersView *m_sentView;
    TransfersView *m_receivedView;
    PeersView *m_peersView;

    struct Job {
        //TorrentClient *client;
        QString torrentFileName;
        QString destinationDirectory;
    };
    QList<Job> jobs;
};

#endif // MAINWINDOW_H
