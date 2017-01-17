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

#include "qglsurface.h"
#include "qglsurface_p.h"

#include <QDebug>
#include <QPainter>
#include <QPaintEngine>
#include <QWindow>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QSurface>
#ifdef QT_WIDGETS_LIB
#include <QOpenGLWidget>
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QGLAbstractSurface
    \brief The QGLAbstractSurface class represents an OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting

    OpenGL can be used to draw into a number of different surface types:
    windows, pixel buffers (pbuffers), framebuffer objects, and so on.
    It is also possible to use only part of a surface by setting
    the \c{glViewport()} to restrict drawing to that part.  An example
    of a subsurface may be the left or right eye image of a stereoscopic
    pair that is rendered into the two halves of a window.

    Note that pixelbuffers are only supported when Qt is built with the
    OpenGL library.

    Activating a surface for OpenGL drawing, and deactivating it afterwards
    can be quite varied across surface types.  Sometimes it is enough
    to just make a QOpenGLContext current and set the \c{glViewport()}.
    Other times a context must be made current, followed by binding a
    framebuffer object, and finally setting the \c{glViewport()}.

    QGLAbstractSurface and its subclasses simplify the activation and
    deactivation of surfaces by encapsulating the logic needed to
    use a particular kind of surface into activate() and deactivate()
    respectively.

    Normally surfaces are activated by calling QGLPainter::pushSurface()
    as in the following example of switching drawing to a framebuffer
    object:

    \code
    QGLPainter painter;
    painter.begin(widget);
    QGLFramebufferObjectSurface surface(fbo);
    painter.pushSurface(&surface);
    ... // draw into the fbo
    painter.popSurface();
    ... // draw into the widget
    \endcode

    QGLPainter maintains a stack of surfaces, starting with the paint
    device specified to QGLPainter::begin() (usually a widget).
    The QGLPainter::pushSurface() function calls deactivate() on the
    current surface, activate() on the new surface, and then adjusts the
    \c{glViewport()} to match the value of viewportGL() for the new surface.
    When QGLPainter::popSurface() is called, the previous surface
    is re-activated and the \c{glViewport()} changed accordingly.

    \sa QGLFramebufferObjectSurface, QGLWindowSurface, QGLSubsurface
    \sa QGLPixelBufferSurface, QGLPainter::pushSurface()
*/

class QGLAbstractSurfacePrivate
{
public:
    QGLAbstractSurfacePrivate(QGLAbstractSurface::SurfaceType surfaceType);
    ~QGLAbstractSurfacePrivate();

    QOpenGLContext *context;
    QWindow *window;
    QOpenGLFramebufferObject *fbo;
    int type;
};

QGLAbstractSurfacePrivate::QGLAbstractSurfacePrivate(QGLAbstractSurface::SurfaceType surfaceType)
    : context(0)
    , window(0)
    , fbo(0)
    , type(surfaceType)
{
    // nothing to do here
}

QGLAbstractSurfacePrivate::~QGLAbstractSurfacePrivate()
{
    // nothing to do here - objects are not owned by us
}

/*!
    \enum QGLAbstractSurface::SurfaceType
    This enum defines the type of a QGLAbstractSurface.

    \value Window Instance of QGLWindowSurface.
    \value FramebufferObject Instance of QGLFramebufferObjectSurface.
    \value PixelBuffer Instance of QGLPixelBufferSurface.
    \value Subsurface Instance of QGLSubsurface.
    \value User First user-defined surface type for use by applications.
*/

/*!
    \fn QGLAbstractSurface::QGLAbstractSurface(int surfaceType)

    Constructs an OpenGL drawing surface of the specified \a surfaceType.
*/
QGLAbstractSurface::QGLAbstractSurface(int surfaceType)
    : d_ptr(new QGLAbstractSurfacePrivate((QGLAbstractSurface::SurfaceType)surfaceType))
{
    Q_ASSERT(surfaceType >= Window);
}

/*!
    Destroys this OpenGL drawing surface.
*/
QGLAbstractSurface::~QGLAbstractSurface()
{
    // nothing to do - the scoped pointer will delete the d_ptr
}

