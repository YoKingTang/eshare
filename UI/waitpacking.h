#ifndef WAITPACKING_H
#define WAITPACKING_H

#include <QDialog>
#include <future>
#include <thread>
#include <memory>

namespace Ui {
class WaitPacking;
}

class MainWindow;

class WaitPacking : public QDialog
{
  Q_OBJECT

public:
  WaitPacking(QString dir, QString packed_dir, MainWindow *parent = 0);

  int exec() Q_DECL_OVERRIDE;

private slots:
  void on_cancelButton_clicked();

private:
  Ui::WaitPacking *ui;
  QString m_dir;
  QString m_packed_dir;

  std::unique_ptr<std::thread> m_thread;
  std::future<void> m_future;
  bool m_cancelled = false;
  std::promise<void> m_promise;
};

#endif // WAITPACKING_H
