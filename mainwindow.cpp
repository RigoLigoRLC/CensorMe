#include "defs.h"
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui_lblPath = new QLabel(this);
    ui_lblEdited = new QLabel(this);
    ui_vline1 = new QFrame(this);
    ui_vline1->setFrameShape(QFrame::VLine);
    ui_vline1->setFrameShadow(QFrame::Sunken);
    ui->statusbar->addWidget(ui_lblPath);
    ui->statusbar->addWidget(ui_vline1);
    ui->statusbar->addWidget(ui_lblEdited);

    ui->scrollArea->installEventFilter(ui->widCanvas); // Canvas needs to know resizes happened there

    m_previewModeGroup.addButton(ui->radPreviewOrig, PM_Original);
    m_previewModeGroup.addButton(ui->radPreviewFullyCensored, PM_FullyCensored);
    m_previewModeGroup.addButton(ui->radPreviewMask, PM_MaskOnly);
    m_previewModeGroup.addButton(ui->radPreviewMaskOnImg, PM_MaskOnImage);
    m_previewModeGroup.addButton(ui->radPreviewFinal, PM_FinalPreview);
    connect(&m_previewModeGroup, &QButtonGroup::idClicked, this, &MainWindow::previewModeButtonGroupIdClicked);
    ui->widCanvas->setPreviewMode(m_previewModeGroup.checkedId());

    m_fsModel.setFilter(QDir::Filter::Files | QDir::NoDotAndDotDot);
    m_fsModel.setNameFilters({"*.jpg", "*.jpeg", "*.png"});
    m_fsModel.setNameFilterDisables(false);

    ui->lstFileList->setModel(&m_fsModel);
    ui->lstFileList->setSelectionMode(QListView::SingleSelection);
    ui->lstFileList->setSelectionBehavior(QListView::SelectRows);
    connect(ui->lstFileList->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentSelectedFileChanged);

    setOperatingInFolderMode(false);

    m_censorMaskEdited = false;
    connect(ui->widCanvas, &CanvasWidget::censorMaskEdited, [&](){ setCensorMaskEdited(true); });

    m_autoSaveOnSwitching = ui->chkAutoSave->isChecked();
    connect(ui->chkAutoSave, &QCheckBox::stateChanged, [&](int s){ m_autoSaveOnSwitching = s; });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::previewModeButtonGroupIdClicked(int id)
{
    ui->widCanvas->setPreviewMode(id);
}

void MainWindow::currentSelectedFileChanged(const QModelIndex &current, const QModelIndex &previous)
{
    if (previous.isValid()) {
        if (!ensureSaved()) {
            disconnect(ui->lstFileList->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentSelectedFileChanged);
            ui->lstFileList->setCurrentIndex(previous);
            connect(ui->lstFileList->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentSelectedFileChanged);
            m_folderModeFileNameNoDir = "";
            return;
        }
    }

    QString file = m_dirModeDirAbsPath + QDir::separator() + m_fsModel.data(current).toString();
    qApp->setOverrideCursor(Qt::BusyCursor);
    QImage img(file);
    if (img.isNull()) {
        qApp->restoreOverrideCursor();
        QMessageBox::critical(this, tr("Failed to load image"), tr("Probably is not supported format"));
        ui->widCanvas->switchImage(QImage(), QImage(), {});
        m_folderModeFileNameNoDir = "";
        return;
    }

    QImage mask;
    MetaConfig meta;
    if (takeMaskAndMetadataForImage(file, mask, meta)) {
        ui->sliderChunkSize->setSliderPosition(meta.chunkSize);
    } else {
        meta.method = CensorType::CT_Pixelize;
        meta.chunkSize = ui->sliderChunkSize->value();
    }

    m_censorMaskEdited = false;
    m_folderModeFileNameNoDir = m_fsModel.data(current).toString();
    m_dirModeCurrentFileIndex = current.row();
    ui->widCanvas->switchImage(img, mask, meta);
    ui->lblImgCounter->setText(tr("%1/%2").arg(current.row() + 1).arg(m_dirModeFileCount));
    qApp->restoreOverrideCursor();
}

