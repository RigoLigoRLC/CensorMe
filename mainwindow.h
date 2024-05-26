#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "defs.h"
#include <QMainWindow>
#include <QButtonGroup>
#include <QFileSystemModel>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct MetaConfig {
    int chunkSize;
    CensorType method;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void previewModeButtonGroupIdClicked(int id);
    void currentSelectedFileChanged(const QModelIndex &current, const QModelIndex &previous);

    void on_actOpenOneImg_triggered();

    void on_sliderChunkSize_sliderMoved(int action);

    void on_sliderBrushSize_sliderMoved(int position);

    void on_actOpenFolder_triggered();

    void on_btnToggleFileList_toggled(bool checked);

    void on_btnSave_clicked();

    void on_btnRestore_clicked();

private:
    void switchToImage(QString absPath);
    void setCensorMaskEdited(bool);
    void setOperatingInFolderMode(bool);
    bool takeMaskAndMetadataForImage(QString absPath, QImage &maskOut, MetaConfig &metaOut);
    void reloadMaskAndMetadataForImage();
    bool isAnyImageOpened();
    bool ensureSaved();
    bool saveForFileModeEditedFile();
    bool saveForFolderModeEditedFile(QString filenameNoDir);

private:
    Ui::MainWindow *ui;
    QLabel* ui_lblPath;
    QLabel* ui_lblEdited;
    QFrame* ui_vline1;

    QString m_fileModeFileAbsPath;
    QString m_dirModeDirAbsPath;
    QString m_folderModeFileNameNoDir;

    QFileSystemModel m_fsModel;
    QButtonGroup m_previewModeGroup;
    bool m_isNowOperatingInFolderMode;
    bool m_censorMaskEdited;
    bool m_autoSaveOnSwitching;
};
#endif // MAINWINDOW_H