/*!
    Returns the type of this surface.
*/
int QGLAbstractSurface::surfaceType() const
{
    Q_D(const QGLAbstractSurface);
    return d->type;
}

/*!
    Returns the OpenGL context which is associated with this surface, if any.
    When this surface is first activated, if this value is null then it will be
    set to the context current at that time.

    The default value is null.

    \sa setContext()
*/
QOpenGLContext *QGLAbstractSurface::context() const
{
    Q_D(const QGLAbstractSurface);
    return d->context;
}

/*!
    Sets the OpenGL context which is to be associated with this surface.
    When this surface is first activated, if this value is null then it will be
    set to the context current at that time.

    \sa context()
*/
void QGLAbstractSurface::setContext(QOpenGLContext *context)
{
    Q_D(QGLAbstractSurface);
    d->context = context;
}

/*!
    Returns the QWindow which is associated with this surface if any.
    The default value is null.

    \sa setWindow()
*/
QWindow *QGLAbstractSurface::window() const
{
    Q_D(const QGLAbstractSurface);
    return d->window;
}

/*!
    Sets the QWindow which is to be associated with this surface to be \a window.

    \sa window()
*/
void QGLAbstractSurface::setWindow(QWindow *window)
{
    Q_D(QGLAbstractSurface);
    d->window = window;
}

/*!
    Returns the QOpenGLFramebufferObject which is associated with this surface if any.
    The default value is null.

    \sa setFramebufferObject()
*/
QOpenGLFramebufferObject *QGLAbstractSurface::framebufferObject() const
{
    Q_D(const QGLAbstractSurface);
    return d->fbo;
}

/*!
    Sets the QOpenGLFramebufferObject which is to be associated with this surface to be \a framebufferObject.

    \sa framebufferObject()
*/
void QGLAbstractSurface::setFramebufferObject(QOpenGLFramebufferObject *framebufferObject)
{
    Q_D(QGLAbstractSurface);
    d->fbo = framebufferObject;
}

/*!
    \fn void QGLAbstractSurface::activate(QGLAbstractSurface *prevSurface)

    Activate this surface.

    If \a prevSurface is null, then that surface has just been deactivated
    in the process of switching to this surface.  This may allow activate()
    to optimize the transition to avoid unnecessary state changes.

    Typically implementations should assert if this fails in debug mode,
    since no rendering into the surface is possible.

    \sa deactivate(), switchTo()
*/

/*!
    \fn void QGLAbstractSurface::deactivate(QGLAbstractSurface *nextSurface)

    Deactivate this surface from the current context, but leave the
    context current.  Typically this will release the framebuffer
    object associated with the surface.

    If \a nextSurface is not-null, then that surface will be activated next
    in the process of switching away from this surface.  This may allow
    deactivate() to optimize the transition to avoid unnecessary state
    changes.

    \sa activate(), switchTo()
*/

/*!
    Returns the rectangle of the surface device() that is occupied by
    the viewport for this surface.  The origin is at the top-left.

    This function calls viewportGL() and then flips the rectangle
    upside down using the height of device() so that the origin
    is at the top-left instead of the bottom-left.

    \sa viewportGL(), device()
*/
QRect QGLAbstractSurface::viewportRect() const
{
    Q_ASSERT(isValid());

    QRect view = viewportGL();
    int height = 0;
    if (surfaceType() == Window)
    {
        Q_ASSERT(window());
        height = window()->height();
    }
    else if (surfaceType() == FramebufferObject)
    {
        Q_ASSERT(framebufferObject());
        height = framebufferObject()->size().height();
    }

    return QRect(view.x(), height - (view.y() + view.height()),
                 view.width(), view.height());
}

