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
  connect(this, SIGNAL(all_senders_aborted()), &m_parent, SLOT(all_senders_aborted_slot()));

  if (!m_server.listen(QHostAddress::Any, m_parent.m_local_transfer_port))
  {
    QMessageBox::critical(nullptr, tr("Server"),
                          tr("Errore durante l'inizializzazione del transfer server: '%1'\n\nL'applicazione sara' chiusa.")
                          .arg(m_server.errorString()));
    exit(1); // Cannot recover
  }
}

ListenerSocketWrapper::~ListenerSocketWrapper()
{
  abort_all_running_senders_slot();
}

void ListenerSocketWrapper::abort_all_running_senders_slot()
{
  for(auto& socket : m_running_senders)
    socket->close();
  m_running_senders.clear();
  emit all_senders_aborted();
}

void ListenerSocketWrapper::cleanup_disconnected_socket()
{

}

void ListenerSocketWrapper::new_transfer_connection() // SLOT
{
  if (m_server.hasPendingConnections() == false)
    return;

  auto new_socket_connection = m_server.nextPendingConnection();
  // Do not delete the object immediately when disconnected - wait for cleanup
  // connect(new_socket_connection, SIGNAL(disconnected()), new_socket_connection, SLOT(deleteLater()));
  connect(new_socket_connection, SIGNAL(readyRead()), this, SLOT(socket_ready_read()));
  connect(new_socket_connection, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socket_error(QAbstractSocket::SocketError)));
  connect(new_socket_connection, &QAbstractSocket::disconnected, this, [&]() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    auto index = m_running_senders.indexOf(socket);
    if (index != -1)
      m_running_senders.remove(index);
    auto chunker_property = socket->property("chunker");
    if (chunker_property.isValid())
    {
      Chunker *chunker = chunker_property.value<Chunker*>();
      auto index = m_allocated_chunkers.indexOf(chunker);
      if (index != -1)
      {
        m_allocated_chunkers.remove(index);
        chunker->close();
        delete chunker;
      }
    }
    socket->deleteLater();
  });

  new_socket_connection->setProperty("status", "needs_identification");
  m_running_senders.append(new_socket_connection);
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

    TransferRequest request;
    request.m_unique_id = req_id;

    if (!m_parent.m_trans_retriever(request))
    {
      qDebug() << "[socket_ready_read] Error: we were given permission for a transfer we didn't ask for";
      socket->disconnectFromHost();
      return;
    }
    // req now contains the transfer requests we did send. Save it along with the socket
    socket->setProperty("transfer_request", QVariant::fromValue(request));

    Chunker *chunker = new Chunker(Chunker_Mode::SENDER);
    m_allocated_chunkers.append(chunker);
    QString file_to_send = (request.m_size == -1) ? m_parent.m_packed_retriever(request.m_file_path) : request.m_file_path;
    if (!chunker->open(file_to_send))
    {
      qDebug() << "[socket_ready_read] Error: could not open file '" << file_to_send << "' (aka " << request.m_file_path << ")";
      socket->disconnectFromHost();
      return;
    }

    connect(socket, SIGNAL(disconnected()), chunker, SLOT(deleteLater()));
    //socket->setProperty("listview_item", QVariant::fromValue(associated_item));
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
    TransferRequest request = socket->property("transfer_request").value<TransferRequest>();

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
      //auto listview_item_property = socket->property("listview_item");
//      if (!listview_item_property.isValid() || !listview_item_property.canConvert<DynamicTreeWidgetItem*>())
//      {
//        // Element has been destroyed but socket hasn't been disconnected yet. Abort
//        socket->disconnectFromHost();
//        return;
//      }
      //DynamicTreeWidgetItem *listview_item = listview_item_property.value<DynamicTreeWidgetItem*>();

      if (chunker->reached_eof())
      {
        // Transfer finished
        emit update_progress(request.m_unique_id, 100);
//        if (listview_item)
//          QMetaObject::invokeMethod(listview_item, "update_percentage", Q_ARG(int, 100));
        socket->disconnectFromHost();

        if (request.m_size == -1)
          m_parent.m_packed_cleanup(request.m_file_path); // Cleanup zip

        return;
      }

      int percentage = ((float)chunker->get_pos() / (float)chunker->get_file_size()) * 100;
      emit update_progress(request.m_unique_id, percentage);


      CHECK_SOCKET_IO_SUCCESS(socket->write(chunker->read_next_chunk()));
    }
  }
}

void ListenerSocketWrapper::socket_error(QAbstractSocket::SocketError err) // SLOT
{
  QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

  Chunker *chunker = socket->property("chunker").value<Chunker*>();

  if (socket->property("status").toString() == "awaiting_chunk_ack" &&
      chunker->reached_eof())
  {
    return; // Transfer was done anyway
  }

  qDebug() << "[socket_error] Error: " << err;
  socket->disconnectFromHost();
}

TransferListener::TransferListener(MainWindow *main_win,
                                   std::function<bool(TransferRequest&)> trans_retriever,
                                   std::function<QString(QString)> packed_retriever, std::function<void(QString)> packed_cleanup) :
  m_main_win(main_win),
  m_trans_retriever(trans_retriever),
  m_packed_retriever(packed_retriever),
  m_packed_cleanup(packed_cleanup)
{}

void TransferListener::set_transfer_port(int local_transfer_port)
{
  m_local_transfer_port = local_transfer_port;
}

void TransferListener::run() // Main thread entry point
{
  // Will be deleted as soon as this scope returns
  std::unique_ptr<ListenerSocketWrapper> m_sw = std::make_unique<ListenerSocketWrapper>(*this);

  connect(m_sw.get(), SIGNAL(update_progress(quint64,int)), this, SLOT(update_progress_slot(quint64,int)));
  connect(this, SIGNAL(abort_all_running_senders()), m_sw.get(), SLOT(abort_all_running_senders_slot()));
  connect(this, SIGNAL(update_progress(quint64,int)), m_main_win, SLOT(update_progress_sender(quint64,int)));

  this->exec();
}

void TransferListener::all_senders_aborted_slot() // SLOT
{
  emit all_senders_aborted();
}

void TransferListener::update_progress_slot(quint64 transfer_unique_id, int progress) // SLOT
{
  emit update_progress(transfer_unique_id, progress);
}
