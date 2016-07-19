#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QSet>
#include <vector>
#include <tuple>
#include <memory>

namespace Ui {
class MainWindow;
}
class PeersView;
class TransfersView;

enum ClientSocketState {
  CONTACTED_HOST_FOR_PING, // A socket has contacted a host for ping and is now waiting for connection
  WAITING_FOR_PONG         // "PING?" has been sent, "PONG" is awaited
};

enum ServerSocketState {
  IDLE                     // Any message received will contain a directive
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

    // The peers we can connect to
    std::vector<std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>>
      m_peers;

    // Hashmap for fast peer-checking (address only ~ client ports can change)
    QSet<QString /* Ip */> m_registeredPeers;

    std::unique_ptr<QTimer> m_peersPingTimer;

    int m_localPort = 66; // The port this app should listen on
    QTcpServer m_tcpServer;
    QSet<QTcpSocket*> m_tcpServerConnections;

    std::vector<std::unique_ptr<QTcpSocket>> m_peersClientSockets; // Sockets used when pinging and contacting to initiate transfers

private slots:
    void pingAllPeers();
    void socketConnected();
    void updateClientProgress();
    void socketError(QAbstractSocket::SocketError);

    void acceptConnection();
    void updateServerProgress();
    void serverSocketError(QAbstractSocket::SocketError);

private:


    struct Job {
        //TorrentClient *client;
        QString torrentFileName;
        QString destinationDirectory;
    };
    QList<Job> jobs;
};

#endif // MAINWINDOW_H