/*!
    \fn QRect QGLAbstractSurface::viewportGL() const

    Returns the rectangle of the surface that is occupied by
    the viewport for this surface.  The origin is at the bottom-left,
    which makes the value suitable for passing to \c{glViewport()}:

    \code
    QRect viewport = surface->viewportGL();
    glViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height());
    \endcode

    Normally the viewport rectangle is the full extent of the device(),
    but it could be smaller if the application only wishes to draw
    into a subpart of the device().  An example would be rendering left
    and right stereo eye images into the two halves of a QGLWidget.
    The eye surfaces would typically be instances of QGLSubsurface.

    Note that the value returned from this function is not defined before
    the activate() function has been called at least once.

    \sa viewportRect(), device()
*/

/*!
    Returns the aspect ratio of viewportGL().

    The return value is used to correct perspective and orthographic
    projections for the aspect ratio of the drawing surface.

    No adjustments are made for DPI.

    Subclasses may override this function to further adjust the return value
    if the DPI in the horizontal vs vertical direction is not the same,
    that is, the pixels are not square.
*/
float QGLAbstractSurface::aspectRatio() const
{
    Q_ASSERT(isValid());

    // Get the size of the current viewport.
    QSize size = viewportGL().size();
    if (size.width() == size.height())
        return 1.0f;

    // Return the final aspect ratio based on viewport.
    return float(size.width()) / float(size.height());
}

/*!
    Switches from this surface to \a nextSurface by calling deactivate()
    on this surface and activate() on \a nextSurface.

    Returns true if \a nextSurface was activated, or false otherwise.
    If \a nextSurface could not be activated, then this surface will
    remain activated.

    \sa activate(), deactivate()
*/
bool QGLAbstractSurface::switchTo(QGLAbstractSurface *nextSurface)
{
    if (nextSurface) {
        deactivate(nextSurface);
        if (nextSurface->activate(this))
            return true;
        activate();
        return false;
    } else {
        deactivate();
        return true;
    }
}

/*!
    Returns true if this surface is valid and ready to be drawn into with OpenGL
    commands.  Typically it will be true if the surface has been associated with
    an opengl context and a supported painting context such as a window or fbo.

    Sub-class implementations can use this fall-back, which simply checks for
    a valid viewport rectangle.

    Note that the surface may only become valid during activate() calls.
*/
bool QGLAbstractSurface::isValid() const
{
    return viewportGL().isValid();
}

/*!
    Creates an OpenGL drawing surface using the given \a context.  This relies on
    the \a context having a valid surface which is a QWindow in which case the
    surface returned is a QGLWindowSurface.

    If not, then a generic surface based on the OpenGL context is returned.  In this
    case the generic surface will fail unless an underlying rendering surface becomes
    available prior to attempting to activate the surface.
*/
QGLAbstractSurface *QGLAbstractSurface::createSurfaceForContext(QOpenGLContext *context)
{
    Q_ASSERT(context);
#ifndef QT_NO_DEBUG_STREAM
    if (context->surface() && context->surface()->surfaceClass() != QSurface::Window)
        qWarning() << "Attempt to cast non-window surface";
#endif
    QWindow *win = static_cast<QWindow*>(context->surface());
    if (win)
    {
        return new QGLWindowSurface(win);
    }
    else
    {
        return new QGLContextSurface(context);
    }
    return 0;
}



/*!
    \class QGLFramebufferObjectSurface
    \brief The QGLFramebufferObjectSurface class represents a framebuffer object that is being used as an OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

// This exists solely for future expansion
class QGLFramebufferObjectSurfacePrivate
{
public:
    QGLFramebufferObjectSurfacePrivate()
        : forExpansion(0) {}

    int forExpansion;
};

/*!
    Constructs a default framebuffer object surface.  This constructor
    should be followed by a call to setFramebufferObject().
*/
QGLFramebufferObjectSurface::QGLFramebufferObjectSurface()
    : QGLAbstractSurface(QGLAbstractSurface::FramebufferObject)
    , d_ptr(new QGLFramebufferObjectSurfacePrivate)
{
}

