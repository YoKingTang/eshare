#include <Receiver/TransferStarter.h>
#include <QDataStream>
#include <JlCompress.h>

// RECEIVER

#define CHECK_SOCKET_IO_SUCCESS(socketCall) do { \
                                              auto ret = socketCall; \
                                              if (ret == -1) { \
                                                qDebug() << "SOCKET ERROR at " << __FILE__ << ":" << __LINE__; \
                                              } \
                                            } while(0)

StarterSocketWrapper::StarterSocketWrapper(TransferStarter& parent) :
  m_parent(parent)
{
  connect(&m_socket, SIGNAL(connected()), this, SLOT(new_transfer_connection()));
  connect(&m_socket, SIGNAL(bytesWritten(qint64)), SLOT(transfer_bytes_written(qint64)));
  connect(&m_socket, SIGNAL(readyRead()), SLOT(transfer_ready_read()));
  connect(&m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(socket_error(QAbstractSocket::SocketError)));

  if (parent.m_request.m_size == -1)
    m_unpack_after_transfer = true;
  else
    m_unpack_after_transfer = false;

  m_chunker = std::make_unique<Chunker>(RECEIVER, m_unpack_after_transfer ? parent.m_request.m_packed_size :
                                                                            parent.m_request.m_size);
  if (!m_chunker || !m_chunker->open(parent.m_local_file))
  {
    qDebug() << "[StarterSocketWrapper] Could not open '" << parent.m_local_file << "' aborting";
    m_parent.exit(1);
    return;
  }

  connect(this, SIGNAL(update_percentage(int)), &m_parent, SLOT(update_percentage_slot(int)));

  m_socket.connectToHost(parent.m_request.m_sender_address, parent.m_request.m_sender_transfer_port);
}

void StarterSocketWrapper::send_chunk_ACK()
{
  QString command = "ACK_CHUNK";
  QByteArray block;
  QDataStream stream(&block, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_7);
  stream << command;
  CHECK_SOCKET_IO_SUCCESS(m_socket.write(block));
}

void StarterSocketWrapper::new_transfer_connection() // SLOT
{
  qDebug() << "[new_transfer_connection] Connected, sending request id we're allowing";

  QByteArray block;
  QDataStream stream(&block, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_7);

  stream << m_parent.m_request.m_unique_id;

  m_socket.setProperty("status", "receiving_chunks");

  if (m_chunker->reached_expected_eof())
  {
    // 0-bytes file
    send_chunk_ACK();
    m_socket.flush();
    m_socket.disconnectFromHost();
    m_parent.exit(0);
    return;
  }

  m_bytes_to_read = m_chunker->get_next_chunk_size();
  m_bytes_read = 0;
  CHECK_SOCKET_IO_SUCCESS(m_socket.write(block));
}

void StarterSocketWrapper::socket_error(QAbstractSocket::SocketError err) // SLOT
{
  qDebug() << "[socket_error] Error: " << err;
  m_socket.abort();
}

void StarterSocketWrapper::transfer_bytes_written(qint64) // SLOT
{
  // DELETE IF UNUSED DEBUG
}

void StarterSocketWrapper::transfer_ready_read() // SLOT
{
  qint64 available_bytes = m_socket.bytesAvailable();
  QString status = m_socket.property("status").toString();

  if (status == "receiving_chunks")
  {
    if (available_bytes < m_bytes_to_read)
      return; // Wait for more data

    QByteArray chunk = m_socket.read(m_bytes_to_read);
    m_bytes_read += chunk.size();

    m_chunker->write_next_chunk(chunk);
    int percentage = ((float)m_chunker->get_pos() / (float)m_chunker->get_expected_file_size()) * 100;

    if (percentage < 100) // Set it to 100 only after any unpack has been done
      emit update_percentage(percentage);

    if (m_chunker->reached_expected_eof())
    {
      // Transfer finished
      send_chunk_ACK();
      m_socket.flush();
      m_socket.waitForBytesWritten(2000); // We were no longer accepting bytes anyway
      qDebug() << "[transfer_ready_read] File correctly received";
      m_socket.disconnectFromHost();
      m_chunker->close();

      if (m_unpack_after_transfer)
      {
        QString destination_path = QFileInfo(m_parent.m_local_file).absolutePath();
        QString directory_name = QFileInfo(m_parent.m_local_file).fileName();
        directory_name.chop(4 /* .zip */);
        auto dir = QDir(destination_path);
        auto res = dir.mkdir(directory_name);
        if (!res)
          qDebug() << "[TransferStarter cleanup] error while creating directory '" << directory_name << "'";
        else
        {
          dir.cd(directory_name);
          JlCompress::extractDir(m_parent.m_local_file, dir.absolutePath());
          QFile zip(m_parent.m_local_file); // Cleanup zip
          res = zip.remove();
          if (!res)
            qDebug() << "[TransferStarter cleanup] error: " << zip.errorString();
        }
      }

      emit update_percentage(100);

      m_parent.exit(0);
      return;
    }

    m_bytes_to_read = m_chunker->get_next_chunk_size();
    m_bytes_read = 0;

    send_chunk_ACK();
  }
}

TransferStarter::TransferStarter(TransferRequest req, QString local_file) :
  m_request(req),
  m_local_file(local_file)
{}

void TransferStarter::run() // Main thread entry point
{
  // Will be deleted as soon as this scope returns
  std::unique_ptr<StarterSocketWrapper> m_sw = std::make_unique<StarterSocketWrapper>(*this);

  this->exec();
}

void TransferStarter::update_percentage_slot(int value) // SLOT
{
  emit update_percentage(value);
}
