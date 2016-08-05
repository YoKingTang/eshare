#include <UI/WaitPacking.h>
#include "ui_WaitPacking.h"
#include <JlCompress.h>
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

  auto future = std::async(std::launch::async, [](QString dir, QString packed_dir) {

     JlCompress::compressDir(packed_dir, dir, true /* Recursive */);

  }, m_dir, m_packed_dir);

  std::future_status status;
  do
  {
    status = future.wait_for(std::chrono::milliseconds(0));
    QCoreApplication::processEvents();
  } while(status != std::future_status::ready);

  return QDialog::Accepted;
}