/*!
    Constructs a framebuffer object surface for \a fbo and \a context.
    If \a context is null, then the framebuffer will be bound to the
    current context when activate() is called.
*/
QGLFramebufferObjectSurface::QGLFramebufferObjectSurface
        (QOpenGLFramebufferObject *fbo, QOpenGLContext *context)
    : QGLAbstractSurface(QGLAbstractSurface::FramebufferObject)
    , d_ptr(new QGLFramebufferObjectSurfacePrivate)
{
    setFramebufferObject(fbo);
    setContext(context);
}

/*!
    Destroys this framebuffer object surface.
*/
QGLFramebufferObjectSurface::~QGLFramebufferObjectSurface()
{
}

/*!
    \reimp
*/
bool QGLFramebufferObjectSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_UNUSED(prevSurface);
    bool success = false;
    if (context()) {
        if (!QOpenGLContext::areSharing(QOpenGLContext::currentContext(), context()))
        {
            context()->makeCurrent(context()->surface());
        }
    } else {
        setContext(QOpenGLContext::currentContext());
    }

    if (isValid())
    {
        success = framebufferObject()->bind();
    }
#ifndef QT_NO_DEBUG_STREAM
    else
    {
        qWarning() << "Attempt to activate invalid fbo surface";
    }
#endif
    return success;
}

/*!
    \reimp
*/
void QGLFramebufferObjectSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    if (framebufferObject()) {
        if (nextSurface && nextSurface->surfaceType() == FramebufferObject) {
            // If we are about to switch to another fbo that is on the
            // same context, then don't bother doing the release().
            // This saves an unnecessary glBindFramebuffer(0) call.
            if (static_cast<QGLFramebufferObjectSurface *>(nextSurface)
                    ->context() == context())
                return;
        }
        framebufferObject()->release();
    }
}

/*!
    \reimp
*/
QRect QGLFramebufferObjectSurface::viewportGL() const
{
    if (framebufferObject())
        return QRect(QPoint(0, 0), framebufferObject()->size());
    else
        return QRect();
}

/*!
    \reimp
*/
bool QGLFramebufferObjectSurface::isValid() const
{
    return QGLAbstractSurface::isValid() && framebufferObject() && context();
}



/*!
    \class QGLPixelBufferSurface
    \brief The QGLPixelBufferSurface class represents a pixel buffer that is being used as an OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

/*!
    Constructs a default pixel buffer surface.  This constructor
    should be followed by a call to setPixelBuffer().
*/
QGLPixelBufferSurface::QGLPixelBufferSurface()
    : QGLAbstractSurface(QGLAbstractSurface::PixelBuffer)
{
}

/*!
    Constructs a pixel buffer surface for \a pbuffer.
*/
QGLPixelBufferSurface::QGLPixelBufferSurface(QGLPixelBuffer *pbuffer)
    : QGLAbstractSurface(QGLAbstractSurface::PixelBuffer)
{
    m_pb = pbuffer;
}

/*!
    Destroys this pixel buffer surface.
*/
QGLPixelBufferSurface::~QGLPixelBufferSurface()
{
}

/*!
    Returns the pixel buffer for this surface, or null if
    it has not been set yet.

    \sa setPixelBuffer()
*/
QGLPixelBuffer *QGLPixelBufferSurface::pixelBuffer() const
{
    return m_pb;
}

/*!
    Sets the framebuffer object for this surface to \a pbuffer.

    \sa pixelBuffer()
*/
void QGLPixelBufferSurface::setPixelBuffer
        (QGLPixelBuffer *pbuffer)
{
    m_pb = pbuffer;
}

/*!
    \reimp
*/
bool QGLPixelBufferSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_UNUSED(prevSurface);
    if (m_pb)
        return m_pb->makeCurrent();
    else
        return false;
}

/*!
    \reimp
*/
void QGLPixelBufferSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    // Nothing to do here - leave the context current.
    Q_UNUSED(nextSurface);
}

/*!
    \reimp
*/
QRect QGLPixelBufferSurface::viewportGL() const
{
    if (m_pb)
        return QRect(0, 0, m_pb->width(), m_pb->height());
    else
        return QRect();
}

bool QGLPixelBufferSurface::isValid() const
{
    return QGLAbstractSurface::isValid() && m_pb;
}



