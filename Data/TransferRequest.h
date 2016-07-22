#ifndef TRANSFERREQUEST_H
#define TRANSFERREQUEST_H

#include <QString>

// Represents a transfer request towards this endpoint which might be
// accepted or rejected.
struct TransferRequest
{
  quint64 m_unique_id; // A unique id to identify this transfer
  QString m_file_path; // The entire file path and name on the remote host
  qint64  m_size;
  QString m_sender_address;
  int     m_sender_transfer_port;

  // Generate a unique transfer request
  static TransferRequest generate_unique() {
    static quint64 id = 0;
    TransferRequest t;
    t.m_unique_id = id;
    ++id;
    return t;
  }
};

#endif // TRANSFERREQUEST_H