void MainWindow::on_actOpenOneImg_triggered()
{
    if (!ensureSaved()) {
        return;
    }

    auto file = QFileDialog::getOpenFileName(this,
                                             tr("Open one single image"),
                                             "",
                                             "JPEG/PNG Image (*.jpg;*.jpeg;*.png)");

    if (file.isEmpty()) {
        return;
    }
    setOperatingInFolderMode(false);
    setCensorMaskEdited(false);
    m_fileModeFileAbsPath = file;

    QImage img(file);
    if (img.isNull()) {
        QMessageBox::critical(this, tr("Failed to load image"), tr("Probably is not supported format"));
        return;
    }

    QImage mask;
    MetaConfig meta;
    if (takeMaskAndMetadataForImage(file, mask, meta)) {
        ui->sliderChunkSize->setSliderPosition(meta.chunkSize);
        on_sliderChunkSize_sliderMoved(meta.chunkSize); // FIXME: WHYYYYYYYY???
    } else {
        meta.method = CensorType::CT_Pixelize;
        meta.chunkSize = ui->sliderChunkSize->value();
    }

    ui->widCanvas->switchImage(img, mask, meta);
}


void MainWindow::on_sliderChunkSize_sliderMoved(int position)
{
    ui->widCanvas->setChunkSize(position);
    ui->lblChunkSize->setText(tr("%1px").arg(position));
}


void MainWindow::on_sliderBrushSize_sliderMoved(int position)
{
    ui->widCanvas->setBrushSize(position);
    ui->lblBrushSize->setText(tr("%1px").arg(position));
}


void MainWindow::on_actOpenFolder_triggered()
{
    if (!ensureSaved()) {
        return;
    }

    auto folder = QFileDialog::getExistingDirectory(this,
                                                    tr("Open a folder of many images"),
                                                    "");
    if (folder.isEmpty()) {
        return;
    }

    setOperatingInFolderMode(true);
    setCensorMaskEdited(false);
    m_dirModeDirAbsPath = folder;

    m_fsModel.setRootPath(folder);
    auto rootIdx = m_fsModel.index(folder);
    ui->lstFileList->setRootIndex(rootIdx);

    // Wait until loads
    QEventLoop el;
    auto connection = connect(&m_fsModel, &QFileSystemModel::directoryLoaded, [&](QString dir){ el.quit(); });
    el.exec();
    disconnect(connection);

    auto count = m_fsModel.rowCount(rootIdx);
    if (count == 0) {
        QMessageBox::critical(this, tr("No Image In Folder"), tr("Accepted images: %1").arg(m_fsModel.nameFilters().join(", ")));
        setOperatingInFolderMode(false);
        return;
    }
    m_dirModeFileCount = count;

    m_fsModel.sort(0);
    auto firstFile = m_fsModel.index(0, 0, rootIdx);
    ui->lstFileList->setCurrentIndex(firstFile);

//    QDirIterator dit(folder, {"*.jpg", "*.jpeg", "*.png"}, QDir::Filter::Files | QDir::NoDotAndDotDot);
//    QStringList fileList;
//    while (dit.hasNext()) {
//        auto file = dit.next();
//        qDebug() << file;
//        fileList << file;
//    }
}

void MainWindow::setOperatingInFolderMode(bool isInFolderMode)
{
    m_isNowOperatingInFolderMode = isInFolderMode;
    ui->lstFileList->setEnabled(isInFolderMode);
    ui->lstFileList->setVisible(isInFolderMode);
    ui->btnToggleFileList->setEnabled(isInFolderMode);
    ui->btnNextImg->setEnabled(isInFolderMode);
    ui->btnPrevImg->setEnabled(isInFolderMode);
}

