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

void FileChunker::close() {
  if (m_file.isOpen())
    m_file.close();
}

qint64 FileChunker::getFileSize() const {
  return m_file.size();
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
