#include "filechunker.h"

static const qint64 CHUNK_SIZE = 50 * 1024 * 1024;

FileChunker::FileChunker(QString file) : m_file(file)
{}

FileChunker::~FileChunker() {
  if (m_file.isOpen())
    m_file.close();
}

bool FileChunker::open(RWMode mode) {
  return m_file.open((mode == READONLY) ? QIODevice::ReadOnly : QIODevice::ReadWrite);
}

bool FileChunker::isOpen() const {
  return m_file.isOpen();
}

void FileChunker::close() {
  if (m_file.isOpen())
    m_file.close();
}

qint64 FileChunker::getFileSize() const {
  if (!isOpen())
    return 0;

  return m_file.size();
}

QString FileChunker::formatSizeHuman(qint64 num) {
  // Simple adaptation from lists.qt-project.org/pipermail/qt-interest-old/2010-August/027043.html

  QStringList list;
  list << "KB" << "MB" << "GB" << "TB";

  QStringListIterator i(list);
  QString unit("bytes");

  double fnum = num;
  while (fnum >= 1024.0 && i.hasNext()) {
     unit = i.next();
     fnum /= 1024.0;
  }
  return QString().setNum(fnum,'f', 2)+" " + unit;
}

qint64 FileChunker::chunkSize() const {
  return CHUNK_SIZE;
}

bool FileChunker::reachedEOF() const {
  return m_file.atEnd();
}

QByteArray FileChunker::readNextFileChunk() {
  return m_file.read(CHUNK_SIZE);
}

void FileChunker::writeNextFileChunk(const QByteArray& chunk) {
  m_file.write(chunk);
}

void FileChunker::movePointerBack(qint64 bytes) {
  m_file.seek(m_file.pos() - bytes);
}