bool MainWindow::takeMaskAndMetadataForImage(QString absPath, QImage &maskOut, MetaConfig &metaOut)
{
    QFileInfo fi(absPath);
    QString dataBaseDir = fi.dir().absolutePath() +
                     QDir::separator() +
                     CensorMeDataDir +
                     QDir::separator();
    maskOut = QImage(dataBaseDir + fi.fileName());

    do {
        QFile f(dataBaseDir + fi.fileName() + ".json");
        if (!f.open(QFile::ReadOnly)) break;
        QJsonParseError pe;
        auto jsd = QJsonDocument::fromJson(f.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError) break;
        if (!jsd.isObject()) break;
        auto obj = jsd.object();
        metaOut.method = (CensorType)obj["method"].toInt(0);
        metaOut.chunkSize = std::clamp(obj["chunkSize"].toInt(15), 2, 200);

        return true;
    } while (false);

    return false;
}

void MainWindow::reloadMaskAndMetadataForImage()
{
    QString file;
    if (!m_isNowOperatingInFolderMode) {
        file = m_fileModeFileAbsPath;
    } else {
        file = m_dirModeDirAbsPath + QDir::separator() + m_folderModeFileNameNoDir;
    }

    QImage mask;
    MetaConfig meta;
    if (takeMaskAndMetadataForImage(file, mask, meta)) {
        ui->sliderChunkSize->setSliderPosition(meta.chunkSize);
        on_sliderChunkSize_sliderMoved(meta.chunkSize); // FIXME: WHYYYYYYYY???
    } else {
        meta.method = CensorType::CT_Pixelize;
        meta.chunkSize = 15;
    }

    ui->widCanvas->restoreChanges(mask, meta);
}

bool MainWindow::isAnyImageOpened()
{
    if ((m_isNowOperatingInFolderMode && m_dirModeDirAbsPath.isNull()) ||
        ((!m_isNowOperatingInFolderMode) && m_fileModeFileAbsPath.isNull())) {
        return false;
    }
    return true;
}

bool MainWindow::ensureSaved()
{
    if (!isAnyImageOpened() || !m_censorMaskEdited) {
        return true;
    }

    if (!m_autoSaveOnSwitching) {
        QMessageBox::warning(this,
                             tr("Please save current file"),
                             tr("Autosave is not enabled. You must save before switching."));
        return false;
    }

    if (m_isNowOperatingInFolderMode) {
        return saveForFolderModeEditedFile(m_folderModeFileNameNoDir);
    } else {
        return saveForFileModeEditedFile();
    }
}

bool MainWindow::saveForFileModeEditedFile()
{
    QFileInfo fi(m_fileModeFileAbsPath);
    QDir maskDir(fi.dir().absolutePath() + QDir::separator() + CensorMeDataDir);

    if (!maskDir.exists()) {
        if (!QDir().mkpath(maskDir.absolutePath())) {
            QMessageBox::critical(this,
                                  tr("Cannot create data folder"),
                                  tr("Please check permission, disk space or other things that may cause this problem!"));
            return false;
        }
    }

retrySaveMask:
    if (!ui->widCanvas->getMaskImage().save(fi.dir().absolutePath() +
                                            QDir::separator() +
                                            CensorMeDataDir +
                                            QDir::separator() +
                                            fi.fileName() + ".png")) {
        auto ret = QMessageBox::critical(this,
                                         tr("Cannot save mask image"),
                                         tr("Please check permission, disk space or other things that may cause this problem!"),
                                         QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore);
        switch (ret) {
        default:
        case QMessageBox::Abort: return false;
        case QMessageBox::Retry: goto retrySaveMask;
        case QMessageBox::Ignore: break;
        }
    }

    QJsonObject ro;
    ro["method"] = ui->widCanvas->getCensorType();
    ro["chunkSize"] = ui->sliderChunkSize->value();
    QJsonDocument jsd(ro);
    QString meta = jsd.toJson(QJsonDocument::Compact);
    bool success = false;

retrySaveMeta:
    do {
        QFile f(fi.dir().absolutePath() +
                QDir::separator() +
                CensorMeDataDir +
                QDir::separator() +
                fi.fileName() + ".json");
        if (!f.open(QFile::WriteOnly)) break;
        QTextStream ts(&f);
        ts << meta;
        if (ts.status() != QTextStream::Ok) break;
        f.close();
        success = true;
    } while (false);
    if (!success) {
        auto ret = QMessageBox::critical(this,
                                         tr("Cannot save extra info file"),
                                         tr("Please check permission, disk space or other things that may cause this problem!"),
                                         QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore);
        switch (ret) {
        default:
        case QMessageBox::Abort: return false;
        case QMessageBox::Retry: goto retrySaveMeta;
        case QMessageBox::Ignore: break;
        }
    }

    setCensorMaskEdited(false);
    return true;
}

