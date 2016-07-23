#include <Chunker/Chunker.h>
#include <QDebug>
#include <algorithm>

static const qint64 CHUNK_SIZE = 50 * 1024 * 1024;

Chunker::Chunker(Chunker_Mode mode, qint64 expected_size) :
  m_mode(mode),
  m_expected_size(expected_size)
{}

Chunker::~Chunker()
{
  if (m_file.isOpen())
    m_file.close();
}

bool Chunker::open(QString file)
{
  if (m_file.isOpen() || file.isEmpty())
    return false;
  m_file.setFileName(file);
  return m_file.open((m_mode == SENDER) ? QIODevice::ReadOnly : QIODevice::ReadWrite);
}

bool Chunker::is_open() const
{
  return m_file.isOpen();
}

void Chunker::close()
{
  if (m_file.isOpen())
    m_file.close();
}

qint64 Chunker::get_file_size() const
{
  if (!is_open())
    return 0;

  return m_file.size();
}

bool Chunker::reached_expected_eof() const
{
  if (m_mode != RECEIVER)
    qDebug() << "[reached_expected_eof] Warning: calling receiver function from sender mode";

  return m_file.pos() >= m_expected_size;
}

qint64 Chunker::get_next_chunk_size() const
{
  qint64 total_size = (m_mode == SENDER) ? get_file_size() : m_expected_size;
  qint64 remaining_bytes = total_size - m_file.pos();
  return std::min(remaining_bytes, chunk_size());
}

QString Chunker::format_size_human(qint64 num)
{
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

qint64 Chunker::chunk_size() const
{
  return CHUNK_SIZE;
}

bool Chunker::reached_eof() const
{
  return m_file.atEnd();
}

QByteArray Chunker::read_next_chunk()
{
  return m_file.read(CHUNK_SIZE);
}

void Chunker::write_next_chunk(const QByteArray& chunk)
{
  m_file.write(chunk);
}

void Chunker::move_ptr_back(qint64 bytes)
{
  m_file.seek(m_file.pos() - bytes);
}
