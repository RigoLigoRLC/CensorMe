#include "canvaswidget.h"
#include <QEvent>
#include <QDebug>
#include <QResizeEvent>
#include <vector>
#include <array>
#include <chrono>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget{parent}
{
    setMouseTracking(true);
    m_chunkSize = 15;
    m_brushSize = 50;
    m_mouseActionType = None;
    m_previewShowMaskOnly = true;
    m_censorType = CensorType::CT_Pixelize;
}

void CanvasWidget::setChunkSize(int chunkSize)
{
    m_chunkSize = chunkSize;

    recomputeCensoredImage();
}

void CanvasWidget::setBrushSize(int diameterPx)
{
    m_brushSize = diameterPx;

    if (m_drawCensorPainter.isActive()) {
        QPen pen = m_drawCensorPainter.pen();
        pen.setWidth(diameterPx);
        m_drawCensorPainter.setPen(pen);
    }
}

void CanvasWidget::setPreviewMode(int mode)
{
    m_previewMode = (PreviewMode)mode;
    update();
}

void CanvasWidget::switchImage(QImage baseImage, QImage maskImage, MetaConfig meta)
{
    m_baseImage = baseImage;
    m_censorType = meta.method;
    m_chunkSize = meta.chunkSize;
    if (maskImage.isNull()) {
        m_maskImage = QImage(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
        m_maskImage.fill(Qt::transparent);
    } else {
        m_maskImage = maskImage;
    }
    this->setFixedSize(baseImage.size());
    m_censoredImage = QImage(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
    m_previewFramebuffer = QImage(baseImage.size(), QImage::Format_ARGB32_Premultiplied);

    recomputeCensoredImage();
}

void CanvasWidget::restoreChanges(QImage maskImage, MetaConfig meta)
{
    if (maskImage.isNull()) {
        m_maskImage = QImage(m_baseImage.size(), QImage::Format_ARGB32_Premultiplied);
        m_maskImage.fill(Qt::transparent);
    } else {
        m_maskImage = maskImage;
    }
    m_chunkSize = meta.chunkSize;
    m_censorType = meta.method;

    recomputeCensoredImage();
}

bool CanvasWidget::eventFilter(QObject *obj, QEvent *event)
{
    // This event filter is only ever installed onto scroll area
    switch (event->type()) {
    case QEvent::Resize: {
        auto e = (QResizeEvent*)event;
        // Scroll area size != viewport size. Shrink by 1 pixel on all sides to work around it
        redetermineWidgetSize((m_parentSize = e->size().shrunkBy(QMargins(1, 1, 1, 1))));
        break;
    }
    default:
        break;
    }
    return false;
}

void CanvasWidget::paintEvent(QPaintEvent *pe)
{
    if (m_baseImage.isNull() || m_censoredImage.isNull()) {
        return;
    }

    QPainter p(this);
//    p.drawImage(rect(), m_censoredImage.isNull() ? m_baseImage : m_censoredImage);
    p.drawImage(rect(), m_maskImage);
    switch (m_previewMode) {

    case PM_Original:
        p.drawImage(rect(), m_baseImage);
        break;
    case PM_FullyCensored:
        p.drawImage(rect(), m_censoredImage);
        break;
    default:
    case PM_MaskOnly:
        p.drawImage(rect(), m_maskImage);
        break;
    case PM_MaskOnImage:
        p.drawImage(rect(), m_baseImage);
        p.drawImage(rect(), m_maskImage);
        break;
    case PM_FinalPreview: {
        p.drawImage(rect(), m_previewFramebuffer);
        break;
    }
    }

    // Below are inverted overlays
    p.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
    p.setPen(Qt::white);
    p.setBrush(Qt::NoBrush);
    p.drawText(20, 50, QString("Time taken: %1s").arg(m_censorComputationTime));

    // Brush
    int brushRadius = std::round(((double)width() / m_baseImage.width()) * m_brushSize);
    brushRadius /= 2;
    p.drawEllipse(m_mouseHoverPos, brushRadius, brushRadius);
    p.end();
}

void CanvasWidget::wheelEvent(QWheelEvent *)
{

}

void CanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_mouseLastHoverPos = m_mouseHoverPos;
    m_mouseHoverPos = e->pos();
    processMouseDrag();
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->buttons() == Qt::LeftButton) {
        // Prepare draw censor painter here
        m_drawCensorPainter.begin(&m_maskImage);
        m_previewFramebuffer.fill(Qt::transparent);
        m_mixPainter.begin(&m_previewFramebuffer);
        QPen pen = m_drawCensorPainter.pen();
        pen.setColor(Qt::white);
        pen.setWidth(m_brushSize);
        pen.setCapStyle(Qt::RoundCap);
        m_drawCensorPainter.setPen(pen);
        if (e->modifiers() & Qt::CTRL) {
            m_mouseActionType = EraseCensor;
            m_drawCensorPainter.setCompositionMode(QPainter::CompositionMode_Clear);
        } else {
            m_mouseActionType = DrawCensor;
            m_drawCensorPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    } else if (e->buttons() == Qt::RightButton) {
        m_mouseActionType = DragCanvas;
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    switch (m_mouseActionType) {

    case None:
        break;
    case DrawCensor:
    case EraseCensor:
        if (e->button() == Qt::LeftButton) {
            m_mouseActionType = None;
            m_mouseLastHoverPos = {-1, -1};
            m_drawCensorPainter.end();
            m_mixPainter.end();
        }
        break;
    case DragCanvas:
        if (e->button() == Qt::RightButton) {
            m_mouseActionType = None;
            m_mouseLastHoverPos = {-1, -1};
        }
        break;
    }
}

void CanvasWidget::enterEvent(QEvent *)
{
    m_brushShown = true;
    update();
}

void CanvasWidget::leaveEvent(QEvent *)
{
    m_brushShown = false;
    update();
}

void CanvasWidget::processMouseDrag()
{
    if (m_mouseLastHoverPos.x() == -1) {
        return;
    }

    switch (m_mouseActionType) {

    case None:
        break;
    case DrawCensor:
    case EraseCensor: {
        QPoint mappedBegin, mappedEnd;
        double ratio = (double)m_maskImage.width() / width();
        mappedBegin = m_mouseLastHoverPos * ratio;
        mappedEnd = m_mouseHoverPos * ratio;
        m_drawCensorPainter.drawLine(mappedBegin, mappedEnd);
        mixdownToPreviewFramebuffer();

        update();
        emit censorMaskEdited();
        break;
    }
    case DragCanvas:
        break;
    }
}

void CanvasWidget::redetermineWidgetSize(QSize containerSize)
{
    bool determineByHeight = (containerSize.width() / (double)containerSize.height()) >
                             (m_imageSize.width() / (double)m_imageSize.height());
    QSize newSize;
    if (determineByHeight) {
        newSize = {int((double)m_imageSize.width() / m_imageSize.height() * containerSize.height()),
                   containerSize.height()};
    } else {
        newSize = {containerSize.width(),
                   int((double)m_imageSize.height() / m_imageSize.width() * containerSize.width())};
    }
    setFixedSize(newSize);
}

void CanvasWidget::recomputeCensoredImage()
{
    switch (m_censorType) {

    case CT_Pixelize:
        censorMethodPixelize();
        break;
    case CT_GaussianBlur:
        break;
    case CT_White:
        break;
    }


    m_mixPainter.begin(&m_previewFramebuffer);
    mixdownToPreviewFramebuffer();
    m_mixPainter.end();
}

void CanvasWidget::censorMethodPixelize()
{
    int chunkSize = m_chunkSize;
    QSize imgSmallSize((m_baseImage.width() + chunkSize - 1) / chunkSize,
                       (m_baseImage.height() + chunkSize - 1) / chunkSize);
    QImage imgSmall(imgSmallSize, QImage::Format_ARGB32);

    auto hrcBegin = std::chrono::high_resolution_clock::now();

    // Construct the mean buckets for a line of resulting pixelized image
    std::vector<std::array<uint64_t, 3>> meanBucket;
    meanBucket.resize(imgSmall.width());
    for (auto &&i : meanBucket) i.fill(0); // Zero all

    std::vector<size_t> chunkWidths(imgSmall.width(), chunkSize);
    if (m_baseImage.width() % chunkSize) chunkWidths.back() = m_baseImage.width() % chunkSize;

    size_t nextRowToDump = 1;
    for (size_t y = 0; y < m_baseImage.height(); y++) {
        // One line was previously finished
        if (nextRowToDump == (y / chunkSize)) {
            for (size_t i = 0; i < meanBucket.size(); i++) {
                auto px = imgSmall.bits() + ((nextRowToDump - 1) * imgSmall.width() + i) * 4;
                size_t meanChunkPixelCount = chunkWidths[i] * chunkSize;

                px[3] = 0xff; // Alpha
                px[2] = uint8_t(meanBucket[i][0] / meanChunkPixelCount); // Red
                px[1] = uint8_t(meanBucket[i][1] / meanChunkPixelCount); // Green
                px[0] = uint8_t(meanBucket[i][2] / meanChunkPixelCount); // Blue

                meanBucket[i].fill(0);
            }
            nextRowToDump++;
        }


        for (size_t x = 0; x < m_baseImage.width(); x++) {
            auto color = m_baseImage.pixel(int(x), int(y));
            meanBucket[x / chunkSize][0] += qRed(color);
            meanBucket[x / chunkSize][1] += qGreen(color);
            meanBucket[x / chunkSize][2] += qBlue(color);
        }
    }

    // Last line of pixelated
    for (size_t i = 0; i < meanBucket.size(); i++) {
        auto px = imgSmall.bits() + ((nextRowToDump - 1) * imgSmall.width() + i) * 4;
        size_t meanChunkPixelCount = chunkWidths[i] * (m_baseImage.height() % chunkSize ?
                                                       m_baseImage.height() % chunkSize :
                                                       chunkSize);

        px[3] = 0xff; // Alpha
        px[2] = uint8_t(meanBucket[i][0] / meanChunkPixelCount); // Red
        px[1] = uint8_t(meanBucket[i][1] / meanChunkPixelCount); // Green
        px[0] = uint8_t(meanBucket[i][2] / meanChunkPixelCount); // Blue

        meanBucket[i].fill(0);
    }

    m_copyPainter.begin(&m_censoredImage);
    m_copyPainter.setRenderHint(QPainter::Antialiasing, false);
    m_copyPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    m_copyPainter.drawImage(QRect(QPoint{0, 0},
                                  QPoint{imgSmall.width() * chunkSize,
                                         imgSmall.height() * chunkSize}
                                  ),
                            imgSmall);
    m_copyPainter.end();

    auto hrcEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> timeTaken = hrcEnd - hrcBegin;
    m_censorComputationTime = timeTaken.count();
    m_imageSize = m_baseImage.size();

    redetermineWidgetSize(m_parentSize);
    update();
}

void CanvasWidget::mixdownToPreviewFramebuffer()
{
    m_previewFramebuffer.fill(Qt::transparent);
    m_mixPainter.drawImage(m_previewFramebuffer.rect(), m_maskImage);
    m_mixPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    m_mixPainter.drawImage(m_previewFramebuffer.rect(), m_censoredImage);
    m_mixPainter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    m_mixPainter.drawImage(m_previewFramebuffer.rect(), m_baseImage);
}