bool MainWindow::saveForFolderModeEditedFile(QString filename)
{
    QDir maskDir(m_dirModeDirAbsPath + QDir::separator() + CensorMeDataDir);

    if (!maskDir.exists()) {
        if (!QDir().mkpath(maskDir.absolutePath())) {
            QMessageBox::critical(this,
                                  tr("Cannot create data folder"),
                                  tr("Please check permission, disk space or other things that may cause this problem!"));
            return false;
        }
    }


retrySaveMask:
    if (!ui->widCanvas->getMaskImage().save(maskDir.absolutePath() + QDir::separator() + filename + ".png")) {
        auto ret = QMessageBox::critical(this,
                                         tr("Cannot save mask image"),
                                         tr("Please check permission, disk space or other things that may cause this problem!"),
                                         QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore);
        switch (ret) {
        default:
        case QMessageBox::Abort: return false;
        case QMessageBox::Retry: goto retrySaveMask;
        case QMessageBox::Ignore: break;
        }
    }

    QJsonObject ro;
    ro["method"] = ui->widCanvas->getCensorType();
    ro["chunkSize"] = ui->sliderChunkSize->value();
    QJsonDocument jsd(ro);
    QString meta = jsd.toJson(QJsonDocument::Compact);
    bool success = false;

retrySaveMeta:
    do {
        QFile f(maskDir.absolutePath() + QDir::separator() + filename + ".json");
        if (!f.open(QFile::WriteOnly)) break;
        QTextStream ts(&f);
        ts << meta;
        if (ts.status() != QTextStream::Ok) break;
        f.close();
        success = true;
    } while (false);
    if (!success) {
        auto ret = QMessageBox::critical(this,
                                         tr("Cannot save extra info file"),
                                         tr("Please check permission, disk space or other things that may cause this problem!"),
                                         QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore);
        switch (ret) {
        default:
        case QMessageBox::Abort: return false;
        case QMessageBox::Retry: goto retrySaveMeta;
        case QMessageBox::Ignore: break;
        }
    }

    setCensorMaskEdited(false);
    return true;
}

void MainWindow::exportTo(QString dir)
{
    QDir exportDir(dir);

    if (!exportDir.exists()) {
        if (!QDir().mkpath(exportDir.absolutePath())) {
            QMessageBox::critical(this,
                                  tr("Cannot create data folder"),
                                  tr("Please check permission, disk space or other things that may cause this problem!"));
            return;
        }
    }

    QMessageBox::StandardButton choice;
    if (m_isNowOperatingInFolderMode) {
        QProgressDialog pd(this);
        pd.setMinimumDuration(0);
        pd.setMaximum(m_dirModeFileCount);
        pd.setWindowModality(Qt::WindowModal);
        pd.show();

        int i = 0;
        auto rootIndex = ui->lstFileList->rootIndex();
        forever {
            auto fileIndex = m_fsModel.index(i, 0, rootIndex);
            if (!fileIndex.isValid()) break;
            // Select file
            ui->lstFileList->setCurrentIndex(fileIndex);
            // Export
            exportImageConfirmOverwrite(ui->widCanvas->getFinalImage(),
                                        dir + QDir::separator() + fileIndex.data().toString(),
                                        choice);
            switch (choice) {
            default:
            case QMessageBox::Yes:
            case QMessageBox::No:
                choice = QMessageBox::NoButton;
                break;
            case QMessageBox::YesToAll:
            case QMessageBox::NoToAll:
                break;
            case QMessageBox::Abort:
                goto abortExport;
                break;
            }
            i++;
            pd.setValue(i);
        }
    abortExport:
        pd.close();
    } else {
        exportImageConfirmOverwrite(ui->widCanvas->getFinalImage(),
                                    dir + QDir::separator() + QFileInfo(m_fileModeFileAbsPath).fileName(),
                                    choice);
    }
}