/*!
    \class QGLMaskedSurface
    \brief The QGLMaskedSurface class represents a masked copy of another OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal

    Masked surfaces are typically used to render red-cyan anaglyph images
    into an underlying surface().  For the left eye image, the mask()
    is set to RedMask | AlphaMask.  Then for the right eye image, the mask()
    is set to GreenMask | BlueMask.
*/

/*!
    \enum QGLMaskedSurface::BufferMaskBit
    This enum defines the channels to mask with QGLMaskedSurface.

    \value RedMask Allow the red channel to be written to the color buffer.
    \value GreenMask Allow the green channel to be written to the color buffer.
    \value BlueMask Allow the blue channel to be written to the color buffer.
    \value AlphaMask Allow the alpha channel to be written to the color buffer.
*/

class QGLMaskedSurfacePrivate
{
public:
    QGLMaskedSurfacePrivate
        (QGLAbstractSurface *surf = 0,
         QGLMaskedSurface::BufferMask msk = QGLMaskedSurface::RedMask |
                                            QGLMaskedSurface::GreenMask |
                                            QGLMaskedSurface::BlueMask |
                                            QGLMaskedSurface::AlphaMask)
            : surface(surf), mask(msk) {}

    QGLAbstractSurface *surface;
    QGLMaskedSurface::BufferMask mask;
};

#define MaskedSurfaceType       501

/*!
    Constructs a masked OpenGL drawing surface with surface() initially
    set to null and mask() initially set to allow all channels to be
    written to the color buffer.
*/
QGLMaskedSurface::QGLMaskedSurface()
    : QGLAbstractSurface(MaskedSurfaceType)
    , d_ptr(new QGLMaskedSurfacePrivate)
{
}

/*!
    Constructs a masked OpenGL drawing surface that applies \a mask
    to \a surface when activate() is called.
*/
QGLMaskedSurface::QGLMaskedSurface
        (QGLAbstractSurface *surface, QGLMaskedSurface::BufferMask mask)
    : QGLAbstractSurface(MaskedSurfaceType)
    , d_ptr(new QGLMaskedSurfacePrivate(surface, mask))
{
}

/*!
    Destroys this masked OpenGL drawing surface.
*/
QGLMaskedSurface::~QGLMaskedSurface()
{
}

/*!
    Returns the underlying surface that mask() will be applied to
    when activate() is called.

    \sa setSurface(), mask()
*/
QGLAbstractSurface *QGLMaskedSurface::surface() const
{
    Q_D(const QGLMaskedSurface);
    return d->surface;
}

/*!
    Sets the underlying \a surface that mask() will be applied to
    when activate() is called.

    \sa surface(), setMask()
*/
void QGLMaskedSurface::setSurface(QGLAbstractSurface *surface)
{
    Q_D(QGLMaskedSurface);
    d->surface = surface;
}

/*!
    Returns the color mask to apply to surface() when activate()
    is called.

    \sa setMask(), surface()
*/
QGLMaskedSurface::BufferMask QGLMaskedSurface::mask() const
{
    Q_D(const QGLMaskedSurface);
    return d->mask;
}

/*!
    Sets the color \a mask to apply to surface() when activate()
    is called.

    \sa mask(), setSurface()
*/
void QGLMaskedSurface::setMask(QGLMaskedSurface::BufferMask mask)
{
    Q_D(QGLMaskedSurface);
    d->mask = mask;
}

/*!
    \reimp
*/
bool QGLMaskedSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_D(const QGLMaskedSurface);
    if (!d->surface || !d->surface->activate(prevSurface))
        return false;
    glColorMask((d->mask & RedMask) != 0, (d->mask & GreenMask) != 0,
                (d->mask & BlueMask) != 0, (d->mask & AlphaMask) != 0);
    return true;
}

