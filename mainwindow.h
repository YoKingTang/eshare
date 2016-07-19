#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "peerfiletransfer.h"
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <vector>
#include <tuple>
#include <memory>

namespace Ui {
class MainWindow;
}
class PeersView;
class TransfersView;

enum PingClientSocketState {
  CONTACTED_HOST_FOR_PING, // A socket has contacted a host for ping and is now waiting for connection
  WAITING_FOR_PONG,        // "PING?" has been sent, "PONG" is awaited
  DONE                     // Acknowledged
};

enum ListeningServerSocketState {
  IDLE                     // Any message received will contain a directive
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();  

protected:

  void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
  void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
  void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private:
    Ui::MainWindow *ui;

    TransfersView *m_sentView;
    TransfersView *m_receivedView;
    PeersView *m_peersView;
    QStringList m_peersCompletionList; // A list of words from peers data for autocompletion

    void initializePeers();
    void initializeServer();
    void readPeersListAndSettings();
    void processSendFiles(const QStringList files);

    // The peers we can connect to
    std::vector<std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>>
      m_peers;

    // Hashmap for fast peer-checking (address only ~ client ports can change)
    QMap<QString /* Ip */, size_t /* index of peer */> m_registeredPeers;

    // Static ping data challenges
    static const char PING[];
    static const char PONG[];

    std::unique_ptr<QTimer> m_peersPingTimer;
    std::vector<bool> m_isPeerOnline;
public:
    inline bool isPeerActive(int index) {
      return m_isPeerOnline[index];
    }
private:

    int m_localPort = 66; // The port this app should listen on
    QTcpServer m_tcpServer;
    QSet<QTcpSocket*> m_tcpServerConnections;

    std::vector<std::unique_ptr<QTcpSocket>> m_peersClientPingSockets; // Sockets used when pinging peers

    // File transfer wrappers - heavy load
    std::vector<std::unique_ptr<PeerFileTransfer>> m_transfers;

private slots:
    void pingAllPeers();
    void socketConnected();
    void updateClientProgress();
    void socketError(QAbstractSocket::SocketError);

    void acceptConnection();
    void updateServerProgress();
    void serverSocketError(QAbstractSocket::SocketError);

private:
    QString m_defaultDownloadPath;
public:
    QString getDownloadPath() const;
};

#endif // MAINWINDOW_H
