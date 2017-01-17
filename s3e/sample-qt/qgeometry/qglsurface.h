/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGLSURFACE_H
#define QGLSURFACE_H

#include <QRect>
#include <QScopedPointer>

#include <QtGui/QOpenGLFramebufferObject>
#include <QtOpenGL/QGLPixelBuffer>

QT_BEGIN_NAMESPACE

class QWindow;
class QOpenGLWidget;
class QOpenGLContext;
class QGLAbstractSurfacePrivate;
class QGLFramebufferObjectSurfacePrivate;
class QGLSubsurfacePrivate;
class QGLWidgetSurfacePrivate;

class QGLAbstractSurface
{
public:
    virtual ~QGLAbstractSurface();

    enum SurfaceType
    {
        Window,
        FramebufferObject,
        PixelBuffer,
        Subsurface,
        User = 1000
    };

    int surfaceType() const;

    QOpenGLContext *context() const;
    void setContext(QOpenGLContext *context);
    QWindow *window() const;
    void setWindow(QWindow *window);
    QOpenGLFramebufferObject *framebufferObject() const;
    void setFramebufferObject(QOpenGLFramebufferObject *framebufferObject);

    virtual bool activate(QGLAbstractSurface *prevSurface = 0) = 0;
    virtual void deactivate(QGLAbstractSurface *nextSurface = 0) = 0;
    virtual QRect viewportGL() const = 0;
    QRect viewportRect() const;
    virtual float aspectRatio() const;

    bool switchTo(QGLAbstractSurface *nextSurface);
    virtual bool isValid() const;

    static QGLAbstractSurface *createSurfaceForContext(QOpenGLContext *context);

protected:
    QGLAbstractSurface(int surfaceType);

private:
    QScopedPointer<QGLAbstractSurfacePrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLAbstractSurface)
    Q_DISABLE_COPY(QGLAbstractSurface)
};

class QGLContextSurface : public QGLAbstractSurface
{
public:
    static const int QGLCONTEXT_SURFACE_ID = 502;

    explicit QGLContextSurface(QOpenGLContext *context)
        : QGLAbstractSurface(QGLCONTEXT_SURFACE_ID)
    {
        setContext(context);
    }
    ~QGLContextSurface() {}

    bool activate(QGLAbstractSurface *prevSurface);
    void deactivate(QGLAbstractSurface *nextSurface);
    QRect viewportGL() const;
    bool isValid() const;

private:
    Q_DISABLE_COPY(QGLContextSurface)
};

class QGLFramebufferObjectSurface : public QGLAbstractSurface
{
public:
    QGLFramebufferObjectSurface();
    explicit QGLFramebufferObjectSurface
        (QOpenGLFramebufferObject *fbo, QOpenGLContext *context = 0);
    ~QGLFramebufferObjectSurface();

    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QScopedPointer<QGLFramebufferObjectSurfacePrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLFramebufferObjectSurface)
    Q_DISABLE_COPY(QGLFramebufferObjectSurface)
};

class QGLPixelBufferSurface : public QGLAbstractSurface
{
public:
    QGLPixelBufferSurface();
    explicit QGLPixelBufferSurface(QGLPixelBuffer *pbuffer);
    ~QGLPixelBufferSurface();

    QGLPixelBuffer *pixelBuffer() const;
    void setPixelBuffer(QGLPixelBuffer *pbuffer);

    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QGLPixelBuffer *m_pb;
    Q_DISABLE_COPY(QGLPixelBufferSurface)
};

class QGLSubsurface : public QGLAbstractSurface
{
public:
    QGLSubsurface();
    QGLSubsurface(QGLAbstractSurface *surface, const QRect &region);
    ~QGLSubsurface();

    QGLAbstractSurface *surface() const;
    void setSurface(QGLAbstractSurface *subSurface);

    QRect region() const;
    void setRegion(const QRect &region);

    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;
    bool isValid() const;

private:
    QScopedPointer<QGLSubsurfacePrivate> d_ptr;

    Q_DECLARE_PRIVATE(QGLSubsurface)
    Q_DISABLE_COPY(QGLSubsurface)
};

class QGLWindowSurface : public QGLAbstractSurface
{
public:
    QGLWindowSurface();
    explicit QGLWindowSurface(QWindow *window);
    ~QGLWindowSurface();

    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;
    bool isValid() const;

private:
    Q_DISABLE_COPY(QGLWindowSurface)
};

#ifdef QT_WIDGETS_LIB
class QGLWidgetSurface : public QGLAbstractSurface
{
public:
    QGLWidgetSurface();
    explicit QGLWidgetSurface(QOpenGLWidget *widget);
    ~QGLWidgetSurface();

    QOpenGLWidget *widget() const;
    void setWidget(QOpenGLWidget *widget);

    QPaintDevice *device() const;
    bool activate(QGLAbstractSurface *prevSurface = 0);
    void deactivate(QGLAbstractSurface *nextSurface = 0);
    QRect viewportGL() const;

private:
    QOpenGLWidget *m_widget;

    Q_DISABLE_COPY(QGLWidgetSurface)
};
#endif // QT_WIDGETS_LIB

QT_END_NAMESPACE

#endif // QGLSURFACE_H