/*!
    \reimp
*/
void QGLMaskedSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    Q_D(QGLMaskedSurface);
    if (d->surface)
        d->surface->deactivate(nextSurface);
    if (nextSurface && nextSurface->surfaceType() == MaskedSurfaceType) {
        // If we are about to switch to another masked surface for
        // the same underlying surface, then don't bother calling
        // glColorMask() for this one.
        QGLMaskedSurface *next = static_cast<QGLMaskedSurface *>(nextSurface);
        if (d->surface == next->surface())
            return;
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

/*!
    \reimp
*/
QRect QGLMaskedSurface::viewportGL() const
{
    Q_D(const QGLMaskedSurface);
    return d->surface ? d->surface->viewportGL() : QRect();
}

bool QGLMaskedSurface::isValid() const
{
    return QGLAbstractSurface::isValid();
}



/*!
    \class QGLSubsurface
    \brief The QGLSubsurface class represents a sub-surface of another OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

class QGLSubsurfacePrivate
{
public:
    QGLSubsurfacePrivate() : surface(0) {}
    QGLSubsurfacePrivate(QGLAbstractSurface *surf, const QRect &rgn)
        : surface(surf), region(rgn) {}

    QGLAbstractSurface *surface;
    QRect region;
};

/*!
    Constructs a new subsurface.  This constructor should be followed
    by calls to setSurface() and setRegion().
*/
QGLSubsurface::QGLSubsurface()
    : QGLAbstractSurface(QGLAbstractSurface::Subsurface)
    , d_ptr(new QGLSubsurfacePrivate)
{
}

/*!
    Constructs a new subsurface that occupies \a region within
    \a surface.  The \a region has its origin at the top-left
    of \a surface.
*/
QGLSubsurface::QGLSubsurface
        (QGLAbstractSurface *surface, const QRect &region)
    : QGLAbstractSurface(QGLAbstractSurface::Subsurface)
    , d_ptr(new QGLSubsurfacePrivate(surface, region))
{
}

/*!
    Destroys this subsurface.
*/
QGLSubsurface::~QGLSubsurface()
{
}

/*!
    Returns the surface behind this subsurface, or null if the
    surface has not been set.

    \sa setSurface(), region()
*/
QGLAbstractSurface *QGLSubsurface::surface() const
{
    Q_D(const QGLSubsurface);
    return d->surface;
}

/*!
    Sets the \a surface behind this subsurface.

    \sa surface(), setRegion()
*/
void QGLSubsurface::setSurface(QGLAbstractSurface *surface)
{
    Q_D(QGLSubsurface);
    d->surface = surface;
}

/*!
    Returns the region within surface() that is occupied by this
    subsurface, relative to the viewportRect() of surface().
    The origin is at the top-left of surface().

    \sa setRegion(), surface()
*/
QRect QGLSubsurface::region() const
{
    Q_D(const QGLSubsurface);
    return d->region;
}

/*!
    Sets the \a region within surface() that is occupied by this
    subsurface, relative to the viewportRect() of surface().
    The origin is at the top-left of surface().

    \sa region(), setSurface()
*/
void QGLSubsurface::setRegion(const QRect &region)
{
    Q_D(QGLSubsurface);
    d->region = region;
}

/*!
    \reimp
*/
bool QGLSubsurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_D(QGLSubsurface);
    if (d->surface)
        return d->surface->activate(prevSurface);
    else
        return false;
}

/*!
    \reimp
*/
void QGLSubsurface::deactivate(QGLAbstractSurface *nextSurface)
{
    Q_D(QGLSubsurface);
    if (d->surface)
        d->surface->deactivate(nextSurface);
}

/*!
    \reimp
*/
QRect QGLSubsurface::viewportGL() const
{
    Q_D(const QGLSubsurface);
    if (d->surface) {
        // The underlying surface's viewportGL() has its origin
        // at the bottom-left, whereas d->region has its origin
        // at the top-left.  Flip the sub-region and adjust.
        QRect rect = d->surface->viewportGL();
        return QRect(rect.x() + d->region.x(),
                     rect.y() + rect.height() -
                        (d->region.y() + d->region.height()),
                     d->region.width(), d->region.height());
    } else {
        // Don't know the actual height of the surrounding surface,
        // so the best we can do is assume the region is bottom-aligned.
        return QRect(d->region.x(), 0, d->region.width(), d->region.height());
    }
}

