#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include "defs.h"
#include "mainwindow.h"
#include <QWidget>
#include <QPainter>

class CanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void setCensorType(CensorType type);
    CensorType getCensorType() { return m_censorType; }

    void setChunkSize(int chunkSize);
    void setBrushSize(int diameterPx);

    void setPreviewMode(int mode);

    void switchImage(QImage baseImage, QImage maskImage, MetaConfig meta);
    void restoreChanges(QImage maskImage, MetaConfig meta);

    QImage getFinalImage() { return m_previewFramebuffer; }
    QImage getMaskImage() { return m_maskImage; }

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void paintEvent(QPaintEvent*) override;
    virtual void wheelEvent(QWheelEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent*) override;
    virtual void mousePressEvent(QMouseEvent*) override;
    virtual void mouseReleaseEvent(QMouseEvent*) override;
    virtual void enterEvent(QEvent*) override;
    virtual void leaveEvent(QEvent*) override;

private:
    void processMouseDrag();

    void redetermineWidgetSize(QSize containerSize);

    void recomputeCensoredImage();
    void censorMethodPixelize();

    void mixdownToPreviewFramebuffer();

private:
    QSize m_parentSize;

    QSize m_imageSize;
    QImage m_baseImage;
    QImage m_maskImage;
    QImage m_censoredImage;
    QImage m_previewFramebuffer;
    QPainter m_copyPainter, m_drawCensorPainter, m_mixPainter;
    double m_censorComputationTime;

    QPoint m_mouseHoverPos, m_mouseLastHoverPos;
    bool m_brushShown;
    int m_brushSize; // Diameter
    enum {
        None,
        DrawCensor,
        EraseCensor,
        DragCanvas,
    } m_mouseActionType;
    PreviewMode m_previewMode;

    CensorType m_censorType;
    int m_chunkSize; // for pixelize
    bool m_previewShowMaskOnly;
    bool m_previewShowFullCensored;

signals:
    void censorMaskEdited();
};

#endif // CANVASWIDGET_H
