#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Data/TransferRequest.h>
#include <UI/TransferTreeView.h>
#include <Listener/TransferListener.h>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMutex>
#include <QVector>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <tuple>
#include <memory>

namespace Ui {
  class MainWindow;
}
class PeersView;
class TransfersView;
class WaitPacking;
class TransferStarter;

QT_BEGIN_NAMESPACE
class QNetworkSession;
QT_END_NAMESPACE

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
    QNetworkSession *m_network_session = nullptr;

    TransferTreeView *m_sentView;
    QMenu *m_sentViewMenu;
    TransferTreeView *m_receivedView;
    QTreeWidget *m_peersView;
    QStringList m_peers_completion_list; // A list of words from peers data for autocompletion

    void read_peers_from_file();
    void initialize_peers();
    void initialize_servers();
    void initialize_peers_ping();
    std::unique_ptr<QTimer> m_peers_ping_timer;

    std::vector<bool> m_peer_answered_last_ping;
    std::vector<bool> m_peer_online;
public:
    bool is_peer_active(size_t index) const {
      return m_peer_online[index];
    }
private:

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

    // Service server for incoming ping requests and transfer requests to this endpoint
    QTcpServer m_service_server;

    // Service socket for outgoing transfer requests to a remote endpoint
    QTcpSocket m_service_socket;

    // All pending transfer requests from external endpoints
    QVector<TransferRequest> m_external_transfer_requests;
    void add_new_external_transfer_requests(QVector<TransferRequest> reqs);

    // A map which stores the association between a local folder and a zip file of the same folder
    // into the temporary directory (used by the sender)
    QMutex m_folder_to_packed_mutex;
    QMap<QString, QString> m_folder_to_packed;
    QString packed_retriever(QString file);
    void packed_cleanup(QString file);

    // All sent pending transfer requests
    QMutex m_my_transfer_requests_mutex;
    QVector<TransferRequest> m_my_transfer_requests;
    void add_new_my_transfer_requests(QString receiver, QVector<TransferRequest> reqs);
    QVector<TransferRequest> m_my_pending_requests_to_send; // Temporary storage for requests not yet sent
                                                            // during drag/drop event processing    

    // Transfer server for incoming pending requests
    std::unique_ptr<TransferListener> m_transfer_listener;
    // Retrieves a 'my transfer' request from the id. Also retrieves the associated listview item pointer
    bool my_transfer_retriever(TransferRequest& req);
    QVector<TransferStarter*> m_running_transfer_starters;

    void pickreceiver_and_send(QStringList files);
    QString form_local_destination_file(TransferRequest& req) const;
    QString get_peer_name_from_address(QString address) const;

private slots:
    void process_new_connection();
    void server_ready_read();
    void server_socket_error(QAbstractSocket::SocketError err);

    void temporary_service_socket_connected();
    void temporary_service_socket_error(QAbstractSocket::SocketError err);
    void service_socket_error(QAbstractSocket::SocketError err);
    void listview_transfer_accepted(QModelIndex index);
    void file_received(TransferRequest req);
    void sent_view_custom_context_menu(QPoint point);
    void sent_view_send_file(bool);
    void sent_view_send_folder(bool);

    void ping_peers();
    void ping_failed(QAbstractSocket::SocketError err);
    void ping_socket_connected();
    void ping_socket_ready_read();
    void network_session_opened();

    void clear_sent(bool);
    void clear_received(bool);

    void hide_main_window();
    void show_main_window();

private:

    QSystemTrayIcon *m_tray_icon = nullptr;
    void initialize_tray_icon();

    QMenu *m_tray_icon_menu = nullptr;
      QAction *m_open_action = nullptr;
      QAction *m_quit_action = nullptr;

    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    bool nativeEvent(const QByteArray&, void* msg, long* result);

    std::unique_ptr<WaitPacking> m_wait_packing_window;

private slots:
    void tray_icon_activated(QSystemTrayIcon::ActivationReason reason);
public slots:
    // Sending transfers are harder to deal with since we're accepting them asynchronously
    // and associating them with a new socket instance
    void update_progress_sender(quint64 transfer_unique_id, int progress);
};

#endif // MAINWINDOW_H
