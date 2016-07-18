#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSet>
#include <vector>
#include <tuple>
#include <memory>

namespace Ui {
class MainWindow;
}
class PeersView;
class TransfersView;

enum SocketState {
  CONTACTED_HOST_FOR_PING // A socket has contacted a host for ping and is now waiting for connection
};

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

    void initializePeers();
    void initializeServer();
    void readPeersList();
    void pingAllPeers();

    // The peers we can connect to
    std::vector<std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>>
      m_peers;

    // Hashmap for fast peer-checking (address only ~ client ports can change)
    QSet<QString /* Ip */> m_registeredPeers;

    int m_localPort = 66; // The port this app should listen on
    QTcpServer m_tcpServer;
    QTcpSocket *m_tcpServerConnection = nullptr;

    std::vector<std::unique_ptr<QTcpSocket>> m_peersClientSockets; // Sockets used when pinging and contacting to initiate transfers

private slots:
    void socketConnected();
    void socketError(QAbstractSocket::SocketError);

    void acceptConnection();

private:


    struct Job {
        //TorrentClient *client;
        QString torrentFileName;
        QString destinationDirectory;
    };
    QList<Job> jobs;
};

#endif // MAINWINDOW_H
