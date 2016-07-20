#ifndef FILECHUNKER_H
#define FILECHUNKER_H

#include <QString>
#include <QFile>

enum RWMode {READONLY, READWRITE};

class FileChunker : public QObject
{
  Q_OBJECT

public:
  FileChunker(QString file = "");
  ~FileChunker();

  bool open(RWMode mode = READONLY);
  void close();
  bool reachedEOF() const;

  qint64 getFileSize() const;

  qint64 chunkSize() const;

  QByteArray readNextFileChunk();
  void writeNextFileChunk(const QByteArray& chunk);
  void movePointerBack(qint64 bytes);

private:
  QFile m_file;
};

#endif // FILECHUNKER_H
