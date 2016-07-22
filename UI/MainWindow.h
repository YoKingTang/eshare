#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Data/TransferRequest.h>
#include <UI/TransferTreeView.h>
#include <QMainWindow>
#include <QMutex>
#include <QVector>
#include <QTcpServer>
#include <QTcpSocket>
#include <tuple>

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

protected:

//  void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
//  void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
//  void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private:
    Ui::MainWindow *ui;

    TransferTreeView *m_sentView;
    TransferTreeView *m_receivedView;
    QTreeWidget *m_peersView;

    void read_peers_from_file();
    void initialize_peers();
    void initialize_server();

    // ~-~-~-~-~-~-~-~- Local configuration data ~-~-~-~-~-~-~-~-~-~-
    int m_service_port = 66; // Port used for service communications
    int m_transfer_port = 67; // Port used for file transfers
    QString m_default_download_path;

    // The peers we can connect to
    std::vector<std::tuple<QString  /* Ip */,
                           int     /* ping port */,
                           int     /* transfer port */,
                           QString /* hostname */>> m_peers;
    // ~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-

    QString get_local_address();

    // Service server for ping requests and transfer requests to this endpoint
    QTcpServer m_service_server;

    // Service socket for ping requests and transfer requests to a remote endpoint
    QTcpSocket m_service_socket;

    // All pending transfer requests from external endpoints
    QVector<TransferRequest> m_external_transfer_requests;
    void add_new_external_transfer_request(TransferRequest req);

    QVector<TransferRequest> m_my_transfer_requests;
    void add_new_my_transfer_request(TransferRequest req);

private slots:
    void process_new_connection();
    void server_ready_read();
    void server_socket_error(QAbstractSocket::SocketError err);

    void service_socket_connected();
    void service_socket_read_ready();
    void service_socket_error(QAbstractSocket::SocketError err);

    void SIMULATE_SEND(bool);
};

#endif // MAINWINDOW_H
