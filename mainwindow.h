#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
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

//  void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
//  void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
//  void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private:
    Ui::MainWindow *ui;

    TransfersView *m_sentView;
    TransfersView *m_receivedView;
    QTreeWidget *m_peersView;

    void read_peers_from_file();
    void initialize_peers();
    void initialize_server();

    int m_ping_port = 66; // Port used for service communications
    int m_transfer_port = 67; // Port used for file transfers
    QString m_default_download_path;

    // The peers we can connect to
    std::vector<std::tuple<QString  /* Ip */,
                           int     /* ping port */,
                           int     /* transfer port */,
                           QString /* hostname */>> m_peers;
};

#endif // MAINWINDOW_H
