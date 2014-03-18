/******************************************************************************
    QtAV:  Media play library based on Qt and FFmpeg
    Copyright (C) 2012-2014 Wang Bin <wbsecg1@gmail.com>

*   This file is part of QtAV

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include <QtAV/VideoRenderer.h>
#include <QtAV/private/VideoRenderer_p.h>
#include <QtAV/Filter.h>
#include <QtAV/OSDFilter.h>
#include <QtCore/QCoreApplication>
#include <QWidget>
#include <QGraphicsItem>

namespace QtAV {

VideoRenderer::VideoRenderer()
    :AVOutput(*new VideoRendererPrivate)
{
}

VideoRenderer::VideoRenderer(VideoRendererPrivate &d)
    :AVOutput(d)
{
}

VideoRenderer::~VideoRenderer()
{
}

bool VideoRenderer::receive(const VideoFrame &frame)
{
    setInSize(frame.width(), frame.height());
    return receiveFrame(frame);
}

bool VideoRenderer::setPreferredPixelFormat(VideoFormat::PixelFormat pixfmt)
{
    if (!isSupported(pixfmt)) {
        qWarning("pixel format '%s' is not supported", VideoFormat(pixfmt).name().toUtf8().constData());
        return false;
    }
    d_func().preferred_format = pixfmt;
    return true;
}

VideoFormat::PixelFormat VideoRenderer::preferredPixelFormat() const
{
    return d_func().preferred_format;
}

void VideoRenderer::forcePreferredPixelFormat(bool force)
{
    d_func().force_preferred = force;
}

bool VideoRenderer::isPreferredPixelFormatForced() const
{
    return d_func().force_preferred;
}

void VideoRenderer::scaleInRenderer(bool q)
{
    d_func().scale_in_renderer = q;
}

bool VideoRenderer::scaleInRenderer() const
{
    return d_func().scale_in_renderer;
}

void VideoRenderer::setOutAspectRatioMode(OutAspectRatioMode mode)
{
    DPTR_D(VideoRenderer);
    if (mode == d.out_aspect_ratio_mode)
        return;
    d.aspect_ratio_changed = true;
    d.out_aspect_ratio_mode = mode;
    if (mode == RendererAspectRatio) {
        //compute out_rect
        d.out_rect = QRect(1, 0, d.renderer_width, d.renderer_height); //remove? already in computeOutParameters()
        setOutAspectRatio(qreal(d.renderer_width)/qreal(d.renderer_height));
        //is that thread safe?
    } else if (mode == VideoAspectRatio) {
        setOutAspectRatio(d.source_aspect_ratio);
    }
}

VideoRenderer::OutAspectRatioMode VideoRenderer::outAspectRatioMode() const
{
    return d_func().out_aspect_ratio_mode;
}

void VideoRenderer::setOutAspectRatio(qreal ratio)
{
    DPTR_D(VideoRenderer);
    bool ratio_changed = d.out_aspect_ratio != ratio;
    d.out_aspect_ratio = ratio;
    //indicate that this function is called by user. otherwise, called in VideoRenderer
    if (!d.aspect_ratio_changed) {
        d.out_aspect_ratio_mode = CustomAspectRation;
    }
    d.aspect_ratio_changed = false; //TODO: when is false?
    if (d.out_aspect_ratio_mode != RendererAspectRatio) {
        d.update_background = true; //can not fill the whole renderer with video
    }
    //compute the out out_rect
    d.computeOutParameters(ratio);
    if (ratio_changed) {
        resizeFrame(d.out_rect.width(), d.out_rect.height());
    }
}

qreal VideoRenderer::outAspectRatio() const
{
    return d_func().out_aspect_ratio;
}

void VideoRenderer::setQuality(Quality q)
{
    DPTR_D(VideoRenderer);
    d.quality = q;
    qDebug("Quality: %d", q);
}

VideoRenderer::Quality VideoRenderer::quality() const
{
    return d_func().quality;
}

void VideoRenderer::setInSize(const QSize& s)
{
    setInSize(s.width(), s.height());
}

void VideoRenderer::setInSize(int width, int height)
{
    DPTR_D(VideoRenderer);
    if (d.src_width != width || d.src_height != height) {
        d.aspect_ratio_changed = true; //?? for VideoAspectRatio mode
    }
    if (!d.aspect_ratio_changed)// && (d.src_width == width && d.src_height == height))
        return;
    d.src_width = width;
    d.src_height = height;
    d.source_aspect_ratio = qreal(d.src_width)/qreal(d.src_height);
    qDebug("%s => calculating aspect ratio from converted input data(%f)", __FUNCTION__, d.source_aspect_ratio);
    //see setOutAspectRatioMode
    if (d.out_aspect_ratio_mode == VideoAspectRatio) {
        //source_aspect_ratio equals to original video aspect ratio here, also equals to out ratio
        setOutAspectRatio(d.source_aspect_ratio);
    }
    d.aspect_ratio_changed = false; //TODO: why graphicsitemrenderer need this? otherwise aspect_ratio_changed is always true?
}

bool VideoRenderer::open()
{
    return true;
}

bool VideoRenderer::close()
{
    return true;
}

void VideoRenderer::resizeRenderer(const QSize &size)
{
    resizeRenderer(size.width(), size.height());
}

void VideoRenderer::resizeRenderer(int width, int height)
{
    DPTR_D(VideoRenderer);
    if (width == 0 || height == 0)
        return;

    d.renderer_width = width;
    d.renderer_height = height;
    d.computeOutParameters(d.out_aspect_ratio);
    resizeFrame(d.out_rect.width(), d.out_rect.height());
}

QSize VideoRenderer::rendererSize() const
{
    DPTR_D(const VideoRenderer);
    return QSize(d.renderer_width, d.renderer_height);
}

int VideoRenderer::rendererWidth() const
{
    return d_func().renderer_width;
}

int VideoRenderer::rendererHeight() const
{
    return d_func().renderer_height;
}

QSize VideoRenderer::frameSize() const
{
    DPTR_D(const VideoRenderer);
    return QSize(d.src_width, d.src_height);
}

QRect VideoRenderer::videoRect() const
{
    return d_func().out_rect;
}

QRectF VideoRenderer::regionOfInterest() const
{
    return d_func().roi;
}

void VideoRenderer::setRegionOfInterest(qreal x, qreal y, qreal width, qreal height)
{
    DPTR_D(VideoRenderer);
    d.roi = QRectF(x, y, width, height);
}

void VideoRenderer::setRegionOfInterest(const QRectF &roi)
{
    d_func().roi = roi;
}

QRect VideoRenderer::realROI() const
{
    DPTR_D(const VideoRenderer);
    if (!d.roi.isValid()) {
        return QRect(QPoint(), d.video_frame.size());
    }
    QRect r = d.roi.toRect();
    if (qAbs(d.roi.x()) <= 1)
        r.setX(d.roi.x()*qreal(d.src_width)); //TODO: why not video_frame.size()? roi not correct
    if (qAbs(d.roi.y()) <= 1)
        r.setY(d.roi.y()*qreal(d.src_height));
    // whole size use width or height = 0, i.e. null size
    if (qAbs(d.roi.width()) < 1)
        r.setWidth(d.roi.width()*qreal(d.src_width));
    if (qAbs(d.roi.height() < 1))
        r.setHeight(d.roi.height()*qreal(d.src_height));
    //TODO: insect with source rect?
    return r;
}

QPointF VideoRenderer::mapToFrame(const QPointF &p) const
{
    QRectF roi = realROI();
    // zoom=roi.w/roi.h>vo.w/vo.h?roi.w/vo.w:roi.h/vo.h
    qreal zoom = qMax(roi.width()/rendererWidth(), roi.height()/rendererHeight());
    QPointF delta = p - QPointF(rendererWidth()/2, rendererHeight()/2);
    return roi.center() + delta * zoom;
}

QPointF VideoRenderer::mapFromFrame(const QPointF &p) const
{
    QRectF roi = realROI();
    // zoom=roi.w/roi.h>vo.w/vo.h?roi.w/vo.w:roi.h/vo.h
    qreal zoom = qMax(roi.width()/rendererWidth(), roi.height()/rendererHeight());
    // (p-roi.c)/zoom + c
    QPointF delta = p - roi.center();
    return QPointF(rendererWidth()/2, rendererHeight()/2) + delta / zoom;
}

OSDFilter *VideoRenderer::setOSDFilter(OSDFilter *filter)
{
    DPTR_D(VideoRenderer);
    Filter *old = d.osd_filter;
    //may be both null
    if (old == filter) {
        return static_cast<OSDFilter*>(old);
    }
    d.osd_filter = filter;
    //subtitle and osd is at the end
    int idx = d.filters.lastIndexOf(old);
    if (idx != -1) {
        if (filter)
            d.filters.replace(idx, filter);
        else //null==disable
            d.filters.takeAt(idx);
    } else {
        if (filter)
            d.filters.push_back(filter);
    }
    return static_cast<OSDFilter*>(old);
}

OSDFilter* VideoRenderer::osdFilter()
{
    return static_cast<OSDFilter*>(d_func().osd_filter);
}
//TODO: setSubtitleFilter and setOSDFilter are almost the same. refine code
Filter* VideoRenderer::setSubtitleFilter(Filter *filter)
{
    DPTR_D(VideoRenderer);
    Filter *old = d.subtitle_filter;
    //may be both null
    if (old == filter) {
        return old;
    }
    d.subtitle_filter = filter;
    //subtitle and osd is at the end
    int idx = d.filters.lastIndexOf(old);
    if (idx != -1) {
        if (filter)
            d.filters.replace(idx, filter);
        else //null==disable
            d.filters.takeAt(idx);
    } else {
        if (filter)
            d.filters.push_back(filter);
    }
    return old;
}

Filter* VideoRenderer::subtitleFilter()
{
    return d_func().subtitle_filter;
}

bool VideoRenderer::needUpdateBackground() const
{
    return d_func().update_background;
}

void VideoRenderer::drawBackground()
{
}

bool VideoRenderer::needDrawFrame() const
{
    return d_func().video_frame.isValid();
}

void VideoRenderer::resizeFrame(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void VideoRenderer::handlePaintEvent()
{
    DPTR_D(VideoRenderer);
    d.setupQuality();
    //begin paint. how about QPainter::beginNativePainting()?
    {
        //lock is required only when drawing the frame
        QMutexLocker locker(&d.img_mutex);
        Q_UNUSED(locker);
        /* begin paint. how about QPainter::beginNativePainting()?
         * fill background color when necessary, e.g. renderer is resized, image is null
         * if we access d.data which will be modified in AVThread, the following must be
         * protected by mutex. otherwise, e.g. QPainterRenderer, it's not required if drawing
         * on the shared data is safe
         */
        if (needUpdateBackground()) {
            /* xv: should always draw the background. so shall we only paint the border
             * rectangles, but not the whole widget
             */
            d.update_background = false;
            //fill background color. DO NOT return, you must continue drawing
            drawBackground();
        }
        /* DO NOT return if no data. we should draw other things
         * NOTE: if data is not copyed in receiveFrame(), you should always call drawFrame()
         */
        /*
         * why the background is white if return? the below code draw an empty bitmap?
         */
        //DO NOT return if no data. we should draw other things
        if (needDrawFrame()) {
            drawFrame();
        }
    }
    hanlePendingTasks();
    //TODO: move to AVOutput::applyFilters() //protected?
    if (!d.filters.isEmpty() && d.filter_context && d.statistics) {
        foreach(Filter* filter, d.filters) {
            if (!filter) {
                qWarning("a null filter!");
                //d.filters.removeOne(filter);
                continue;
            }
            filter->process(d.filter_context, d.statistics);
        }
    } else {
        //warn once
    }
    //end paint. how about QPainter::endNativePainting()?
}

