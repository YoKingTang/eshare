#ifndef CHUNKER_H
#define CHUNKER_H

#include <QString>
#include <QFile>

enum Chunker_Mode {RECEIVER, SENDER};

class Chunker : public QObject
{
  Q_OBJECT

public:
  Chunker(Chunker_Mode mode, qint64 expected_size = 0);
  ~Chunker();

  bool open(QString file);
  bool is_open() const;
  void close();
  bool reached_eof() const;

  qint64 get_file_size() const;
  bool reached_expected_eof() const;
  qint64 get_next_chunk_size() const;
  // Get a filesize in human-readable form, e.g. 1.2 GB
  static QString format_size_human(qint64 num);

  qint64 chunk_size() const;

  QByteArray read_next_chunk();
  void write_next_chunk(const QByteArray& chunk);
  void move_ptr_back(qint64 bytes);

private:
  Chunker_Mode m_mode;
  qint64 m_expected_size; // Only useful in RECEIVER mode
  QFile m_file;
};

#endif // CHUNKER_H