bool QGLSubsurface::isValid() const
{
    return QGLAbstractSurface::isValid();
}



/*!
    \class QGLWindowSurface
    \brief The QGLWindowSurface class represents a QGLwindow that is begin used as an OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

/*!
    Constructs a window surface, with a null window.  This constructor should be
    followed by a call to setWindow().
*/
QGLWindowSurface::QGLWindowSurface()
    : QGLAbstractSurface(QGLAbstractSurface::Window)
{
}

/*!
    Constructs a window surface for \a window.  If \a window is null,
    then this surface is invalid, that is isValid() will return null.
*/
QGLWindowSurface::QGLWindowSurface(QWindow *window)
    : QGLAbstractSurface(QGLAbstractSurface::Window)
{
    setWindow(window);
}

/*!
    Destroys this window surface.
*/
QGLWindowSurface::~QGLWindowSurface()
{
    // we don't own the window - don't destroy it here
}

/*!
    \reimp
*/
bool QGLWindowSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_UNUSED(prevSurface);
    Q_ASSERT_X(QOpenGLContext::currentContext() || context(), Q_FUNC_INFO,
               "Activating GL window surface without GL context");
    if (context())
    {
        if (context() != QOpenGLContext::currentContext())
        {
            context()->makeCurrent(window());
        }
    }
    else
    {
        setContext(QOpenGLContext::currentContext());
    }
    if (window())
    {
#ifndef QT_NO_DEBUG_STREAM
        if (context()->surface() != window())
            qWarning() << "Attempt to activate GL window surface on wrong context";
#endif
    }
    else
    {
        setWindow(static_cast<QWindow*>(context()->surface()));
    }
#ifndef QT_NO_DEBUG_STREAM
    if (!context()->surface() || context()->surface()->surfaceClass() != QSurface::Window)
        qWarning() << "Attempt to activate GL window surface on bad context";
    if (!isValid())
    {
        qWarning() << "Attempt to activate invalid window surface";
        if (window() && !window()->geometry().isValid())
        {
            qWarning() << "Maybe set the window size, eg view.resize(800, 600)..?";
        }
    }
#endif
    return isValid();
}

/*!
    \reimp
*/
void QGLWindowSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    // Nothing to do here - leave the context current.
    Q_UNUSED(nextSurface);
}

/*!
    \reimp
*/
QRect QGLWindowSurface::viewportGL() const
{
    if (window()) {
        QRect geom = window()->geometry();
        return QRect(0,0,geom.width(),geom.height());
    } else
        return QRect();
}

/*!
    \reimp
*/
bool QGLWindowSurface::isValid() const
{
    return QGLAbstractSurface::isValid() && window();
}



#ifdef QT_WIDGETS_LIB
/*!
    \class QGLWidgetSurface
    \brief The QGLWidgetSurface class represents a QGLWidget that is begin used as an OpenGL drawing surface.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

/*!
    Constructs a widget surface.  This constructor should be followed
    by a call to setWidget().
*/
QGLWidgetSurface::QGLWidgetSurface()
    : QGLAbstractSurface(QGLAbstractSurface::Window)
    , m_widget(0)
{
}

/*!
    Constructs a widget surface for \a widget.
*/
QGLWidgetSurface::QGLWidgetSurface(QOpenGLWidget *widget)
    : QGLAbstractSurface(QGLAbstractSurface::Window)
    , m_widget(widget)
{
}

/*!
    Destroys this widget surface.
*/
QGLWidgetSurface::~QGLWidgetSurface()
{
}

/*!
    Returns the widget that is underlying this surface.

    \sa setWidget()
*/
QOpenGLWidget *QGLWidgetSurface::widget() const
{
    return m_widget;
}

/*!
    Sets the \a widget that is underlying this surface.

    \sa widget()
*/
void QGLWidgetSurface::setWidget(QOpenGLWidget *widget)
{
    m_widget = widget;
}

/*!
    \reimp
*/
QPaintDevice *QGLWidgetSurface::device() const
{
    return m_widget;
}