void VideoRenderer::enableDefaultEventFilter(bool e)
{
    d_func().default_event_filter = e;
}

bool VideoRenderer::isDefaultEventFilterEnabled() const
{
    return d_func().default_event_filter;
}

qreal VideoRenderer::brightness() const
{
    return d_func().brightness;
}

bool VideoRenderer::setBrightness(qreal brightness)
{
    if (!onChangingBrightness(brightness))
        return false;
    d_func().brightness = brightness;
    if (widget()) {
        widget()->update();
    }
    if (graphicsItem()) {
        graphicsItem()->update();
    }
    return true;
}

qreal VideoRenderer::contrast() const
{
    return d_func().contrast;
}

bool VideoRenderer::setContrast(qreal contrast)
{
    if (!onChangingContrast(contrast))
        return false;
    d_func().contrast = contrast;
    if (widget()) {
        widget()->update();
    }
    if (graphicsItem()) {
        graphicsItem()->update();
    }
    return true;
}

qreal VideoRenderer::hue() const
{
    return d_func().hue;
}

bool VideoRenderer::setHue(qreal hue)
{
    if (!onChangingHue(hue))
        return false;
    d_func().hue = hue;
    if (widget()) {
        widget()->update();
    }
    if (graphicsItem()) {
        graphicsItem()->update();
    }
    return true;
}

qreal VideoRenderer::saturation() const
{
    return d_func().saturation;
}

bool VideoRenderer::setSaturation(qreal saturation)
{
    if (!onChangingSaturation(saturation))
        return false;
    d_func().saturation = saturation;
    if (widget()) {
        widget()->update();
    }
    if (graphicsItem()) {
        graphicsItem()->update();
    }
    return true;
}

bool VideoRenderer::onChangingBrightness(qreal b)
{
    Q_UNUSED(b);
    return false;
}

bool VideoRenderer::onChangingContrast(qreal c)
{
    Q_UNUSED(c);
    return false;
}

bool VideoRenderer::onChangingHue(qreal h)
{
    Q_UNUSED(h);
    return false;
}

bool VideoRenderer::onChangingSaturation(qreal s)
{
    Q_UNUSED(s);
    return false;
}


} //namespace QtAV
