#ifndef REQUESTLISTENER_H
#define REQUESTLISTENER_H

#include <Data/TransferRequest.h>
#include <QThread>
#include <QTcpServer>
#include <memory>
#include <functional>

class TransferListener;
class MainWindow;
class Chunker;

// The main transfer listener - identifies incoming acknowledges
// according to the list of local pending transfers and start the
// real transfer from this endpoint to the remote one
class ListenerSocketWrapper : public QObject {
  Q_OBJECT
public:
  explicit ListenerSocketWrapper(TransferListener& parent);
  ~ListenerSocketWrapper();

private:
  friend class TransferListener;
  TransferListener& m_parent;

  QTcpServer m_server;
  QVector<Chunker*> m_allocated_chunkers;
  QVector<QTcpSocket*> m_running_senders;

private slots:
  void new_transfer_connection();
  void socket_ready_read();
  void socket_error(QAbstractSocket::SocketError err);
  // Invoked when a cleanup (e.g. clear all sending transfers) is requested
  void abort_all_running_senders_slot();
  void cleanup_disconnected_socket();

signals:
  void all_senders_aborted();
  void update_progress(quint64 transfer_unique_id, int progress);
};

class TransferListener : public QThread {
  Q_OBJECT

  friend class ListenerSocketWrapper;
public:

  explicit TransferListener(MainWindow *main_win,
                   std::function<bool(TransferRequest&)> trans_retriever,
                   std::function<QString(QString)> packed_retriever,
                   std::function<void(QString)> packed_cleanup);

  void set_transfer_port(int local_transfer_port);

private:
  void run() Q_DECL_OVERRIDE;

  MainWindow *m_main_win = nullptr;
  std::function<bool(TransferRequest&)> m_trans_retriever; // Needs to be thread-safe
  std::function<QString(QString folder)> m_packed_retriever; // Needs to be thread-safe
  std::function<void(QString)> m_packed_cleanup; // Needs to be thread-safe

  int m_local_transfer_port = 67;

signals:
  void abort_all_running_senders(); // Send to children
  void all_senders_aborted(); // Feedback - Send to parents
  void update_progress(quint64 transfer_unique_id, int progress);

public slots:
  void update_progress_slot(quint64 transfer_unique_id, int progress);
  void all_senders_aborted_slot();
};

#endif // REQUESTLISTENER_H
