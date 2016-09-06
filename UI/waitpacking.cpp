#include <UI/WaitPacking.h>
#include "ui_WaitPacking.h"
#include <JlCompress.h>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <future>

WaitPacking::WaitPacking(QString dir, QString packed_dir, MainWindow *parent) :
  m_dir(dir),
  m_packed_dir(packed_dir),
  QDialog((QWidget*)parent),
  ui(new Ui::WaitPacking)
{
  auto flags = this->windowFlags();
  flags = flags & ~(Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint |
                    Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint |
                    Qt::WindowTitleHint);
  this->setWindowFlags(flags);

  ui->setupUi(this);

  QPixmap box(":/Res/box.png");
  ui->box->setPixmap(box);
}

int WaitPacking::exec()
{
  this->show();

  m_future = m_promise.get_future();
  m_thread = std::make_unique<std::thread>([this](QString dir, QString packed_dir) {

     JlCompress::compressDir(packed_dir, dir, true /* Recursive */);
     this->m_promise.set_value();

  }, m_dir, m_packed_dir);

  m_thread->detach();

  QCoreApplication::processEvents();

  auto status = std::future_status::deferred;
  do
  {
    if (m_future.valid())
      status = m_future.wait_for(std::chrono::milliseconds(0));
    QCoreApplication::processEvents();
  } while(status != std::future_status::ready && !m_cancelled);

  if (!m_cancelled)
    return QDialog::Accepted;
  else
    return QDialog::Rejected;
}

#ifdef _WIN32
#include "Windows.h"
#endif

void WaitPacking::on_cancelButton_clicked()
{
    auto reply = QMessageBox::question(this, "Conferma di annullamento", "Non è raccomandato annullare "
                                       "una operazione di packing. Si è proprio sicuri di voler proseguire?",
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No)
      return;

    m_cancelled = true;

    if (m_future.valid() &&
        m_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {        
#ifdef _WIN32

        TerminateThread(m_thread->native_handle(), 0);
#else
        throw std::runtime_error("NOT IMPLEMENTED");
#endif        
    }

    qDebug() << "Packing operation TERMINATED" << m_packed_dir;
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(4s);
    if (QFile::exists(m_packed_dir))
      QFile(m_packed_dir).remove();
}