/*!
    \reimp
*/
bool QGLWidgetSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_UNUSED(prevSurface);
    if (m_widget) {
        if (QOpenGLContext::currentContext() != m_widget->context())
            m_widget->makeCurrent();
        return true;
    } else {
        return false;
    }
}

/*!
    \reimp
*/
void QGLWidgetSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    // Nothing to do here - leave the context current.
    Q_UNUSED(nextSurface);
}

/*!
    \reimp
*/
QRect QGLWidgetSurface::viewportGL() const
{
    if (m_widget)
        return m_widget->rect();    // Origin assumed to be (0, 0).
    else
        return QRect();
}
#endif // QT_WIDGETS_LIB



bool QGLContextSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_UNUSED(prevSurface);
    Q_ASSERT_X(QOpenGLContext::currentContext() || context(),
               Q_FUNC_INFO,
               "Activating GL contex surface without GL context");
    if (context())
    {
        if (context() != QOpenGLContext::currentContext())
        {
            context()->makeCurrent(window());
        }
    }
    else
    {
        setContext(QOpenGLContext::currentContext());
    }
    // Once we have used this context with a window remember that window and
    // complain if it is used with another window, since that will affect the
    // viewport and other rendering assumptions.
    if (!window())
    {
#ifndef QT_NO_DEBUG_STREAM
        if (!context()->surface() || context()->surface()->surfaceClass() == QSurface::Window)
            qWarning() << "Attempt to access context without GL window";
#endif
        setWindow(static_cast<QWindow*>(context()->surface()));
    }
#ifndef QT_NO_DEBUG_STREAM
    else
    {
        if (context()->surface() != window())
            qWarning() << "Attempt to render in wrong window for context";
    }
#endif
    return isValid();
}

void QGLContextSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    Q_UNUSED(nextSurface);
}

QRect QGLContextSurface::viewportGL() const
{
    if (window())
    {
        QRect geom = window()->geometry();
        return QRect(0,0,geom.width(),geom.height());
    }
#ifndef QT_NO_DEBUG_STREAM
    else
    {
        qWarning() << "Attempt to get viewport rect with no window\n"
                      << "Call activate() first";
    }
#endif
    return QRect();
}

bool QGLContextSurface::isValid() const
{
    bool winOK = true;
    if (window())
        winOK = window()->surfaceType() == QWindow::OpenGLSurface;
    return isValid() && winOK;
}



bool QGLPainterSurface::activate(QGLAbstractSurface *prevSurface)
{
    Q_ASSERT_X(QOpenGLContext::currentContext() || context(), Q_FUNC_INFO,
               "Activating GL painter surface without GL context");
    if (context())
    {
        if (context() != QOpenGLContext::currentContext())
        {
            context()->makeCurrent(context()->surface());
        }
    }
    else
    {
        setContext(QOpenGLContext::currentContext());
    }
    if (!prevSurface)
        m_painter->beginNativePainting();
    return true;
}

void QGLPainterSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    if (!nextSurface)
        m_painter->endNativePainting();
}

QRect QGLPainterSurface::viewportGL() const
{
    QPaintDevice *device = m_painter->device();
    return QRect(0, 0, device->width(), device->height());
}

bool QGLPainterSurface::isValid() const
{
    return QGLAbstractSurface::isValid();
}



bool QGLDrawBufferSurface::activate(QGLAbstractSurface *prevSurface)
{
    if (!m_surface->activate(prevSurface))
        return false;
#if defined(GL_BACK_LEFT) && defined(GL_BACK_RIGHT)
    glDrawBuffer(m_buffer);
#endif
    return true;
}

void QGLDrawBufferSurface::deactivate(QGLAbstractSurface *nextSurface)
{
    m_surface->deactivate(nextSurface);
}

QRect QGLDrawBufferSurface::viewportGL() const
{
    return m_surface->viewportGL();
}

bool QGLDrawBufferSurface::isValid() const
{
    return QGLAbstractSurface::isValid();
}

QT_END_NAMESPACE
