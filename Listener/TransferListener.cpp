#include <Listener/TransferListener.h>
#include <UI/MainWindow.h>
#include <Chunker/Chunker.h>
#include <QMessageBox>
#include <QTcpSocket>
#include <QVariant>

// SENDER

#define CHECK_SOCKET_IO_SUCCESS(socketCall) do { \
                                              auto ret = socketCall; \
                                              if (ret == -1) { \
                                                qDebug() << "SOCKET ERROR at " << __FILE__ << ":" << __LINE__; \
                                              } \
                                            } while(0)

ListenerSocketWrapper::ListenerSocketWrapper(TransferListener& parent) :
  m_parent(parent)
{
  connect(&m_server, SIGNAL(newConnection()), this, SLOT(new_transfer_connection()));

  if (!m_server.listen(QHostAddress::Any, m_parent.m_local_transfer_port))
  {
    QMessageBox::critical(nullptr, tr("Server"),
                          tr("Errore durante l'inizializzazione del transfer server: '%1'\n\nL'applicazione sara' chiusa.")
                          .arg(m_server.errorString()));
    exit(1); // Cannot recover
  }
}

void ListenerSocketWrapper::new_transfer_connection() // SLOT
{
  if (m_server.hasPendingConnections() == false)
    return;

  auto new_socket_connection = m_server.nextPendingConnection();
  connect(new_socket_connection, SIGNAL(disconnected()), new_socket_connection, SLOT(deleteLater()));
  connect(new_socket_connection, SIGNAL(readyRead()), this, SLOT(socket_ready_read()));
  connect(new_socket_connection, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socket_error(QAbstractSocket::SocketError)));

  new_socket_connection->setProperty("status", "needs_identification");
}

void ListenerSocketWrapper::socket_ready_read() // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
  QString status = socket->property("status").toString();
  QDataStream stream(socket);
  stream.setVersion(QDataStream::Qt_5_7);

  if (status == "needs_identification")
  {
    quint64 req_id;
    stream.startTransaction();
    stream >> req_id;
    if (!stream.commitTransaction()) {
      stream.rollbackTransaction();
      return; // Wait for more data
    }

    TransferRequest req;
    req.m_unique_id = req_id;

    DynamicTreeWidgetItem *associated_item = nullptr;
    if (!m_parent.m_trans_retriever(req, associated_item))
    {
      qDebug() << "[socket_ready_read] Error: we were given permission for a transfer we didn't ask";
      socket->abort();
    }

    Chunker *chunker = new Chunker(Chunker_Mode::SENDER);
    if (!chunker->open(req.m_file_path))
    {
      qDebug() << "[socket_ready_read] Error: could not open file '" << req.m_file_path << "'";
      socket->abort();
    }

    connect(socket, SIGNAL(disconnected()), chunker, SLOT(deleteLater()));
    socket->setProperty("listview_item", QVariant::fromValue(associated_item));
    socket->setProperty("chunker", QVariant::fromValue(chunker));

    socket->setProperty("status", "awaiting_chunk_ack");

    if (chunker->reached_eof())
    {
      // 0-bytes file, wait ack and exit
      return;
    }

    CHECK_SOCKET_IO_SUCCESS(socket->write(chunker->read_next_chunk()));
  }
  else if (status == "awaiting_chunk_ack")
  {
    QString ack;
    stream.startTransaction();
    stream >> ack;
    if (!stream.commitTransaction()) {
      stream.rollbackTransaction();
      return; // Wait for more data
    }

    if (ack == "ACK_CHUNK")
    {
      Chunker *chunker = socket->property("chunker").value<Chunker*>();
      DynamicTreeWidgetItem *listview_item = socket->property("listview_item").value<DynamicTreeWidgetItem*>();

      if (chunker->reached_eof())
      {
        // Transfer finished
        QMetaObject::invokeMethod(listview_item, "update_percentage", Q_ARG(int, 100));
        socket->flush();
        socket->disconnectFromHost();
        return;
      }

      if (listview_item)
      {
        int percentage = ((float)chunker->get_pos() / (float)chunker->get_file_size()) * 100;
        QMetaObject::invokeMethod(listview_item, "update_percentage", Q_ARG(int, percentage));
      }

      CHECK_SOCKET_IO_SUCCESS(socket->write(chunker->read_next_chunk()));
    }
  }
}

void ListenerSocketWrapper::socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  if (socket->property("status").toString() == "awaiting_chunk_ack" &&
      socket->property("chunker").value<Chunker*>()->reached_eof())
  {
    return; // Transfer was done anyway
  }

  qDebug() << "[socket_error] Error: " << err;
  socket->abort();
}

TransferListener::TransferListener(MainWindow *main_win, std::function<bool(TransferRequest&, DynamicTreeWidgetItem*&)> trans_retriever) :
  m_main_win(main_win),
  m_trans_retriever(trans_retriever)
{}

void TransferListener::set_transfer_port(int local_transfer_port)
{
  m_local_transfer_port = local_transfer_port;
}

void TransferListener::run() // Main thread entry point
{
  // Will be deleted as soon as this scope returns
  std::unique_ptr<ListenerSocketWrapper> m_sw = std::make_unique<ListenerSocketWrapper>(*this);

  this->exec();
}
