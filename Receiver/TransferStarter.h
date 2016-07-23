#ifndef TRANSFERSTARTER_H
#define TRANSFERSTARTER_H

#include <Chunker/Chunker.h>
#include <Data/TransferRequest.h>
#include <QThread>
#include <QTcpSocket>
#include <memory>

class TransferStarter;

// The main transfer starter - acknowledges an external pending
// transfer and starts the real transfer from the remote endpoint
// to a local file on this endpoint
class StarterSocketWrapper : public QObject {
  Q_OBJECT
public:
  StarterSocketWrapper(TransferStarter& parent);

private:
  friend class TransferStarter;
  TransferStarter& m_parent;

  QTcpSocket m_socket;

  std::unique_ptr<Chunker> m_chunker;

  qint64 m_bytes_to_read = 0;
  qint64 m_bytes_read = 0;

  void send_chunk_ACK();

private slots:
  void new_transfer_connection();
  void socket_error(QAbstractSocket::SocketError err);
  void transfer_bytes_written(qint64);
  void transfer_ready_read();

signals:
  void update_percentage(int);
};

class TransferStarter : public QThread {
  Q_OBJECT

  friend class StarterSocketWrapper;
public:

  TransferStarter(TransferRequest req, QString local_file);


private:
  void run() Q_DECL_OVERRIDE;

  TransferRequest m_request;
  QString         m_local_file;

private slots:
  void update_percentage_slot(int value);

signals:
  void update_percentage(int);
};

#endif // TRANSFERSTARTER_H
