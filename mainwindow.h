#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QFileSystemModel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void previewModeButtonGroupIdClicked(int id);

    void on_actOpenOneImg_triggered();

    void on_sliderChunkSize_sliderMoved(int action);

    void on_sliderBrushSize_sliderMoved(int position);

    void on_actOpenFolder_triggered();

    void on_btnToggleFileList_toggled(bool checked);

private:
    void setOperatingInFolderMode(bool);

private:
    Ui::MainWindow *ui;

    QFileSystemModel m_fsModel;
    QButtonGroup m_previewModeGroup;
    bool m_isNowOperatingInFolderMode;
};
#endif // MAINWINDOW_H