void MainWindow::exportImageConfirmOverwrite(QImage image, QString dest, QMessageBox::StandardButton &choice)
{
    QMessageBox::StandardButton newChoice = QMessageBox::Yes;
    if (QFile::exists(dest)) {
        if (choice == QMessageBox::NoButton) {
            newChoice = QMessageBox::question(this,
                                              tr("Confirm overwrite existing file?"),
                                              tr("Destination file %1 already exists.\n"
                                                 "Do you wish to overwrite?\n\n"
                                                 "To stop exporting, click \"Abort\".").arg(dest),
                                              QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Abort);
        }
    }

    bool stillWrite = false;
    switch (newChoice) {
    case QMessageBox::Yes:
    case QMessageBox::YesToAll:
        stillWrite = true;
        break;
    default:
        break;
    }
    choice = newChoice;

    if (stillWrite) {
retryExport:
        if (!image.save(dest, nullptr, 30)) {
            auto ret = QMessageBox::critical(nullptr,
                                             tr("Cannot save exported file"),
                                             tr("Please check permission, disk space or other things that may cause this problem!"),
                                             QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore);
            switch (ret) {
            default:
            case QMessageBox::Abort: choice = QMessageBox::Abort; return;
            case QMessageBox::Retry: goto retryExport;
            case QMessageBox::Ignore: break;
            }
        }
    }
}


void MainWindow::on_btnToggleFileList_toggled(bool checked)
{
    ui->lstFileList->setVisible(checked);
}

void MainWindow::switchToImage(QString absPath)
{

}

void MainWindow::setCensorMaskEdited(bool edited)
{
    if (!isAnyImageOpened()) return;

    m_censorMaskEdited = edited;
    ui->btnSave->setEnabled(edited);
    ui->btnRestore->setEnabled(edited);

    if (edited) {
        ui_lblEdited->setText("EDITED");
    } else {
        ui_lblEdited->setText("");
    }
}


void MainWindow::on_btnSave_clicked()
{
    if (m_isNowOperatingInFolderMode) {
        saveForFolderModeEditedFile(m_folderModeFileNameNoDir);
    } else {
        saveForFileModeEditedFile();
    }
}


void MainWindow::on_btnRestore_clicked()
{
    reloadMaskAndMetadataForImage();
    setCensorMaskEdited(false);
}


void MainWindow::on_actExportToOutput_triggered()
{
    if (m_isNowOperatingInFolderMode) {
        exportTo(m_dirModeDirAbsPath + QDir::separator() + "output");
    } else {
        QFileInfo fi(m_fileModeFileAbsPath);
        exportTo(fi.dir().absolutePath() + QDir::separator() + "output");
    }
}


void MainWindow::on_actExportSelectDest_triggered()
{
    QString baseDir = m_isNowOperatingInFolderMode ? m_dirModeDirAbsPath : QFileInfo(m_fileModeFileAbsPath).dir().absolutePath();
    auto dir = QFileDialog::getExistingDirectory(this,
                                                 tr("Select export directory..."),
                                                 baseDir);
    if (dir.isNull()) return;

    exportTo(dir);
}


void MainWindow::on_btnPrevImg_clicked()
{
    if (m_dirModeCurrentFileIndex == 0) return;

    ui->lstFileList->setCurrentIndex(m_fsModel.index(m_dirModeCurrentFileIndex - 1, 0, ui->lstFileList->rootIndex()));
}


void MainWindow::on_btnNextImg_clicked()
{
    if (m_dirModeCurrentFileIndex == m_dirModeFileCount - 1) return;

    ui->lstFileList->setCurrentIndex(m_fsModel.index(m_dirModeCurrentFileIndex + 1, 0, ui->lstFileList->rootIndex()));
}

