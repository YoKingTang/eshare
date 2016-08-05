#ifndef WAITPACKING_H
#define WAITPACKING_H

#include <QDialog>

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

private:
  Ui::WaitPacking *ui;
  QString m_dir;
  QString m_packed_dir;
};

#endif // WAITPACKING_H
