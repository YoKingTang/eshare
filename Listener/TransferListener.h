#ifndef REQUESTLISTENER_H
#define REQUESTLISTENER_H

#include <Data/TransferRequest.h>
#include <QThread>
#include <QTcpServer>
#include <memory>
#include <functional>

class TransferListener;

// The main transfer listener - identifies incoming acknowledges
// according to the list of local pending transfers and start the
// real transfer from this endpoint to the remote one
class ListenerSocketWrapper : public QObject {
  Q_OBJECT
public:
  ListenerSocketWrapper(TransferListener& parent);

private:
  friend class TransferListener;
  TransferListener& m_parent;

  QTcpServer m_server;
  QSet<QTcpSocket*> m_connections;

private slots:
  void new_transfer_connection();
  void socket_ready_read();
  void socket_error(QAbstractSocket::SocketError err);
};

class TransferListener : public QThread {
  Q_OBJECT

  friend class ListenerSocketWrapper;
public:

  TransferListener(std::function<bool(TransferRequest&)> trans_retriever);

  void set_transfer_port(int local_transfer_port);

private:
  void run() Q_DECL_OVERRIDE;

  std::function<bool(TransferRequest&)> m_trans_retriever; // Needs to be thread-safe

  int m_local_transfer_port = 67;
};

#endif // REQUESTLISTENER_H
