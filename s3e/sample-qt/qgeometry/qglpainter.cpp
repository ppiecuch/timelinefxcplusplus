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

#include "qglpainter.h"
#include "qglpainter_p.h"
#include "qglext_p.h"

#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QPaintEngine>
#include <QVarLengthArray>
#include <QMap>
#include <QRgb>
#include <QDebug>

#if !defined(QT_NO_THREAD)
#include <QThreadStorage>
#include <QThread>
#endif

#include "qgleffect_p.h"
#include "qgltexture2d.h"
#include "qgeometrydata.h"
#include "qglvertexbundle_p.h"
#include "qmatrix4x4stack_p.h"
#include "qglsurface_p.h"

#include <cmath>

#undef glActiveTexture

QT_BEGIN_NAMESPACE

QRgb qt_qgl_pick_color(int index);
QRgb qt_qgl_normalize_pick_color(QRgb color, bool is444 = false);

/*!
    \class QGLPainter
    \brief The QGLPainter class provides portable API's for rendering into a GL context.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting

    TBD - lots of TBD

    All QGLPainter instances on a context share the same context state:
    matrices, effects, vertex attributes, etc.  For example, calling
    ortho() on one QGLPainter instance for a context will alter the
    projectionMatrix() as seen by the other QGLPainter instances.
*/

/*!
    \enum QGLPainter::Update
    This enum defines the values that were changed since the last QGLPainter::update().

    \value UpdateColor The color has been updated.
    \value UpdateModelViewMatrix The modelview matrix has been updated.
    \value UpdateProjectionMatrix The projection matrix has been updated.
    \value UpdateMatrices The combination of UpdateModelViewMatrix and
           UpdateProjectionMatrix.
    \value UpdateLights The lights have been updated.
    \value UpdateMaterials The material parameters have been updated.
    \value UpdateViewport The viewport needs to be updated because the
           drawing surface has changed.
    \value UpdateAll All values have been updated.  This is specified
           when an effect is activated.
*/

#define QGLPAINTER_CHECK_PRIVATE() \
    Q_ASSERT_X(d, "QGLPainter", "begin() has not been called or it failed")

QGLPainterPrivate::QGLPainterPrivate()
    : ref(1),
      badShaderCount(0),
      eye(QGL::NoEye),
      lightModel(0),
      defaultLightModel(0),
      defaultLight(0),
      frontMaterial(0),
      backMaterial(0),
      defaultMaterial(0),
      frontColorMaterial(0),
      backColorMaterial(0),
      viewingCube(QVector3D(-1, -1, -1), QVector3D(1, 1, 1)),
      color(255, 255, 255, 255),
      updates(QGLPainter::UpdateAll),
      pick(0),
      boundVertexBuffer(0),
      boundIndexBuffer(0),
      isFixedFunction(true) // Updated by QGLPainter::begin()
{
    context = 0;
    effect = 0;
    userEffect = 0;
    standardEffect = QGL::FlatColor;
    memset(stdeffects, 0, sizeof(stdeffects));
}

QGLPainterPrivate::~QGLPainterPrivate()
{
    delete defaultLightModel;
    delete defaultLight;
    delete defaultMaterial;
    delete frontColorMaterial;
    delete backColorMaterial;
    for (int effect = 0; effect < QGL_MAX_STD_EFFECTS; ++effect)
        delete stdeffects[effect];
    delete pick;
    qDeleteAll(cachedPrograms);
}

QGLPainterPickPrivate::QGLPainterPickPrivate()
{
    isPicking = false;
    objectPickId = -1;
    pickColorIndex = -1;
    pickColor = 0;
    defaultPickEffect = new QGLFlatColorEffect();
}

QGLPainterPickPrivate::~QGLPainterPickPrivate()
{
    delete defaultPickEffect;
}

#if !defined(QT_NO_THREAD)

// QOpenGLContext's are thread-specific, so QGLPainterPrivateCache should be too.

typedef QThreadStorage<QGLPainterPrivateCache *> QGLPainterPrivateStorage;
Q_GLOBAL_STATIC(QGLPainterPrivateStorage, painterPrivateStorage)
static QGLPainterPrivateCache *painterPrivateCache()
{
    QGLPainterPrivateCache *cache = painterPrivateStorage()->localData();
    if (!cache) {
        cache = new QGLPainterPrivateCache();
        painterPrivateStorage()->setLocalData(cache);
    }
    return cache;
}

#else

Q_GLOBAL_STATIC(QGLPainterPrivateCache, painterPrivateCache)

#endif

QGLPainterPrivateCache::QGLPainterPrivateCache()
{
}

QGLPainterPrivateCache::~QGLPainterPrivateCache()
{
}

QGLPainterPrivate *QGLPainterPrivateCache::fromContext(QOpenGLContext *context)
{
    QGLPainterPrivate *priv = cache.value(context, 0);
    if (priv)
        return priv;
#ifndef QT_NO_THREAD
    Q_ASSERT_X(context->thread() == QThread::currentThread(),
               Q_FUNC_INFO,
               "Attempt to fetch painter state for context outside contexts thread");
#endif
    // since we assert this is the same thread then this is bound to be a direct
    // connection, not a queued (asynchronous) connection
    connect(context, SIGNAL(destroyed()), this, SLOT(contextDestroyed()));
    priv = new QGLPainterPrivate();
    priv->context = context;
    cache.insert(context, priv);
    return priv;
}

QGLPainterPrivateCache *QGLPainterPrivateCache::instance()
{
    return painterPrivateCache();
}

void QGLPainterPrivateCache::contextDestroyed()
{
    QOpenGLContext *context = qobject_cast<QOpenGLContext *>(sender());
    QGLPainterPrivate *priv = cache.value(context, 0);
    if (priv) {
        priv->context = 0;
        cache.remove(context);
        if (!priv->ref.deref())
            delete priv;
    }
    emit destroyedContext(context);
}

/*!
    Constructs a new GL painter.  Call begin() to attach the
    painter to a GL context.

    \sa begin()
*/
QGLPainter::QGLPainter()
    : d_ptr(0)
{
}

/*!
    Constructs a new GL painter and attaches it to \a context.
    It is not necessary to call begin() after construction.

    \sa begin()
*/
QGLPainter::QGLPainter(QOpenGLContext *context)
    : d_ptr(0)
{
    begin(context);
}

/*!
    Constructs a new GL painter and attaches it to the GL
    context associated with \a window.  It is not necessary to
    call begin() after construction.

    \sa begin(), isActive()
*/
QGLPainter::QGLPainter(QWindow *window)
    : d_ptr(0)
{
    begin(window);
}

/*!
    Constructs a new GL painter and attaches it to the GL context associated
    with \a painter.  It is assumed that \a painter is the currently
    active painter and that it is associated with the current GL context.

    If \a painter is not using an OpenGL paint engine, then isActive()
    will return false; true otherwise.

    This constructor is typically used when mixing regular Qt painting
    operations and GL painting operations on a widget that is being
    drawn using the OpenGL graphics system.

    \sa begin(), isActive()
*/
QGLPainter::QGLPainter(QPainter *painter)
    : d_ptr(0)
{
    begin(painter);
}

/*!
    Constructs a new GL painter and attaches it to the GL context associated
    with \a surface.

    \sa begin(), isActive()
*/
QGLPainter::QGLPainter(QGLAbstractSurface *surface)
    : d_ptr(0)
{
    begin(surface);
}

/*!
    Destroys this GL painter.
*/
QGLPainter::~QGLPainter()
{
    end();
}

/*!
    Begins painting on the current GL context.  Returns false
    if there is no GL context current.

    \sa end()
*/
bool QGLPainter::begin()
{
    return begin(QOpenGLContext::currentContext());
}

/*!
    Begins painting on \a context.  If painting was already in progress,
    then this function will call end() first.  The \a context will be
    made current if it is not already current.

    Returns true if painting can begin; false otherwise.

    All QGLPainter instances on a context share the same context state:
    matrices, the effect(), vertex attributes, etc.  For example,
    calling ortho() on one QGLPainter instance for a context will
    alter the projectionMatrix() as seen by the other QGLPainter instances.

    \sa end(), isActive()
*/
bool QGLPainter::begin(QOpenGLContext *context)
{
    if (!context)
        return false;
    end();
    return begin(context, QGLAbstractSurface::createSurfaceForContext(context));
}

/*!
    \internal
*/
bool QGLPainter::begin
    (QOpenGLContext *context, QGLAbstractSurface *surface,
     bool destroySurface)
{
    // If we don't have a context specified, then use the one
    // that the surface just made current.
    if (!context)
        context = QOpenGLContext::currentContext();

    if (!context)
    {
        qWarning() << "##### Attempt to begin painter with no GL context!";
        return false;
    }

    // Initialize the QOpenGLFunctions parent class.
    initializeOpenGLFunctions();

    // Determine if the OpenGL implementation is fixed-function or not.
    bool isFixedFunction = !hasOpenGLFeature(QOpenGLFunctions::Shaders);
    if (!isFixedFunction)
        isFixedFunction = !QOpenGLShaderProgram::hasOpenGLShaderPrograms();
#ifdef Q_OS_WIN
    // On windows platforms in a virtual environment the hasOpenGLFunctions can report shaders are
    // available when in fact they are not, and the only effective test for the presence of
    // shaders is checking for extensions handling important shader functionality.
    // We need to support this environment to run autotests.
    // On non-windows platforms native support can mean that shaders are available without extensions,
    // so in the general case, we trust hasOpenGLFeature/hasOpenGLShaderPrograms, and on windows we
    // check for the relevant extensions.
    if (!isFixedFunction)

    {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        QFunctionPointer res = ctx->getProcAddress("glCreateShader");
        if (!res)
        {
            res = ctx->getProcAddress("glCreateShaderObject");
            if (!res)
            {
                res = ctx->getProcAddress("glCreateShaderObjectARB");
            }
        }
        if (!res)
            isFixedFunction = !res;
    }
#endif

    // Find the QGLPainterPrivate for the context, or create a new one.
    d_ptr = painterPrivateCache()->fromContext(context);
    d_ptr->ref.ref();
    d_ptr->isFixedFunction = isFixedFunction;

    // Activate the main surface for the context.
    QGLAbstractSurface *prevSurface;
    if (d_ptr->surfaceStack.isEmpty()) {
        prevSurface = 0;
    } else {
        // We are starting a nested begin()/end() scope, so switch
        // to the new main surface rather than activate from scratch.
        prevSurface = d_ptr->surfaceStack.last().surface;
        prevSurface->deactivate(surface);
    }
    if (!surface->activate(prevSurface)) {
        if (prevSurface)
            prevSurface->activate(surface);
        if (destroySurface)
            delete surface;
        if (!d_ptr->ref.deref())
            delete d_ptr;
        d_ptr = 0;
        return false;
    }

    // Push a main surface descriptor onto the surface stack.
    QGLPainterSurfaceInfo psurf;
    psurf.surface = surface;
    psurf.destroySurface = destroySurface;
    psurf.mainSurface = true;
    d_ptr->surfaceStack.append(psurf);

    // Force the matrices to be updated the first time we use them.
    d_ptr->modelViewMatrix.setDirty(true);
    d_ptr->projectionMatrix.setDirty(true);

    return true;
}

/*!
    Begins GL painting on \a widget.  Returns false if \a widget is null.

    \sa end()
*/
bool QGLPainter::begin(QWindow *window)
{
    bool result = false;
    if (window)
    {
        end();
        result = begin(0, new QGLWindowSurface(window));
    }
    return result;
}

/*!
    Begins painting on the GL context associated with \a painter.
    Returns false if \a painter is not using an OpenGL paint engine.
    It is assumed that \a painter is the currently active painter
    and that it is associated with the current GL context.

    This function is typically used when mixing regular Qt painting
    operations and GL painting operations on a widget that is being
    drawn using the OpenGL graphics system.

    \sa end()
*/
bool QGLPainter::begin(QPainter *painter)
{
    // Validate that the painting is OpenGL-based.
    if (!painter)
        return false;
    QPaintEngine *engine = painter->paintEngine();
    if (!engine)
        return false;
    if (engine->type() != QPaintEngine::OpenGL &&
            engine->type() != QPaintEngine::OpenGL2)
        return false;

    // Begin GL painting operations.
    return begin(0, new QGLPainterSurface(painter));
}

/*!
    Begins painting to \a surface.  Returns false if \a surface is
    null or could not be activated.

    \sa end(), QGLAbstractSurface::activate()
*/
bool QGLPainter::begin(QGLAbstractSurface *surface)
{
    if (!surface)
        return false;
    end();
    return begin(0, surface, false);
}

/*!
    Ends GL painting.  Returns true if painting was ended successfully;
    false if this painter was not bound to a GL context.

    The GL context that was bound to this painter will not have
    QOpenGLContext::doneCurrent() called on it.  It is the responsibility
    of the caller to terminate context operations.

    The effect() will be left active in the GL context and will be
    assumed to still be active the next time begin() is called.
    If this assumption doesn't apply, then call disableEffect()
    to disable the effect before calling end().

    This function will pop all surfaces from the surface stack,
    and return currentSurface() to null (the default drawing surface).

    \sa begin(), isActive(), disableEffect()
*/
bool QGLPainter::end()
{
    Q_D(QGLPainter);
    if (!d)
        return false;

    // Unbind the current vertex and index buffers.
    if (d->boundVertexBuffer) {
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
        d->boundVertexBuffer = 0;
    }
    if (d->boundIndexBuffer) {
        QOpenGLBuffer::release(QOpenGLBuffer::IndexBuffer);
        d->boundIndexBuffer = 0;
    }

    // Pop surfaces from the surface stack until we reach a
    // main surface.  Then deactivate the main surface.
    int size = d->surfaceStack.size();
    while (size > 0) {
        --size;
        QGLPainterSurfaceInfo &surf = d->surfaceStack[size];
        if (surf.mainSurface) {
            if (size > 0) {
                // There are still other surfaces on the stack, probably
                // because we are within a nested begin()/end() scope.
                // Re-activate the next surface down in the outer scope.
                QGLPainterSurfaceInfo &nextSurf = d->surfaceStack[size - 1];
                surf.surface->switchTo(nextSurf.surface);
            } else {
                // Last surface on the stack, so deactivate it permanently.
                surf.surface->deactivate();
            }
            if (surf.destroySurface)
                delete surf.surface;
            break;
        } else if (size > 0) {
            surf.surface->deactivate(d->surfaceStack[size - 1].surface);
        }
    }
    d->surfaceStack.resize(size);

    // Force a viewport update if we are within a nested begin()/end().
    d->updates |= UpdateViewport;

    // Destroy the QGLPainterPrivate if this is the last reference.
    if (!d->ref.deref())
        delete d;
    d_ptr = 0;
    return true;
}

/*!
    Returns true if this painter is currently bound to a GL context;
    false otherwise.

    \sa begin(), end()
*/
bool QGLPainter::isActive() const
{
    return (d_ptr != 0 && d_ptr->context != 0);
}

/*!
    Returns the GL context that is bound to this painter, or null
    if it is not currently bound.
*/
QOpenGLContext *QGLPainter::context() const
{
    if (d_ptr)
        return d_ptr->context;
    else
        return 0;
}

/*!
    Returns true if the underlying OpenGL implementation is OpenGL 1.x
    or OpenGL/ES 1.x and only supports fixed-function OpenGL operations.
    Returns false if the underlying OpenGL implementation is using
    GLSL or GLSL/ES shaders.

    If this function returns false, then the built-in effects will
    use shaders and QGLPainter will not update the fixed-function
    matrices in the OpenGL context when update() is called.
    User-supplied effects will need to use shaders also or update
    the fixed-function matrices themselves or call updateFixedFunction().

    \sa update(), updateFixedFunction()
*/
bool QGLPainter::isFixedFunction() const
{
#if defined(QT_OPENGL_ES_2)
    return false;
#else
    Q_D(const QGLPainter);
    if (d)
        return d->isFixedFunction;
    else
        return true;
#endif
}

/*!
    Sets the \a color to use to clear the color buffer when \c{glClear()}
    is called.
*/
void QGLPainter::setClearColor(const QColor& color)
{
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

/*!
    Sets the scissor \a rect for the current drawing surface
    to use when \c{GL_SCISSOR_TEST} is enabled.  If \a rect is empty,
    then the scissor will be set to clip away all drawing.

    Note that \a rect is in Qt co-ordinates with the origin
    at the top-left of the drawing surface's viewport rectangle.
    If the currentSurface() is an instance of QGLSubsurface,
    then \a rect will be adjusted relative to the subsurface's position.

    \sa currentSurface(), QGLAbstractSurface::viewportGL()
*/
void QGLPainter::setScissor(const QRect& rect)
{
    if (!rect.isEmpty()) {
        // Adjust the rectangle by the position of the surface viewport.
        QGLAbstractSurface *surface = currentSurface();
        QRect viewport = surface->viewportGL();
        QRect r(viewport.x() + rect.x(),
                viewport.y() + viewport.height() - (rect.y() + rect.height()),
                rect.width(), rect.height());
        if (!r.isEmpty())
            glScissor(r.x(), r.y(), r.width(), r.height());
        else
            glScissor(0, 0, 0, 0);
    } else {
        glScissor(0, 0, 0, 0);
    }
}

/*!
    Returns a reference to the projection matrix stack.

    It is recommended that setCamera() be used to set the projection
    matrix at the beginning of a scene rendering pass so that the
    eye position can be adjusted for stereo.

    \sa modelViewMatrix(), combinedMatrix(), setCamera()
*/
QMatrix4x4Stack& QGLPainter::projectionMatrix()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->projectionMatrix;
}

/*!
    Returns a reference to the modelview matrix stack.

    \sa projectionMatrix(), combinedMatrix(), normalMatrix(), setCamera()
    \sa worldMatrix()
*/
QMatrix4x4Stack& QGLPainter::modelViewMatrix()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->modelViewMatrix;
}

/*!
    \fn QMatrix4x4 QGLPainter::combinedMatrix() const

    Returns the result of multiplying the projectionMatrix()
    and the modelViewMatrix().  This combined matrix value is
    useful for setting uniform matrix values on shader programs.

    Calling this function is more efficient than calling
    projectionMatrix() and modelViewMatrix() separately and
    multiplying the return values.

    \sa projectionMatrix(), modelViewMatrix(), normalMatrix()
*/
QMatrix4x4 QGLPainter::combinedMatrix() const
{
    const QGLPainterPrivate *d = d_func();
    if (!d)
        return QMatrix4x4();
    const QMatrix4x4StackPrivate *proj = d->projectionMatrix.d_func();
    const QMatrix4x4StackPrivate *mv = d->modelViewMatrix.d_func();
    return proj->matrix * mv->matrix;
}

// Inverting the eye transformation will often result in values like
// 1.5e-15 in the world matrix.  Clamp these to zero to make worldMatrix()
// more stable when removing the eye component of the modelViewMatrix().
static inline float qt_gl_stablize_value(float value)
{
    return (qAbs(value) >= 0.00001f) ? value : 0.0f;
}
static inline QMatrix4x4 qt_gl_stablize_matrix(const QMatrix4x4 &m)
{
    return QMatrix4x4(qt_gl_stablize_value(m(0, 0)),
                      qt_gl_stablize_value(m(0, 1)),
                      qt_gl_stablize_value(m(0, 2)),
                      qt_gl_stablize_value(m(0, 3)),
                      qt_gl_stablize_value(m(1, 0)),
                      qt_gl_stablize_value(m(1, 1)),
                      qt_gl_stablize_value(m(1, 2)),
                      qt_gl_stablize_value(m(1, 3)),
                      qt_gl_stablize_value(m(2, 0)),
                      qt_gl_stablize_value(m(2, 1)),
                      qt_gl_stablize_value(m(2, 2)),
                      qt_gl_stablize_value(m(2, 3)),
                      qt_gl_stablize_value(m(3, 0)),
                      qt_gl_stablize_value(m(3, 1)),
                      qt_gl_stablize_value(m(3, 2)),
                      qt_gl_stablize_value(m(3, 3)));
}

/*!
    Returns the world matrix, which is the modelViewMatrix() without
    the eye transformation that was set in the previous call to
    setCamera().

    In the following example, the \c{world} variable will be set to the
    translation and scale component of the modelview transformation,
    without the "look at" component from the camera:

    \code
    painter.setCamera(camera);
    painter.modelViewMatrix().translate(0.0f, 5.0f, 0.0f);
    painter.modelViewMatrix().scale(1.5f);
    QMatrix4x4 world = painter.worldMatrix();
    \endcode

    Note: the world matrix is determined by multiplying the inverse of
    the camera's look at component with the current modelview matrix.
    Thus, the result may not be precisely the same as constructing a
    matrix from translate and scale operations starting with the identity.

    \sa modelViewMatrix(), setCamera()
*/
QMatrix4x4 QGLPainter::worldMatrix() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return qt_gl_stablize_matrix
        (d->inverseEyeMatrix * d->modelViewMatrix.top());
}

/*!
    \fn QMatrix3x3 QGLPainter::normalMatrix() const

    Returns the normal matrix corresponding to modelViewMatrix().

    The normal matrix is the transpose of the inverse of the top-left
    3x3 part of the 4x4 modelview matrix.  If the 3x3 sub-matrix is not
    invertible, this function returns the identity.

    \sa modelViewMatrix(), combinedMatrix()
*/
QMatrix3x3 QGLPainter::normalMatrix() const
{
    const QGLPainterPrivate *d = d_func();
    if (!d)
        return QMatrix3x3();
    const QMatrix4x4StackPrivate *mv = d->modelViewMatrix.d_func();
    return mv->matrix.normalMatrix();
}

/*!
    Returns the camera eye that is currently being used for stereo
    rendering.  The default is QGL::NoEye.

    The eye is used to adjust the camera position by a small amount
    when setCamera() is called.

    \sa setEye(), setCamera()
*/
QGL::Eye QGLPainter::eye() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->eye;
}

/*!
    Sets the camera \a eye that is currently being used for stereo
    rendering.

    The \a eye is used to adjust the camera position by a small amount
    when setCamera() is called.

    \sa eye(), setCamera()
*/
void QGLPainter::setEye(QGL::Eye eye)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->eye = eye;
}

/*!
    Sets the modelViewMatrix() and projectionMatrix() to the view
    defined by \a camera.  If eye() is not QGL::NoEye, then the view
    will be adjusted for the camera's eye separation.

    This function is typically called at the beginning of a scene rendering
    pass to initialize the modelview and projection matrices.

    Note that this does not cause the painter to take ownership of the camera
    and it does not save the pointer value.  The \a camera may be safely
    deleted after calling this function.

    \sa eye(), modelViewMatrix(), projectionMatrix(), worldMatrix()
*/
void QGLPainter::setCamera(const QGLCamera *camera)
{
    Q_ASSERT(camera);
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    QMatrix4x4 lookAt = camera->modelViewMatrix(d->eye);
    d->modelViewMatrix = lookAt;
    d->projectionMatrix = camera->projectionMatrix(aspectRatio());
    d->inverseEyeMatrix = lookAt.inverted();
}

/*!
    Returns true if \a point is outside the current viewing volume.
    This is used to perform object culling checks.
*/
bool QGLPainter::isCullable(const QVector3D& point) const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    QVector3D projected = d->modelViewMatrix * point;
    projected = d->projectionMatrix * projected;
    return !d->viewingCube.contains(projected);
}

static inline uint outcode(const QVector4D &v)
{
    // For a discussion of outcodes see pg 388 Dunn & Parberry.
    // For why you can't just test if the point is in a bounding box
    // consider the case where a view frustum with view-size 1.5 x 1.5
    // is tested against a 2x2 box which encloses the near-plane, while
    // all the points in the box are outside the frustum.
    // TODO: optimise this with assembler - according to D&P this can
    // be done in one line of assembler on some platforms
    uint code = 0;
    if (v.x() < -v.w()) code |= 0x01;
    if (v.x() > v.w())  code |= 0x02;
    if (v.y() < -v.w()) code |= 0x04;
    if (v.y() > v.w())  code |= 0x08;
    if (v.z() < -v.w()) code |= 0x10;
    if (v.z() > v.w())  code |= 0x20;
    return code;
}

/*!
    Returns true if \a box is completely outside the current viewing volume.
    This is used to perform object culling checks.
*/
bool QGLPainter::isCullable(const QBox3D& box) const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    // This function uses the technique of view frustum culling known as
    // clip space testing.  Since the normal QVector3D representation
    // of the points throws away the w value needed, we convert the box
    // into a set of 8 points represented as QVector4D's and then apply
    // the test.  The test is to transform the points into clip space
    // by applying the MV and Proj matrices, then test to see if the 4D
    // points are outside the clip space by testing x, y & z against w.
    QArray<QVector4D> box4d;
    QVector3D n = box.minimum();
    QVector3D x = box.maximum();
    box4d.append(QVector4D(n.x(), n.y(), x.z(), 1), QVector4D(x.x(), n.y(), x.z(), 1),
                 QVector4D(x.x(), x.y(), x.z(), 1), QVector4D(n.x(), x.y(), x.z(), 1));
    box4d.append(QVector4D(n.x(), n.y(), n.z(), 1), QVector4D(x.x(), n.y(), n.z(), 1),
                 QVector4D(x.x(), x.y(), n.z(), 1), QVector4D(n.x(), x.y(), n.z(), 1));
    QMatrix4x4 mvp = d->projectionMatrix.top() * d->modelViewMatrix.top();
    for (int i = 0; i < box4d.size(); ++i)
    {
        box4d[i] = mvp * box4d.at(i);
    }
    // if the logical AND of all the outcodes is non-zero then the BB is
    // definitely outside the view frustum.
    uint out = 0xff;
    for (int i = 0; i < box4d.size(); ++i)
    {
        out = out & outcode(box4d.at(i));
    }
    return out;
}

/*!
    Returns the aspect ratio of the viewport for adjusting projection
    transformations.
*/
float QGLPainter::aspectRatio() const
{
    return currentSurface()->aspectRatio();
}

/*!
    Returns the current effect that is in use, which is userEffect()
    if it is not null, or the effect object associated with
    standardEffect() otherwise.

    If isPicking() is true, then this will return the effect object
    that is being used to generate pick colors.

    \sa userEffect(), standardEffect(), isPicking()
*/
QGLAbstractEffect *QGLPainter::effect() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->ensureEffect(const_cast<QGLPainter *>(this));
    return d->effect;
}

/*!
    Returns the user-defined effect that is being used for drawing
    operations, or null if standardEffect() is in use.

    \sa setUserEffect(), standardEffect(), effect()
*/
QGLAbstractEffect *QGLPainter::userEffect() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->userEffect;
}

/*!
    Sets a user-defined \a effect to use for drawing operations
    in the current GL context.  If \a effect is null, this will
    disable user-defined effects and return to using standardEffect().

    \sa effect(), draw(), setStandardEffect()
*/
void QGLPainter::setUserEffect(QGLAbstractEffect *effect)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->userEffect == effect)
        return;
    if (d->effect)
        d->effect->setActive(this, false);
    d->userEffect = effect;
    if (effect && (!d->pick || !d->pick->isPicking)) {
        d->effect = effect;
        d->effect->setActive(this, true);
        d->updates = UpdateAll;
    } else {
        // Revert to the effect associated with standardEffect().
        d->effect = 0;
        d->ensureEffect(this);
    }
}

/*!
    Returns the standard effect to use for rendering fragments in
    the current GL context when userEffect() is null.

    \sa setStandardEffect(), userEffect()
*/
QGL::StandardEffect QGLPainter::standardEffect() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->standardEffect;
}

/*!
    Sets a standard \a effect to use for rendering fragments
    in the current GL context.  This will also set userEffect()
    to null.  If \a effect is an invalid value, then the behavior
    of QGL::FlatColor will be used instead.

    \sa standardEffect(), setUserEffect()
*/
void QGLPainter::setStandardEffect(QGL::StandardEffect effect)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->standardEffect == effect && d->effect && d->userEffect == 0)
        return;
    if (d->effect)
        d->effect->setActive(this, false);
    d->standardEffect = effect;
    d->userEffect = 0;
    d->effect = 0;
    d->ensureEffect(this);
}

/*!
    Disables the current effect and sets userEffect() to null.
    Unlike setUserEffect() this not activate the standardEffect()
    until the next time effect() is called.

    This function can be used to disable all effect-based drawing
    operations prior to performing raw GL calls.  The next time
    effect() is called on this QGLPainter, the standardEffect()
    will be reactivated.  An effect can also be reactivated by
    calling setUserEffect() or setStandardEffect().

    \sa userEffect(), standardEffect()
*/
void QGLPainter::disableEffect()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->effect)
        d->effect->setActive(this, false);
    d->userEffect = 0;
    d->effect = 0;
}

/*!
    Returns the cached shader program associated with \a name; or null
    if \a name is not currently associated with a shader program.

    \sa setCachedProgram()
*/
QOpenGLShaderProgram *QGLPainter::cachedProgram(const QString& name) const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->cachedPrograms.value(name, 0);
}

/*!
    Sets the cached shader \a program associated with \a name.

    Effect objects can use this function to store pre-compiled
    and pre-linked shader programs in the painter for future
    use by the same effect.  The \a program will be destroyed
    when context() is destroyed.

    If \a program is null, then the program associated with \a name
    will be destroyed.  If \a name is already present as a cached
    program, then it will be replaced with \a program.

    Names that start with "\c{qt.}" are reserved for use by Qt's
    internal effects.

    \sa cachedProgram()
*/
void QGLPainter::setCachedProgram
    (const QString& name, QOpenGLShaderProgram *program)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    QOpenGLShaderProgram *current = d->cachedPrograms.value(name, 0);
    if (current != program) {
        if (program)
            d->cachedPrograms[name] = program;
        else
            d->cachedPrograms.remove(name);
        delete current;
    }
}

void QGLPainterPrivate::createEffect(QGLPainter *painter)
{
    if (userEffect) {
        if (!pick || !pick->isPicking) {
            effect = userEffect;
            effect->setActive(painter, true);
            updates = QGLPainter::UpdateAll;
            return;
        }
        if (userEffect->supportsPicking()) {
            effect = userEffect;
            effect->setActive(painter, true);
            updates = QGLPainter::UpdateAll;
            return;
        }
        effect = pick->defaultPickEffect;
        effect->setActive(painter, true);
        updates = QGLPainter::UpdateAll;
        return;
    }
    if (uint(standardEffect) >= QGL_MAX_STD_EFFECTS)
        effect = stdeffects[int(QGL::FlatColor)];
    else
        effect = stdeffects[int(standardEffect)];
    if (!effect) {
        switch (standardEffect) {
        case QGL::FlatColor: default:
            effect = new QGLFlatColorEffect();
            break;
        case QGL::FlatPerVertexColor:
            effect = new QGLPerVertexColorEffect();
            break;
        case QGL::FlatReplaceTexture2D:
            effect = new QGLFlatTextureEffect();
            break;
        case QGL::FlatDecalTexture2D:
            effect = new QGLFlatDecalTextureEffect();
            break;
        case QGL::LitMaterial:
            effect = new QGLLitMaterialEffect();
            break;
        case QGL::LitDecalTexture2D:
            effect = new QGLLitDecalTextureEffect();
            break;
        case QGL::LitModulateTexture2D:
            effect = new QGLLitModulateTextureEffect();
            break;
        }
        if (uint(standardEffect) >= QGL_MAX_STD_EFFECTS)
            stdeffects[int(QGL::FlatColor)] = effect;
        else
            stdeffects[int(standardEffect)] = effect;
    }
    if (!pick || !pick->isPicking || effect->supportsPicking()) {
        effect->setActive(painter, true);
    } else {
        effect = pick->defaultPickEffect;
        effect->setActive(painter, true);
    }
    updates = QGLPainter::UpdateAll;
}

/*!
    Returns the last color that was set with setColor().  The default
    value is (1, 1, 1, 1).

    \sa setColor()
*/
QColor QGLPainter::color() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->color;
}

/*!
    Sets the default fragment \a color for effects associated
    with this painter.  This function does not apply the color
    to the effect until update() is called.

    \sa color(), update()
*/
void QGLPainter::setColor(const QColor& color)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->color = color;
    d->updates |= UpdateColor;
}

static void qt_gl_setVertexAttribute(QGL::VertexAttribute attribute, const QGLAttributeValue& value)
{
#if !defined(QT_OPENGL_ES_2)
    switch (attribute) {
    case QGL::Position:
        glVertexPointer(value.tupleSize(), value.type(),
                        value.stride(), value.data());
        break;

    case QGL::Normal:
        if (value.tupleSize() == 3)
            glNormalPointer(value.type(), value.stride(), value.data());
        break;

    case QGL::Color:
        glColorPointer(value.tupleSize(), value.type(),
                       value.stride(), value.data());
        break;

#ifdef GL_TEXTURE_COORD_ARRAY
    case QGL::TextureCoord0:
    case QGL::TextureCoord1:
    case QGL::TextureCoord2:
    {
        int unit = (int)(attribute - QGL::TextureCoord0);
        qt_gl_ClientActiveTexture(GL_TEXTURE0 + unit);
        glTexCoordPointer(value.tupleSize(), value.type(),
                          value.stride(), value.data());
        if (unit != 0)  // Stay on unit 0 between requests.
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
    }
    break;
#endif

    default: break;
    }
#else
    Q_UNUSED(attribute);
    Q_UNUSED(value);
#endif
}

/*!
    Returns the set of vertex attributes that have been set on the
    painter state by setVertexAttribute() and setVertexBundle()
    since the last call to clearAttributes().

    The most common use for this function is to determine if specific
    attributes have been supplied on the painter so as to adjust the
    current drawing effect accordingly.  The following example will
    use a lit texture effect if texture co-ordinates were provided
    in the vertex bundle, or a simple lit material effect if
    texture co-ordinates were not provided:

    \code
    painter.clearAttributes();
    painter.setVertexBundle(bundle);
    if (painter.attributes().contains(QGL::TextureCoord0))
        painter.setStandardEffect(QGL::LitModulateTexture2D);
    else
        painter.setStandardEffect(QGL::LitMaterial);
    \endcode

    It is important to clear the attributes before setting the vertex
    bundle, so that attributes from a previous bundle will not leak
    through.  Multiple vertex bundles may be supplied if they contain
    different parts of the same logical piece of geometry.

    \sa clearAttributes(), setVertexBundle()
*/
QGLAttributeSet QGLPainter::attributes() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->attributeSet;
}

/*!
    Clears the set of vertex attributes that have been set on the
    painter state by setVertexAttribute() and setVertexBundle().
    See the documentation for attributes() for more information.

    \sa attributes()
*/
void QGLPainter::clearAttributes()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->attributeSet.clear();
}

/*!
    Sets a vertex \a attribute on the current GL context to \a value.

    The vertex attribute is bound to the GL state on the index
    corresponding to \a attribute.  For example, QGL::Position
    will be bound to index 0, QGL::TextureCoord0 will be bound
    to index 3, etc.

    Vertex attributes are independent of the effect() and can be
    bound once and then used with multiple effects.

    If this is the first attribute in a new piece of geometry,
    it is recommended that clearAttributes() be called before this
    function.  This will inform QGLPainter that a new piece of geometry
    is being provided and that the previous geometry is now invalid.
    See the documentation for attributes() for more information.

    \sa setVertexBundle(), draw(), clearAttributes(), attributes()
*/
void QGLPainter::setVertexAttribute
    (QGL::VertexAttribute attribute, const QGLAttributeValue& value)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->ensureEffect(this);
    if (d->boundVertexBuffer) {
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
        d->boundVertexBuffer = 0;
    }
    if (d->isFixedFunction) {
        qt_gl_setVertexAttribute(attribute, value);
    } else {
        glVertexAttribPointer(GLuint(attribute), value.tupleSize(),
                              value.type(), GL_TRUE,
                              value.stride(), value.data());
    }
    d->attributeSet.insert(attribute);
}

/*!
    Sets the vertex attributes on the current GL context that are
    stored in \a buffer.

    The vertex attributes are bound to the GL state on the indexes
    that are specified within \a buffer; QGL::Position will be
    bound to index 0, QGL::TextureCoord0 will be bound to index 3, etc.

    Vertex attributes are independent of the effect() and can be
    bound once and then used with multiple effects.

    It is recommended that clearAttributes() be called before this
    function to inform QGLPainter that a new piece of geometry is
    being provided and that the previous geometry is now invalid.
    See the documentation for attributes() for more information.

    \sa setVertexAttribute(), draw(), clearAttributes(), attributes()
*/
void QGLPainter::setVertexBundle(const QGLVertexBundle& buffer)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->ensureEffect(this);
    QGLVertexBundlePrivate *bd = const_cast<QGLVertexBundlePrivate *>(buffer.d_func());
    if (bd->buffer.isCreated()) {
        GLuint id = bd->buffer.bufferId();
        if (id != d->boundVertexBuffer) {
            bd->buffer.bind();
            d->boundVertexBuffer = id;
        }
    } else if (d->boundVertexBuffer) {
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
        d->boundVertexBuffer = 0;
    }
    for (int index = 0; index < bd->attributes.size(); ++index) {
        QGLVertexBundleAttribute *attr = bd->attributes[index];
        if (d->isFixedFunction) {
            qt_gl_setVertexAttribute(attr->attribute, attr->value);
        } else {
            glVertexAttribPointer(GLuint(attr->attribute),
                                  attr->value.tupleSize(),
                                  attr->value.type(), GL_TRUE,
                                  attr->value.stride(), attr->value.data());
        }
    }
    d->attributeSet.unite(buffer.attributes());
}

/*!
    Updates the projection matrix, modelview matrix, and lighting
    conditions in the currently active effect() object by calling
    QGLAbstractEffect::update().  Also updates \c{glViewport()}
    to cover the currentSurface() if necessary.

    Normally this function is called automatically by draw().
    However, if the user wishes to use raw GL functions to draw fragments,
    it will be necessary to explicitly call this function to ensure that
    the matrix state and lighting conditions have been set on the
    active effect().

    Note that this function informs the effect that an update is needed.
    It does not change the GL state itself, except for \c{glViewport()}.
    In particular, the modelview and projection matrices in the
    fixed-function pipeline are not changed unless the effect or
    application calls updateFixedFunction().

    \sa setUserEffect(), projectionMatrix(), modelViewMatrix()
    \sa draw(), updateFixedFunction()
*/
void QGLPainter::update()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->ensureEffect(this);
    QGLPainter::Updates updates = d->updates;
    d->updates = 0;
    if (d->modelViewMatrix.isDirty()) {
        updates |= UpdateModelViewMatrix;
        d->modelViewMatrix.setDirty(false);
    }
    if (d->projectionMatrix.isDirty()) {
        updates |= UpdateProjectionMatrix;
        d->projectionMatrix.setDirty(false);
    }
    if ((updates & UpdateViewport) != 0) {
        QRect viewport = currentSurface()->viewportGL();
        glViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height());
    }
    if (updates != 0)
        d->effect->update(this, updates);
}

#if !defined(QT_OPENGL_ES_2)

static void setLight(int light, const QGLLightParameters *parameters,
                     const QMatrix4x4& transform)
{
    GLfloat params[4];

    QColor color = parameters->ambientColor();
    params[0] = color.redF();
    params[1] = color.greenF();
    params[2] = color.blueF();
    params[3] = color.alphaF();
    glLightfv(light, GL_AMBIENT, params);

    color = parameters->diffuseColor();
    params[0] = color.redF();
    params[1] = color.greenF();
    params[2] = color.blueF();
    params[3] = color.alphaF();
    glLightfv(light, GL_DIFFUSE, params);

    color = parameters->specularColor();
    params[0] = color.redF();
    params[1] = color.greenF();
    params[2] = color.blueF();
    params[3] = color.alphaF();
    glLightfv(light, GL_SPECULAR, params);

    QVector4D vector = parameters->eyePosition(transform);
    params[0] = vector.x();
    params[1] = vector.y();
    params[2] = vector.z();
    params[3] = vector.w();
    glLightfv(light, GL_POSITION, params);

    QVector3D spotDirection = parameters->eyeSpotDirection(transform);
    params[0] = spotDirection.x();
    params[1] = spotDirection.y();
    params[2] = spotDirection.z();
    glLightfv(light, GL_SPOT_DIRECTION, params);

    params[0] = parameters->spotExponent();
    glLightfv(light, GL_SPOT_EXPONENT, params);

    params[0] = parameters->spotAngle();
    glLightfv(light, GL_SPOT_CUTOFF, params);

    params[0] = parameters->constantAttenuation();
    glLightfv(light, GL_CONSTANT_ATTENUATION, params);

    params[0] = parameters->linearAttenuation();
    glLightfv(light, GL_LINEAR_ATTENUATION, params);

    params[0] = parameters->quadraticAttenuation();
    glLightfv(light, GL_QUADRATIC_ATTENUATION, params);
}

static void setMaterial(int face, const QGLMaterial *parameters)
{
    GLfloat params[17];

    QColor mcolor = parameters->ambientColor();
    params[0] = mcolor.redF();
    params[1] = mcolor.greenF();
    params[2] = mcolor.blueF();
    params[3] = mcolor.alphaF();

    mcolor = parameters->diffuseColor();
    params[4] = mcolor.redF();
    params[5] = mcolor.greenF();
    params[6] = mcolor.blueF();
    params[7] = mcolor.alphaF();

    mcolor = parameters->specularColor();
    params[8] = mcolor.redF();
    params[9] = mcolor.greenF();
    params[10] = mcolor.blueF();
    params[11] = mcolor.alphaF();

    mcolor = parameters->emittedLight();
    params[12] = mcolor.redF();
    params[13] = mcolor.greenF();
    params[14] = mcolor.blueF();
    params[15] = mcolor.alphaF();

    params[16] = parameters->shininess();

    glMaterialfv(face, GL_AMBIENT, params);
    glMaterialfv(face, GL_DIFFUSE, params + 4);
    glMaterialfv(face, GL_SPECULAR, params + 8);
    glMaterialfv(face, GL_EMISSION, params + 12);
    glMaterialfv(face, GL_SHININESS, params + 16);
}

#endif // !QT_OPENGL_ES_2

/*!
    Updates the fixed-function pipeline with the current painting
    state according to the flags in \a updates.

    This function is intended for use by effects in their
    QGLAbstractEffect::update() override if they are using the
    fixed-function pipeline.  It can also be used by user
    applications if they need the QGLPainter state to be
    set in the fixed-function pipeline.

    If the OpenGL implementation does not have a fixed-function
    pipeline, e.g. OpenGL/ES 2.0, this function does nothing.

    \sa update()
*/
void QGLPainter::updateFixedFunction(QGLPainter::Updates updates)
{
#if defined(QT_OPENGL_ES_2)
    Q_UNUSED(updates);
#else
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if ((updates & QGLPainter::UpdateColor) != 0) {
        QColor color;
        if (isPicking())
            color = pickColor();
        else
            color = this->color();
        glColor4f(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }
    if ((updates & QGLPainter::UpdateModelViewMatrix) != 0) {
        const QMatrix4x4 &matrix = d->modelViewMatrix.top();
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(reinterpret_cast<const GLfloat *>(matrix.constData()));
    }
    if ((updates & QGLPainter::UpdateProjectionMatrix) != 0) {
        const QMatrix4x4 &matrix = d->projectionMatrix.top();
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(reinterpret_cast<const GLfloat *>(matrix.constData()));
    }
    if ((updates & QGLPainter::UpdateLights) != 0) {
        // Save the current modelview matrix and load the identity.
        // We need to apply the light in the modelview transformation
        // that was active when the light was specified.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // Enable the main light.
        const QGLLightParameters *params = mainLight();
        setLight(GL_LIGHT0, params, mainLightTransform());

        // Restore the previous modelview transformation.
        glPopMatrix();

        // Set up the light model parameters if at least one light is enabled.
        const QGLLightModel *lightModel = this->lightModel();
        GLfloat values[4];
#ifdef GL_LIGHT_MODEL_TWO_SIDE
        if (lightModel->model() == QGLLightModel::TwoSided)
            values[0] = 1.0f;
        else
            values[0] = 0.0f;
        glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, values);
#endif
#ifdef GL_LIGHT_MODEL_COLOR_CONTROL
        if (lightModel->colorControl() == QGLLightModel::SeparateSpecularColor)
            values[0] = GL_SEPARATE_SPECULAR_COLOR;
        else
            values[0] = GL_SINGLE_COLOR;
        glLightModelfv(GL_LIGHT_MODEL_COLOR_CONTROL, values);
#endif
#ifdef GL_LIGHT_MODEL_LOCAL_VIEWER
        if (lightModel->viewerPosition() == QGLLightModel::LocalViewer)
            values[0] = 1.0f;
        else
            values[0] = 0.0f;
        glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, values);
#endif
#ifdef GL_LIGHT_MODEL_AMBIENT
        QColor color = lightModel->ambientSceneColor();
        values[0] = color.redF();
        values[1] = color.blueF();
        values[2] = color.greenF();
        values[3] = color.alphaF();
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, values);
#endif
    }
    if ((updates & QGLPainter::UpdateMaterials) != 0) {
        const QGLMaterial *frontMaterial = faceMaterial(QGL::FrontFaces);
        const QGLMaterial *backMaterial = faceMaterial(QGL::BackFaces);
        if (frontMaterial == backMaterial) {
            setMaterial(GL_FRONT_AND_BACK, frontMaterial);
        } else {
            setMaterial(GL_FRONT, frontMaterial);
            setMaterial(GL_BACK, backMaterial);
        }
    }
#endif
}

/*!
    Draws primitives using \a count vertices from the arrays specified
    by setVertexAttribute().  The type of primitive to draw is specified
    by \a mode.

    This operation will consume \a count values from the
    enabled arrays, starting at \a index.

    \sa update()
*/
void QGLPainter::draw(QGL::DrawingMode mode, int count, int index)
{
    update();
    glDrawArrays((GLenum)mode, index, count);
}

/*!
    \overload

    Draws primitives using vertices from the arrays specified by
    setVertexAttribute().  The type of primitive to draw is
    specified by \a mode.

    This operation will consume \a count elements of \a indices,
    which are used to index into the enabled arrays.

    \sa update()
*/
void QGLPainter::draw(QGL::DrawingMode mode, const ushort *indices, int count)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    update();
    if (d->boundIndexBuffer) {
        QOpenGLBuffer::release(QOpenGLBuffer::IndexBuffer);
        d->boundIndexBuffer = 0;
    }
    glDrawElements(GLenum(mode), count, GL_UNSIGNED_SHORT, indices);
}

/*!
    Pushes \a surface onto the surface stack and makes it the current
    drawing surface for context().  If \a surface is null, then the
    current drawing surface will be set to the main surface (e.g. the window).

    Note: the \a surface object must remain valid until popped from
    the stack or end() is called.  All surfaces are popped from
    the stack by end().

    The UpdateViewport flag will be set to indicate that the
    \c{glViewport()} should be adjusted to the extents of \a surface
    when update() is next called.

    \sa popSurface(), currentSurface(), setSurface()
    \sa QGLAbstractSurface::activate()
*/
void QGLPainter::pushSurface(QGLAbstractSurface *surface)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    Q_ASSERT(surface);
    if (!surface) {
        // Find the most recent main surface for this painter.
        int size = d->surfaceStack.size();
        while (size > 0 && !d->surfaceStack[size - 1].mainSurface)
            --size;
        if (!size)
            return;     // Shouldn't happen, but be safe anyway.
        surface = d->surfaceStack[size - 1].surface;
    }
    Q_ASSERT(!d->surfaceStack.isEmpty()); // Should have a main surface.
    QGLAbstractSurface *current = d->surfaceStack.top().surface;
    QGLPainterSurfaceInfo psurf;
    psurf.surface = surface;
    psurf.destroySurface = false;
    psurf.mainSurface = false;
    d->surfaceStack.append(psurf);
    current->switchTo(surface);
    d->updates |= UpdateViewport;
}

/*!
    Pops the top-most drawing surface from the surface stack
    and returns it.  The next object on the stack will be made
    the current drawing surface for context().  Returns null if the
    surface stack is already at the main surface (e.g. the window).

    The UpdateViewport flag will be set to indicate that the
    \c{glViewport()} should be adjusted to the new surface extents
    when update() is next called.

    \sa pushSurface(), currentSurface(), setSurface()
*/
QGLAbstractSurface *QGLPainter::popSurface()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    Q_ASSERT(!d->surfaceStack.isEmpty()); // Should have a main surface.
    QGLPainterSurfaceInfo &surf = d->surfaceStack.top();
    if (surf.mainSurface)
        return 0;
    QGLAbstractSurface *surface = surf.surface;
    d->surfaceStack.pop();
    Q_ASSERT(!d->surfaceStack.isEmpty()); // Should have a main surface.
    QGLAbstractSurface *nextSurface = d->surfaceStack.top().surface;
    surface->switchTo(nextSurface);
    d->updates |= UpdateViewport;
    return surface;
}

/*!
    Sets the top-most drawing surface on the surface stack to \a surface
    and activate it.

    Note: if the top-most drawing surface is the main surface specified
    during begin(), then this function will perform a pushSurface()
    instead.  Typically this function is used to replace the last
    surface that was pushed onto the stack and avoid doing popSurface()
    followed by pushSurface().  The main surface cannot be replaced
    in this manner.

    The UpdateViewport flag will be set to indicate that the
    \c{glViewport()} should be adjusted to the extents of \a surface
    when update() is next called.

    \sa pushSurface(), popSurface(), currentSurface()
*/
void QGLPainter::setSurface(QGLAbstractSurface *surface)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    Q_ASSERT(surface);
    Q_ASSERT(!d->surfaceStack.isEmpty()); // Should have a main surface.
    QGLPainterSurfaceInfo &surf = d->surfaceStack.top();
    if (surf.mainSurface) {
        pushSurface(surface);
        return;
    }
    QGLAbstractSurface *oldSurface = surf.surface;
    surf.surface = surface;
    oldSurface->switchTo(surface);
    d->updates |= UpdateViewport;
}

/*!
    Returns the current drawing surface.

    \sa pushSurface(), popSurface(), setSurface()
*/
QGLAbstractSurface *QGLPainter::currentSurface() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    Q_ASSERT(!d->surfaceStack.isEmpty()); // Should have a main surface.
    return d->surfaceStack.top().surface;
}

/*!
    Returns the current lighting model.

    \sa setLightModel()
*/
const QGLLightModel *QGLPainter::lightModel() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (!d->lightModel) {
        if (!d->defaultLightModel)
            d->defaultLightModel = new QGLLightModel();
        d->lightModel = d->defaultLightModel;
    }
    return d->lightModel;
}

/*!
    Sets the current lighting model to \a value.  If \a value is
    null, then the default lighting model parameters will be used.

    The light settings in the GL server will not be changed until
    update() is called.

    \sa lightModel()
*/
void QGLPainter::setLightModel(const QGLLightModel *value)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    d->lightModel = value;
    d->updates |= QGLPainter::UpdateLights;
}

/*!
    Returns the parameters for the main light in the scene.

    The light parameters are specified in world co-ordinates at
    the point when setMainLight() was called.  The mainLightTransform()
    must be applied to obtain eye co-ordinates.

    This function is a convenience that returns the light with
    identifier 0.  If light 0 is not currently enabled, then a
    default light is added to the painter with an identity
    transform and then returned as the main light.

    \sa setMainLight(), mainLightTransform(), addLight()
*/
const QGLLightParameters *QGLPainter::mainLight() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->lights.isEmpty()) {
        if (!d->defaultLight)
            d->defaultLight = new QGLLightParameters();
        d->lights.append(d->defaultLight);
        d->lightTransforms.append(QMatrix4x4());
    } else if (!d->lights[0]) {
        if (!d->defaultLight)
            d->defaultLight = new QGLLightParameters();
        d->lights[0] = d->defaultLight;
        d->lightTransforms[0] = QMatrix4x4();
    }
    return d->lights[0];
}

/*!
    Sets the \a parameters for the main light in the scene.
    The mainLightTransform() is set to the current modelViewMatrix().

    Light parameters are stored in world co-ordinates, not eye co-ordinates.
    The mainLightTransform() specifies the transformation to apply to
    convert the world co-ordinates into eye co-ordinates when the light
    is used.

    Note: the \a parameters may be ignored by effect() if it
    has some other way to determine the lighting conditions.

    The light settings in the GL server will not be changed until
    update() is called.

    This function is a convenience that sets the light with
    identifier 0.  If \a parameters is null, then light 0
    will be removed.

    \sa mainLight(), mainLightTransform(), addLight()
*/
void QGLPainter::setMainLight(const QGLLightParameters *parameters)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->lights.isEmpty()) {
        if (parameters) {
            d->lights.append(parameters);
            d->lightTransforms.append(modelViewMatrix());
            d->updates |= QGLPainter::UpdateLights;
        }
    } else if (parameters) {
        d->lights[0] = parameters;
        d->lightTransforms[0] = modelViewMatrix();
        d->updates |= QGLPainter::UpdateLights;
    } else {
        removeLight(0);
    }
}

/*!
    Sets the \a parameters for the main light in the scene, and set
    mainLightTransform() to \a transform.

    Light parameters are stored in world co-ordinates, not eye co-ordinates.
    The \a transform specifies the transformation to apply to convert the
    world co-ordinates into eye co-ordinates when the light is used.

    Note: the \a parameters may be ignored by effect() if it
    has some other way to determine the lighting conditions.

    The light settings in the GL server will not be changed until
    update() is called.

    This function is a convenience that sets the light with
    identifier 0.  If \a parameters is null, then light 0
    will be removed.

    \sa mainLight(), mainLightTransform()
*/
void QGLPainter::setMainLight
        (const QGLLightParameters *parameters, const QMatrix4x4& transform)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->lights.isEmpty()) {
        if (parameters) {
            d->lights.append(parameters);
            d->lightTransforms.append(transform);
            d->updates |= QGLPainter::UpdateLights;
        }
    } else if (parameters) {
        d->lights[0] = parameters;
        d->lightTransforms[0] = transform;
        d->updates |= QGLPainter::UpdateLights;
    } else {
        removeLight(0);
    }
}

/*!
    Returns the modelview transformation matrix for the main light that
    was set at the time setMainLight() was called.

    The light transform may be used by later painting operations to
    convert the light from world co-ordinates into eye co-ordinates.
    The eye transformation is set when the light is specified.

    This function is a convenience that returns the tranform for the
    light with identifier 0.  If light 0 is not enabled, then the
    function returns the identity matrix.

    \sa mainLight(), setMainLight(), addLight()
*/
QMatrix4x4 QGLPainter::mainLightTransform() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (!d->lights.isEmpty() && d->lights[0])
        return d->lightTransforms[0];
    else
        return QMatrix4x4();
}

/*!
    Adds a light to this painter, with the specified \a parameters.
    The lightTransform() for the light is set to the current
    modelViewMatrix().  Returns an identifier for the light.

    Light parameters are stored in world co-ordinates, not eye co-ordinates.
    The lightTransform() specifies the transformation to apply to
    convert the world co-ordinates into eye co-ordinates when the light
    is used.

    Note: the \a parameters may be ignored by effect() if it
    has some other way to determine the lighting conditions.

    The light settings in the GL server will not be changed until
    update() is called.

    \sa removeLight(), light(), mainLight()
*/
int QGLPainter::addLight(const QGLLightParameters *parameters)
{
    return addLight(parameters, modelViewMatrix());
}

/*!
    Adds a light to this painter, with the specified \a parameters.
    The lightTransform() for the light is set to \a transform.
    Returns an identifier for the light.

    Light parameters are stored in world co-ordinates, not eye co-ordinates.
    The \a transform specifies the transformation to apply to
    convert the world co-ordinates into eye co-ordinates when the light
    is used.

    Note: the \a parameters may be ignored by effect() if it
    has some other way to determine the lighting conditions.

    The light settings in the GL server will not be changed until
    update() is called.

    \sa removeLight(), light(), mainLight()
*/
int QGLPainter::addLight(const QGLLightParameters *parameters, const QMatrix4x4 &transform)
{
    Q_ASSERT(parameters);
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    int lightId = 0;
    while (lightId < d->lights.size() && d->lights[lightId] != 0)
        ++lightId;
    if (lightId < d->lights.size()) {
        d->lights[lightId] = parameters;
        d->lightTransforms[lightId] = transform;
    } else {
        d->lights.append(parameters);
        d->lightTransforms.append(transform);
    }
    d->updates |= QGLPainter::UpdateLights;
    return lightId;
}

/*!
    Removes the light with the specified \a lightId.

    \sa addLight(), light()
*/
void QGLPainter::removeLight(int lightId)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (lightId >= 0 && lightId < d->lights.size()) {
        d->lights[lightId] = 0;
        if (lightId >= (d->lights.size() - 1)) {
            do {
                d->lights.resize(lightId);
                d->lightTransforms.resize(lightId);
                --lightId;
            } while (lightId >= 0 && d->lights[lightId] == 0);
        }
        d->updates |= QGLPainter::UpdateLights;
    }
}

/*!
    Returns the maximum light identifier currently in use on this painter;
    or -1 if there are no lights.

    It is possible that some light identifiers less than maximumLightId()
    may be invalid because the lights have been removed.  Use the following
    code to locate all enabled lights:

    \code
    int maxLightId = painter.maximumLightId();
    for (int lightId = 0; index <= maxLightId; ++index) {
        const QGLLightParameters *params = painter.light(lightId);
        if (params) {
            ...
        }
    }
    \endcode

    \sa addLight(), light()
*/
int QGLPainter::maximumLightId() const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return d->lights.size() - 1;
}

/*!
    Returns the parameters for the light with the identifier \a lightId;
    or null if \a lightId is not valid or has been removed.

    \sa addLight(), removeLight(), lightTransform()
*/
const QGLLightParameters *QGLPainter::light(int lightId) const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (lightId >= 0 && lightId < d->lights.size())
        return d->lights[lightId];
    else
        return 0;
}

/*!
    Returns the modelview transformation for the light with the identifier
    \a lightId; or the identity matrix if \a lightId is not valid or has
    been removed.

    \sa addLight(), removeLight(), light()
*/
QMatrix4x4 QGLPainter::lightTransform(int lightId) const
{
    Q_D(const QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (lightId >= 0 && lightId < d->lights.size() && d->lights[lightId])
        return d->lightTransforms[lightId];
    else
        return QMatrix4x4();
}

/*!
    Returns the material that is used for drawing \a face on polygons.
    If \a face is QGL::FrontFaces or QGL::AllFaces, then the front
    material is returned.  If \a face is QGL::BackFaces, then the
    back material is returned.

    \sa setFaceMaterial(), setFaceColor()
*/
const QGLMaterial *QGLPainter::faceMaterial(QGL::Face face) const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (face == QGL::BackFaces) {
        if (!d->backMaterial) {
            if (!d->defaultMaterial)
                d->defaultMaterial = new QGLMaterial();
            d->backMaterial = d->defaultMaterial;
        }
        return d->backMaterial;
    } else {
        if (!d->frontMaterial) {
            if (!d->defaultMaterial)
                d->defaultMaterial = new QGLMaterial();
            d->frontMaterial = d->defaultMaterial;
        }
        return d->frontMaterial;
    }
}

/*!
    Sets the material that is used for drawing \a face on polygons
    to \a value.  If \a face is QGL::FrontFaces, then the front
    material is set.  If \a face is QGL::BackFaces, then the
    back material is set.  If \a face is QGL::AllFaces, then both
    the front and back materials are set.

    If \a value is null, then the \a face material will be set to
    the default material properties.

    The material settings in the GL server will not be changed until
    update() is called.

    \sa faceMaterial(), setFaceColor()
*/
void QGLPainter::setFaceMaterial
        (QGL::Face face, const QGLMaterial *value)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (face == QGL::FrontFaces) {
        if (d->frontMaterial == value)
            return;
        d->frontMaterial = value;
    } else if (face == QGL::BackFaces) {
        if (d->backMaterial == value)
            return;
        d->backMaterial = value;
    } else {
        if (d->frontMaterial == value && d->backMaterial == value)
            return;
        d->frontMaterial = value;
        d->backMaterial = value;
    }
    d->updates |= QGLPainter::UpdateMaterials;
}

static QGLMaterial *createColorMaterial
    (QGLMaterial *prev, const QColor& color)
{
    QGLMaterial *material;
    if (prev)
        material = prev;
    else
        material = new QGLMaterial();
    material->setColor(color);
    return material;
}

/*!
    Sets the material that is used for drawing \a face on polygons
    to \a color.  This is a convenience function for setting materials
    to simple colors.

    The RGB components of the ambient color of the material will be set
    to 20% of \a color, and the RGB components of the diffuse color of the
    material will be set to 80% of \a color.  The alpha components of
    the ambient and diffuse material colors will both be set to the
    alpha component of \a color.

    If \a face is QGL::FrontFaces, then the front material is set.
    If \a face is QGL::BackFaces, then the back material is set.
    If \a face is QGL::AllFaces, then both the front and back
    materials are set.

    The material settings in the GL server will not be changed until
    update() is called.

    \sa faceMaterial(), setFaceMaterial()
*/
void QGLPainter::setFaceColor(QGL::Face face, const QColor& color)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (face == QGL::FrontFaces) {
        d->frontColorMaterial =
            createColorMaterial(d->frontColorMaterial, color);
        d->frontMaterial = d->frontColorMaterial;
    } else if (face == QGL::BackFaces) {
        d->backColorMaterial =
            createColorMaterial(d->backColorMaterial, color);
        d->backMaterial = d->backColorMaterial;
    } else {
        d->frontColorMaterial =
            createColorMaterial(d->frontColorMaterial, color);
        d->backColorMaterial =
            createColorMaterial(d->backColorMaterial, color);
        d->frontMaterial = d->frontColorMaterial;
        d->backMaterial = d->backColorMaterial;
    }
    d->updates |= QGLPainter::UpdateMaterials;
}

/*!
    Returns true if this painter is in object picking mode;
    false if this painter is in normal rendering mode.

    \sa setPicking(), objectPickId()
*/
bool QGLPainter::isPicking() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return (d->pick ? d->pick->isPicking : false);
}

/*!
    Enables or disables object picking mode according to \a value.

    If \a value is true, then the effect() will be overridden with a
    simple flat color effect that renders objects with pickColor().
    These colors can be read back later with pickObject().

    \sa isPicking(), objectPickId(), pickObject()
*/
void QGLPainter::setPicking(bool value)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (!d->pick)
        d->pick = new QGLPainterPickPrivate();
    if (d->pick->isPicking != value) {
        // Switch to/from the pick effect.
        d->pick->isPicking = value;
        if (d->effect)
            d->effect->setActive(this, false);
        d->effect = 0;
        d->ensureEffect(this);
    }
}

/*!
    Returns the current object pick identifier.  The default value
    is -1 which indicates that rendered objects should not have a
    pickColor() associated with them.

    \sa setObjectPickId(), clearPickObjects(), pickObject()
*/
int QGLPainter::objectPickId() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    return (d->pick ? d->pick->objectPickId : -1);
}

/*!
    Sets the current object pick identifier to \a value.  If \a value
    is -1, then subsequent objects will be rendered without a pickColor().

    If value is not -1, then the pickColor() is changed to a color
    that represents that object pick identifier.  If \a value has been
    seen previously, then the same pickColor() as last time will
    be returned.

    The function call will be ignored if isPicking() is false.

    \sa objectPickId(), clearPickObjects(), pickObject()
*/
void QGLPainter::setObjectPickId(int value)
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (!d->pick || !d->pick->isPicking)
        return;
    d->pick->objectPickId = value;
    if (value != -1) {
        QRgb color = d->pick->pickObjectToColor.value(value, 0);
        if (!color) {
            color = qt_qgl_pick_color(d->pick->pickColorIndex++);
            d->pick->pickObjectToColor[value] = color;
            d->pick->pickColorToObject[color] = value;
        }
        d->pick->pickColor = color;
        d->updates |= UpdateColor;
    } else {
        d->pick->pickColor = 0;
        d->updates |= UpdateColor;
    }
}

/*!
    Clears the objectPickId() to pickColor() mappings that
    were used previously.  This will also set objectPickId()
    to -1 and pickColor() to (0, 0, 0, 1).

    The function call will be ignored if isPicking() is false.

    \sa objectPickId(), pickColor()
*/
void QGLPainter::clearPickObjects()
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->pick && d->pick->isPicking) {
        d->pick->pickObjectToColor.clear();
        d->pick->pickColorToObject.clear();
        d->pick->pickColorIndex = 0;
        d->pick->objectPickId = -1;
        d->pick->pickColor = 0;
        d->updates |= UpdateColor;
    }
}

/*!
    Returns the current pick color to use to render the object
    associated with objectPickId().  The returned color will
    be (0, 0, 0, 1) if objectPickId() is -1.

    \sa objectPickId(), clearPickObjects()
*/
QColor QGLPainter::pickColor() const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();
    if (d->pick) {
        QColor color;
        color.setRgb(d->pick->pickColor);
        return color;
    } else {
        return Qt::black;
    }
}

/*!
    Picks the color at (\a x, \a y) in the color buffer and
    returns the objectPickId() that corresponds to that color.
    Returns -1 if (\a x, \a y) is not positioned over a
    recognized object.  The origin (0, 0) is assumed to be
    the bottom-left corner of the drawing surface.

    \sa objectPickId()
*/
int QGLPainter::pickObject(int x, int y) const
{
    Q_D(QGLPainter);
    QGLPAINTER_CHECK_PRIVATE();

    if (!d->pick)
    {
        return -1;
    }

    // Fetch the color at the specified pixel.
    unsigned char data[4] = {0, 0, 0, 0};
    context()->functions()->glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
    QRgb color = qRgb(data[0], data[1], data[2]);

    // Normalize the color to account for floating-point rounding.
    color = qt_qgl_normalize_pick_color(color); // XXX: detect RGB444 screens.

    // Map the color back to an object identifier.
    return d->pick->pickColorToObject.value(color, -1);
}



/*!
    \class QGLLightParameters
    \brief The QGLLightParameters class represents the parameters of a light in a 3D scene.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting

    The functions in this class are a convenience wrapper for the OpenGL enumerations
    which control each light source in a 3D scene.  For the general ambient light in a
    scene, not from a source refer to QGLLightModel.

    The ambient, diffuse and specular components of the light can be controlled for by
    colour, and are set by the following functions:
    \list
    \li setAmbientColor()
    \li setDiffuseColor()
    \li setSpecularColor()
    \endlist
    Other than changing intensity by using darker color values, see below for how to
    change the intensity of the light with distance from the lit object.

    Light sources are of two types, directional and positional, described by the
    enumeration QGLLightParameters::LightType.  By default a light source is directional.

    A directional light source is infinitely distant, such that its rays are all
    parallel, to a direction \b vector.  This vector is set by the setDirection()
    function.  If the light source is not directional when setDirection() is called, its
    type is changed to Directional.  Directional light sources model real world light
    sources like the sun.  Such light sources are not located at any particular point
    and affect the scene evenly.

    In the OpenGL model-view, a directional light is represented by a 4d vector, with
    the 4th value, \c w, set to 0.0f.  This means that the spatial data of the light
    is not changed during certain transformations, for example translation.
    See \l {3D Math Basis} for a fuller explanation of this.

    Calling the setPosition() function defines the light source to be located at a given
    \b point, a finite distance from the lit object.  If the light source is not
    positional when this function is called its type is changed to Positional.

    A positional light source models a real world lamp, such as a light-bulb or
    car headlight.  Since it is a finite distance from the lit object the rays diverge
    and lighting calculations are thus more complex.

    In OpenGL the light has its spatial data \c w value set to 1.0f, resulting in the
    lights position being changed along with other objects in the scene by transformations
    such as translation.  See \l {3D Math Basis} for more.

    Positional lights are by default point sources, like the light-bulb where light
    radiates in all directions.  Calling the spotAngle() function sets an angle to
    restrict the light from a positional source to a cone like the car headlight.  This
    makes the light behave like a spotlight, where objects outside of the cone of the
    light do not receive any light from that source.

    The spotlight may be further specified by its spotExponent() which dictates the
    behaviour of the cutoff at the lighting cone; and by the attenuation functions
    which are the terms in the following equation:
    \image attenuation.png
    here \c d is the distance of the light from the lit object, and A is the attenuation
    factor which is a multiplier of the light source, determining its intensity at the
    lit object.  The terms in the denominator are:
    \list
    \li constantAttenuation() - default 1.0
    \raw HTML
    k<sub>c</sub>
    \endraw
    \li linearAttenuation() - default 0.0
    \raw HTML
    k<sub>l</sub>
    \endraw
    \li quadraticAttenuation() - default 0.0
    \raw HTML
    k<sub>q</sub>
    \endraw
    \endlist
    When these terms are large the light contributed by the source becomes much weaker
    with distance from the object.
*/

/*!
    \qmltype Light
    \instantiates QGLLightParameters
    \brief The Light item represents the parameters of a light in a 3D scene.
    \since 4.8
    \ingroup qt3d::qml3d

    \sa LightModel
*/

class QGLLightParametersPrivate
{
public:
    QGLLightParametersPrivate() :
        type(QGLLightParameters::Directional),
        position(0.0f, 0.0f, 1.0f),
        ambientColor(0, 0, 0, 255),
        diffuseColor(255, 255, 255, 255),
        specularColor(255, 255, 255, 255),
        spotDirection(0.0f, 0.0f, -1.0f),
        spotExponent(0.0f),
        spotAngle(180.0f),
        spotCosAngle(-1.0f),
        constantAttenuation(1.0f),
        linearAttenuation(0.0f),
        quadraticAttenuation(0.0f)
    {
    }

    QGLLightParameters::LightType type;
    QVector3D position;
    QColor ambientColor;
    QColor diffuseColor;
    QColor specularColor;
    QVector3D spotDirection;
    float spotExponent;
    float spotAngle;
    float spotCosAngle;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
};


/*!
    \enum QGLLightParameters::LightType
    This enum defines the possible types of light.

    \value Directional The default.  Directional lights are infinitely
           distant from the lit object, and its rays are parallel to a
           direction vector.  Use setDirection() to make a light Directional.
    \value Positional  Positional lights are a finite distance from the
           lit object.  Complex lighting with spot-lights, and attenuation
           are enabled with Positional lighting.  Use setPosition() to
           make a light Positional.
*/

/*!
    Constructs a new light parameters object with default values
    and attaches it to \a parent.
*/
QGLLightParameters::QGLLightParameters(QObject *parent)
    : QObject(parent)
    , d_ptr(new QGLLightParametersPrivate)
{
}

/*!
    Destroys this light parameters object.
*/
QGLLightParameters::~QGLLightParameters()
{
}

/*!
    \property QGLLightParameters::type
    \brief the type of this light; Positional or Directional.

    \sa position(), direction()
*/

/*!
    \qmlproperty enumeration Light::type
    The type of this light; Positional or Directional.  The default
    is Directional.

    \sa position, direction
*/

QGLLightParameters::LightType QGLLightParameters::type() const
{
    Q_D(const QGLLightParameters);
    return d->type;
}

/*!
    \property QGLLightParameters::position
    \brief the position of this light if it is Positional; or a null
    QVector3D if it is Directional.  By default, the light is Directional.

    For a Positional light, the return value is the location in model space
    of the light, at some finite distance from the origin.  The value is
    transformed by the model-view transformation when the light
    parameters are applied.

    When the light is Positional OpenGL will obey other parameters relating
    to the light's position, such as attenuation and spot angle.

    Setting the position converts the light from Directional to Positional.

    \sa direction(), type(), positionChanged()
*/

/*!
    \qmlproperty vector3D Light::position
    The position of this light if it is Positional; or a zero vector
    if it is Directional.  By default, the light is Directional.

    For a Positional light, the return value is the location in model space
    of the light, at some finite distance from the origin.  The value is
    transformed by the model-view transformation when the light
    parameters are applied.

    When the light is Positional OpenGL will obey other parameters relating
    to the light's position, such as attenuation and spot angle.

    Setting the position converts the light from Directional to Positional.

    \sa direction, type
*/

QVector3D QGLLightParameters::position() const
{
    Q_D(const QGLLightParameters);
    if (d->type == Positional)
        return d->position;
    else
        return QVector3D();
}

void QGLLightParameters::setPosition(const QVector3D& point)
{
    Q_D(QGLLightParameters);
    if (d->type == Positional) {
        if (d->position != point) {
            // Only the position() has changed.
            d->position = point;
            emit positionChanged();
            emit lightChanged();
        }
    } else {
        // Both the position() and direction() are changed.
        d->type = Positional;
        d->position = point;
        emit positionChanged();
        emit directionChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::direction
    \brief the direction of this light if it is Directional; or a null
    QVector3D if it is Positional.  By default, the light is Directional.

    For a Directional light, the return value is the direction vector of
    an infinitely distant directional light, like the sun.

    The default value is (0, 0, 1), for a directional light pointing along
    the positive z-axis.

    OpenGL will ignore other parameters such as attenuation and spot angle
    when this value is set, since a directional light has no location.
    In order to set the direction of a spot light, use the setSpotDirection()
    function.

    Setting the direction converts the light from Positional to Directional.

    \sa position(), type(), directionChanged()
*/

/*!
    \qmlproperty vector3D Light::direction
    The direction of this light if it is Directional; or a zero vector
    if it is Positional.  By default, the light is Directional.

    For a Directional light, the return value is the direction vector of
    an infinitely distant directional light, like the sun.

    The default value is (0, 0, 1), for a directional light pointing along
    the positive z-axis.

    OpenGL will ignore other parameters such as attenuation and spot angle
    when this value is set, since a directional light has no location.
    In order to set the direction of a spot light, use the setSpotDirection()
    function.

    Setting the direction converts the light from Positional to Directional.

    \sa position, type
*/

QVector3D QGLLightParameters::direction() const
{
    Q_D(const QGLLightParameters);
    if (d->type == Directional)
        return d->position;
    else
        return QVector3D();
}

void QGLLightParameters::setDirection(const QVector3D& value)
{
    Q_D(QGLLightParameters);
    if (d->type == Directional) {
        if (d->position != value) {
            // Only the direction() has changed.
            d->position = value;
            emit directionChanged();
            emit lightChanged();
        }
    } else {
        // Both the position() and direction() are changed.
        d->type = Directional;
        d->position = value;
        emit positionChanged();
        emit directionChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::ambientColor
    \brief the ambient color of this light.  The default value is black.

    \sa diffuseColor(), specularColor(), ambientColorChanged()
*/

/*!
    \qmlproperty color Light::ambientColor
    The ambient color of this light.  The default value is black.

    \sa diffuseColor, specularColor
*/

QColor QGLLightParameters::ambientColor() const
{
    Q_D(const QGLLightParameters);
    return d->ambientColor;
}

void QGLLightParameters::setAmbientColor(const QColor& value)
{
    Q_D(QGLLightParameters);
    if (d->ambientColor != value) {
        d->ambientColor = value;
        emit ambientColorChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::diffuseColor
    \brief the diffuse color of this light.  The default value is white.

    \sa ambientColor(), specularColor(), diffuseColorChanged()
*/

/*!
    \qmlproperty color Light::diffuseColor
    The diffuse color of this light.  The default value is white.

    \sa ambientColor, specularColor
*/
QColor QGLLightParameters::diffuseColor() const
{
    Q_D(const QGLLightParameters);
    return d->diffuseColor;
}

void QGLLightParameters::setDiffuseColor(const QColor& value)
{
    Q_D(QGLLightParameters);
    if (d->diffuseColor != value) {
        d->diffuseColor = value;
        emit diffuseColorChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::specularColor
    \brief the specular color of this light.  The default value is white.

    \sa ambientColor(), diffuseColor(), specularColorChanged()
*/

/*!
    \qmlproperty color Light::specularColor
    The specular color of this light.  The default value is white.

    \sa ambientColor, diffuseColor
*/

QColor QGLLightParameters::specularColor() const
{
    Q_D(const QGLLightParameters);
    return d->specularColor;
}

void QGLLightParameters::setSpecularColor(const QColor& value)
{
    Q_D(QGLLightParameters);
    if (d->specularColor != value) {
        d->specularColor = value;
        emit specularColorChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::spotDirection
    \brief the direction that a spot-light is shining in.

    A spotlight is a positional light that has a value other than the default
    for spot angle.  The default value is (0, 0, -1).

    This parameter has no effect on a Directional light.

    \sa spotExponent(), spotDirectionChanged()
*/

/*!
    \qmlproperty vector3D Light::spotDirection
    The direction that a spot-light is shining in.

    A spotlight is a positional light that has a value other than the default
    for spot angle.  The default value is (0, 0, -1).

    This parameter has no effect on a Directional light.

    \sa spotExponent
*/
QVector3D QGLLightParameters::spotDirection() const
{
    Q_D(const QGLLightParameters);
    return d->spotDirection;
}

void QGLLightParameters::setSpotDirection(const QVector3D& vector)
{
    Q_D(QGLLightParameters);
    if (d->spotDirection != vector) {
        d->spotDirection = vector;
        emit spotDirectionChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::spotExponent
    \brief the exponent value between 0 and 128 that indicates the
    intensity distribution of a spot-light.  The default value is 0,
    indicating uniform light distribution.

    This parameter has no effect on a Directional light.

    \sa spotAngle(), setPosition(), spotExponentChanged()
*/

/*!
    \qmlproperty real Light::spotExponent
    The exponent value between 0 and 128 that indicates the
    intensity distribution of a spot-light.  The default value is 0,
    indicating uniform light distribution.

    This parameter has no effect on a Directional light.

    \sa spotAngle, position
*/

float QGLLightParameters::spotExponent() const
{
    Q_D(const QGLLightParameters);
    return d->spotExponent;
}

void QGLLightParameters::setSpotExponent(float value)
{
    Q_D(QGLLightParameters);
    if (d->spotExponent != value) {
        d->spotExponent = value;
        emit spotExponentChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::spotAngle
    \brief the angle over which light is spread from this light.
    It should be between 0 and 90 for spot lights, and 180 for
    uniform light distribution from a remote light source.
    The default value is 180.

    This parameter has no effect on a Directional light.

    \sa spotCosAngle(), spotAngleChanged()
*/

/*!
    \qmlproperty real Light::spotAngle
    The angle over which light is spread from this light.
    It should be between 0 and 90 for spot lights, and 180 for
    uniform light distribution from a remote light source.
    The default value is 180.

    This parameter has no effect on a Directional light.
*/

float QGLLightParameters::spotAngle() const
{
    Q_D(const QGLLightParameters);
    return d->spotAngle;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void QGLLightParameters::setSpotAngle(float value)
{
    Q_D(QGLLightParameters);
    if (d->spotAngle != value) {
        d->spotAngle = value;
        if (value != 180.0f)
            d->spotCosAngle = cosf(value * M_PI / 180.0f);
        else
            d->spotCosAngle = -1.0f;
        emit spotAngleChanged();
        emit lightChanged();
    }
}

/*!
    Returns the cosine of spotAngle(), or -1 if spotAngle() is 180.

    The cosine of spotAngle() is useful in lighting algorithms.
    This function returns a cached copy of the cosine so that it
    does not need to be computed every time the lighting parameters
    are read.

    \sa spotAngle()
*/
float QGLLightParameters::spotCosAngle() const
{
    Q_D(const QGLLightParameters);
    return d->spotCosAngle;
}

/*!
    \property QGLLightParameters::constantAttenuation
    \brief the constant light attenuation factor.  The default value is 1.

    This parameter has no effect on a Directional light.

    \sa linearAttenuation(), quadraticAttenuation()
    \sa constantAttenuationChanged()
*/

/*!
    \qmlproperty real Light::constantAttenuation
    The constant light attenuation factor.  The default value is 1.

    This parameter has no effect on a Directional light.

    \sa linearAttenuation, quadraticAttenuation
*/

float QGLLightParameters::constantAttenuation() const
{
    Q_D(const QGLLightParameters);
    return d->constantAttenuation;
}

void QGLLightParameters::setConstantAttenuation(float value)
{
    Q_D(QGLLightParameters);
    if (d->constantAttenuation != value) {
        d->constantAttenuation = value;
        emit constantAttenuationChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::linearAttenuation
    \brief the linear light attenuation factor.  The default value is 0.

    This parameter has no effect on a Directional light.

    \sa constantAttenuation(), quadraticAttenuation()
    \sa linearAttenuationChanged()
*/

/*!
    \qmlproperty real Light::linearAttenuation
    The linear light attenuation factor.  The default value is 0.

    This parameter has no effect on a Directional light.

    \sa constantAttenuation, quadraticAttenuation
*/

float QGLLightParameters::linearAttenuation() const
{
    Q_D(const QGLLightParameters);
    return d->linearAttenuation;
}

void QGLLightParameters::setLinearAttenuation(float value)
{
    Q_D(QGLLightParameters);
    if (d->linearAttenuation != value) {
        d->linearAttenuation = value;
        emit linearAttenuationChanged();
        emit lightChanged();
    }
}

/*!
    \property QGLLightParameters::quadraticAttenuation
    \brief the quadratic light attenuation factor.  The default value is 0.

    This parameter has no effect on a Directional light.

    \sa constantAttenuation(), linearAttenuation()
    \sa quadraticAttenuationChanged()
*/

/*!
    \qmlproperty real Light::quadraticAttenuation
    The quadratic light attenuation factor.  The default value is 0.

    This parameter has no effect on a Directional light.

    \sa constantAttenuation, linearAttenuation
*/

float QGLLightParameters::quadraticAttenuation() const
{
    Q_D(const QGLLightParameters);
    return d->quadraticAttenuation;
}

void QGLLightParameters::setQuadraticAttenuation(float value)
{
    Q_D(QGLLightParameters);
    if (d->quadraticAttenuation != value) {
        d->quadraticAttenuation = value;
        emit quadraticAttenuationChanged();
        emit lightChanged();
    }
}

/*!
    Returns the position of this light after transforming it from
    world co-ordinates to eye co-ordinates using \a transform.

    If the light is Directional, then direction() will be transformed,
    after setting the w co-ordinate to 0.  If the light is Positional,
    then position() will be transformed after setting the w co-ordinate
    to 1.

    The returned result is suitable to be applied to the GL_POSITION
    property of \c{glLight()}, assuming that the modelview transformation
    in the GL context is set to the identity.

    \sa eyeSpotDirection()
*/
QVector4D QGLLightParameters::eyePosition(const QMatrix4x4& transform) const
{
    Q_D(const QGLLightParameters);
    if (d->type == Directional)
        return transform * QVector4D(d->position, 0.0f);
    else
        return transform * QVector4D(d->position, 1.0f);
}

/*!
    Returns the spotDirection() for this light after transforming it
    from world co-ordinates to eye co-ordinates using the top-left
    3x3 submatrix within \a transform.

    The returned result is suitable to be applied to the GL_SPOT_DIRECTION
    property of \c{glLight()}, assuming that the modelview transformation
    in the GL context is set to the identity.

    \sa eyePosition()
*/
QVector3D QGLLightParameters::eyeSpotDirection
    (const QMatrix4x4& transform) const
{
    Q_D(const QGLLightParameters);
    return transform.mapVector(d->spotDirection);
}

/*!
    \fn void QGLLightParameters::positionChanged()

    This signal is emitted when position() changes, or when the type()
    changes between Directional and Positional.

    \sa position(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::directionChanged()

    This signal is emitted when direction() changes, or when the type()
    changes between Directional and Positional.

    \sa direction(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::ambientColorChanged()

    This signal is emitted when ambientColor() changes.

    \sa ambientColor(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::diffuseColorChanged()

    This signal is emitted when diffuseColor() changes.

    \sa diffuseColor(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::specularColorChanged()

    This signal is emitted when specularColor() changes.

    \sa specularColor(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::spotDirectionChanged()

    This signal is emitted when spotDirection() changes.

    \sa spotDirection(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::spotExponentChanged()

    This signal is emitted when spotExponent() changes.

    \sa spotExponent(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::spotAngleChanged()

    This signal is emitted when spotAngle() changes.

    \sa spotAngle(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::constantAttenuationChanged()

    This signal is emitted when constantAttenuation() changes.

    \sa constantAttenuation(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::linearAttenuationChanged()

    This signal is emitted when linearAttenuation() changes.

    \sa linearAttenuation(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::quadraticAttenuationChanged()

    This signal is emitted when quadraticAttenuation() changes.

    \sa quadraticAttenuation(), lightChanged()
*/

/*!
    \fn void QGLLightParameters::lightChanged()

    This signal is emitted when any of the properties on the light changes.
*/



/*!
    \class QGLLightModel
    \brief The QGLLightModel class defines the lighting model to use for the scene.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::painting
*/

/*!
    \qmltype LightModel
    \instantiates QGLLightModel
    \brief The LightModel item defines the lighting model to use for the scene.
    \since 4.8
    \ingroup qt3d::qml3d

    \sa Light
*/

/*!
    \enum QGLLightModel::Model
    This enum defines the type of lighting model to use: one-sided or two-sided.

    \value OneSided One-sided lighting, with the front face material used for both front and back faces.
    \value TwoSided Two-sided lighting, with separate front and back face materials.
*/

/*!
    \enum QGLLightModel::ColorControl
    This enum controls the number of colors to be generated by the lighting computation.

    \value SingleColor A single color is generated by the lighting computation.
    \value SeparateSpecularColor A separate specular color computation is
           performed and then summed into the pixel color after texture mapping.
*/

/*!
    \enum QGLLightModel::ViewerPosition
    This enum defines the position of the viewer for the purposes of lighting calculations.

    \value ViewerAtInfinity The viewer is at infinity along the -z axis.
    \value LocalViewer The viewer is at the local origin in eye coordinates.
*/

class QGLLightModelPrivate
{
public:
    QGLLightModelPrivate()
        : model(QGLLightModel::OneSided),
          colorControl(QGLLightModel::SingleColor),
          viewerPosition(QGLLightModel::ViewerAtInfinity)
    {
        ambientSceneColor.setRgbF(0.2, 0.2, 0.2, 1.0);
    }

    QGLLightModel::Model model;
    QGLLightModel::ColorControl colorControl;
    QGLLightModel::ViewerPosition viewerPosition;
    QColor ambientSceneColor;
};

/*!
    Constructs a light model object with default values and attach
    it to \a parent.
*/
QGLLightModel::QGLLightModel(QObject *parent)
    : QObject(parent)
    , d_ptr(new QGLLightModelPrivate)
{
}

/*!
    Destroys this light model.
*/
QGLLightModel::~QGLLightModel()
{
}

/*!
    \property QGLLightModel::model
    \brief the lighting model to use, either OneSided or TwoSided.
    The default is OneSided.

    \sa modelChanged()
*/

/*!
    \qmlproperty enumeration LightModel::model
    The lighting model to use, either OneSided or TwoSided.
    The default is OneSided.
*/

QGLLightModel::Model QGLLightModel::model() const
{
    Q_D(const QGLLightModel);
    return d->model;
}

void QGLLightModel::setModel(QGLLightModel::Model value)
{
    Q_D(QGLLightModel);
    if (d->model != value) {
        d->model = value;
        emit modelChanged();
        emit lightModelChanged();
    }
}

/*!
    \property QGLLightModel::colorControl
    \brief the color control mode, either SingleColor or
    SeparateSpecularColor.  The default value is SingleColor.

    If SingleColor is specified, then a single color is calculated
    by the lighting computation for a vertex.  If SeparateSpecularColor
    is specified, then a separate specular color computation is
    performed and then summed into the pixel color after texture mapping.

    \sa colorControlChanged()
*/

/*!
    \qmlproperty enumeration LightModel::colorControl
    The color control mode, either SingleColor or
    SeparateSpecularColor.  The default value is SingleColor.

    If SingleColor is specified, then a single color is calculated
    by the lighting computation for a vertex.  If SeparateSpecularColor
    is specified, then a separate specular color computation is
    performed and then summed into the pixel color after texture mapping.
*/

QGLLightModel::ColorControl QGLLightModel::colorControl() const
{
    Q_D(const QGLLightModel);
    return d->colorControl;
}

void QGLLightModel::setColorControl(QGLLightModel::ColorControl value)
{
    Q_D(QGLLightModel);
    if (d->colorControl != value) {
        d->colorControl = value;
        emit colorControlChanged();
        emit lightModelChanged();
    }
}

/*!
    \property QGLLightModel::viewerPosition
    \brief the viewer position, either ViewerAtInfinity or LocalViewer.
    The default value is ViewerAtInfinity.

    \sa viewerPositionChanged()
*/

/*!
    \qmlproperty enumeration LightModel::viewerPosition
    The viewer position, either ViewerAtInfinity or LocalViewer.
    The default value is ViewerAtInfinity.
*/

QGLLightModel::ViewerPosition QGLLightModel::viewerPosition() const
{
    Q_D(const QGLLightModel);
    return d->viewerPosition;
}

void QGLLightModel::setViewerPosition(QGLLightModel::ViewerPosition value)
{
    Q_D(QGLLightModel);
    if (d->viewerPosition != value) {
        d->viewerPosition = value;
        emit viewerPositionChanged();
        emit lightModelChanged();
    }
}

/*!
    \property QGLLightModel::ambientSceneColor
    \brief the ambient color of the entire scene.  The default value
    is (0.2, 0.2, 0.2, 1.0).

    \sa ambientSceneColorChanged()
*/

/*!
    \qmlproperty color LightModel::ambientSceneColor
    The ambient color of the entire scene.  The default value
    is (0.2, 0.2, 0.2, 1.0).
*/

QColor QGLLightModel::ambientSceneColor() const
{
    Q_D(const QGLLightModel);
    return d->ambientSceneColor;
}

void QGLLightModel::setAmbientSceneColor(const QColor& value)
{
    Q_D(QGLLightModel);
    if (d->ambientSceneColor != value) {
        d->ambientSceneColor = value;
        emit ambientSceneColorChanged();
        emit lightModelChanged();
    }
}

/*!
    \fn void QGLLightModel::modelChanged()

    This signal is emitted when model() changes.

    \sa model(), lightModelChanged()
*/

/*!
    \fn void QGLLightModel::colorControlChanged()

    This signal is emitted when colorControl() changes.

    \sa colorControl(), lightModelChanged()
*/

/*!
    \fn void QGLLightModel::viewerPositionChanged()

    This signal is emitted when viewerPosition() changes.

    \sa viewerPosition(), lightModelChanged()
*/

/*!
    \fn void QGLLightModel::ambientSceneColorChanged()

    This signal is emitted when ambientSceneColor() changes.

    \sa ambientSceneColor(), lightModelChanged()
*/

/*!
    \fn void QGLLightModel::lightModelChanged()

    This signal is emitted when one of model(), colorControl(),
    viewerPosition(), or ambientSceneColor() changes.
*/

QT_END_NAMESPACE



#if !defined(QGL_GENERATOR_PROGRAM)

QT_BEGIN_NAMESPACE

// The following tables are generated by the program listed at the
// end of this source file.

static int const pickColors[4096] = {
    0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff,
    0xff8000, 0x80ff00, 0x8000ff, 0xff0080, 0x0080ff, 0x00ff80, 0xff80ff,
    0x80ffff, 0xffff80, 0x80ff80, 0xff8080, 0x8080ff, 0x808080, 0x800000,
    0x008000, 0x000080, 0x808000, 0x800080, 0x008080, 0xff80c0, 0x80c0ff,
    0xc0ff80, 0xffc080, 0x80ffc0, 0xc080ff, 0xffc000, 0xc0ff00, 0xc000ff,
    0xff00c0, 0x00c0ff, 0x00ffc0, 0xffc0ff, 0xc0ffff, 0xffffc0, 0xc0ffc0,
    0xffc0c0, 0xc0c0ff, 0x80c000, 0xc08000, 0xc00080, 0x8000c0, 0x00c080,
    0x0080c0, 0x80c080, 0xc08080, 0x8080c0, 0xc080c0, 0x80c0c0, 0xc0c080,
    0xc0c0c0, 0xc00000, 0x00c000, 0x0000c0, 0xc0c000, 0xc000c0, 0x00c0c0,
    0xff8040, 0x8040ff, 0x40ff80, 0xff4080, 0x80ff40, 0x4080ff, 0xffc040,
    0xc040ff, 0x40ffc0, 0xff40c0, 0xc0ff40, 0x40c0ff, 0xff4000, 0x40ff00,
    0x4000ff, 0xff0040, 0x0040ff, 0x00ff40, 0xff40ff, 0x40ffff, 0xffff40,
    0x40ff40, 0xff4040, 0x4040ff, 0x80c040, 0xc04080, 0x4080c0, 0x8040c0,
    0xc08040, 0x40c080, 0x804000, 0x408000, 0x400080, 0x800040, 0x004080,
    0x008040, 0x804080, 0x408080, 0x808040, 0x408040, 0x804040, 0x404080,
    0xc04000, 0x40c000, 0x4000c0, 0xc00040, 0x0040c0, 0x00c040, 0xc040c0,
    0x40c0c0, 0xc0c040, 0x40c040, 0xc04040, 0x4040c0, 0x404040, 0x400000,
    0x004000, 0x000040, 0x404000, 0x400040, 0x004040, 0xff80e0, 0x80e0ff,
    0xe0ff80, 0xffe080, 0x80ffe0, 0xe080ff, 0xffc0e0, 0xc0e0ff, 0xe0ffc0,
    0xffe0c0, 0xc0ffe0, 0xe0c0ff, 0xff40e0, 0x40e0ff, 0xe0ff40, 0xffe040,
    0x40ffe0, 0xe040ff, 0xffe000, 0xe0ff00, 0xe000ff, 0xff00e0, 0x00e0ff,
    0x00ffe0, 0xffe0ff, 0xe0ffff, 0xffffe0, 0xe0ffe0, 0xffe0e0, 0xe0e0ff,
    0x80c0e0, 0xc0e080, 0xe080c0, 0x80e0c0, 0xc080e0, 0xe0c080, 0x8040e0,
    0x40e080, 0xe08040, 0x80e040, 0x4080e0, 0xe04080, 0x80e000, 0xe08000,
    0xe00080, 0x8000e0, 0x00e080, 0x0080e0, 0x80e080, 0xe08080, 0x8080e0,
    0xe080e0, 0x80e0e0, 0xe0e080, 0xc040e0, 0x40e0c0, 0xe0c040, 0xc0e040,
    0x40c0e0, 0xe040c0, 0xc0e000, 0xe0c000, 0xe000c0, 0xc000e0, 0x00e0c0,
    0x00c0e0, 0xc0e0c0, 0xe0c0c0, 0xc0c0e0, 0xe0c0e0, 0xc0e0e0, 0xe0e0c0,
    0x40e000, 0xe04000, 0xe00040, 0x4000e0, 0x00e040, 0x0040e0, 0x40e040,
    0xe04040, 0x4040e0, 0xe040e0, 0x40e0e0, 0xe0e040, 0xe0e0e0, 0xe00000,
    0x00e000, 0x0000e0, 0xe0e000, 0xe000e0, 0x00e0e0, 0xff8060, 0x8060ff,
    0x60ff80, 0xff6080, 0x80ff60, 0x6080ff, 0xffc060, 0xc060ff, 0x60ffc0,
    0xff60c0, 0xc0ff60, 0x60c0ff, 0xff4060, 0x4060ff, 0x60ff40, 0xff6040,
    0x40ff60, 0x6040ff, 0xffe060, 0xe060ff, 0x60ffe0, 0xff60e0, 0xe0ff60,
    0x60e0ff, 0xff6000, 0x60ff00, 0x6000ff, 0xff0060, 0x0060ff, 0x00ff60,
    0xff60ff, 0x60ffff, 0xffff60, 0x60ff60, 0xff6060, 0x6060ff, 0x80c060,
    0xc06080, 0x6080c0, 0x8060c0, 0xc08060, 0x60c080, 0x804060, 0x406080,
    0x608040, 0x806040, 0x408060, 0x604080, 0x80e060, 0xe06080, 0x6080e0,
    0x8060e0, 0xe08060, 0x60e080, 0x806000, 0x608000, 0x600080, 0x800060,
    0x006080, 0x008060, 0x806080, 0x608080, 0x808060, 0x608060, 0x806060,
    0x606080, 0xc04060, 0x4060c0, 0x60c040, 0xc06040, 0x40c060, 0x6040c0,
    0xc0e060, 0xe060c0, 0x60c0e0, 0xc060e0, 0xe0c060, 0x60e0c0, 0xc06000,
    0x60c000, 0x6000c0, 0xc00060, 0x0060c0, 0x00c060, 0xc060c0, 0x60c0c0,
    0xc0c060, 0x60c060, 0xc06060, 0x6060c0, 0x40e060, 0xe06040, 0x6040e0,
    0x4060e0, 0xe04060, 0x60e040, 0x406000, 0x604000, 0x600040, 0x400060,
    0x006040, 0x004060, 0x406040, 0x604040, 0x404060, 0x604060, 0x406060,
    0x606040, 0xe06000, 0x60e000, 0x6000e0, 0xe00060, 0x0060e0, 0x00e060,
    0xe060e0, 0x60e0e0, 0xe0e060, 0x60e060, 0xe06060, 0x6060e0, 0x606060,
    0x600000, 0x006000, 0x000060, 0x606000, 0x600060, 0x006060, 0xff80a0,
    0x80a0ff, 0xa0ff80, 0xffa080, 0x80ffa0, 0xa080ff, 0xffc0a0, 0xc0a0ff,
    0xa0ffc0, 0xffa0c0, 0xc0ffa0, 0xa0c0ff, 0xff40a0, 0x40a0ff, 0xa0ff40,
    0xffa040, 0x40ffa0, 0xa040ff, 0xffe0a0, 0xe0a0ff, 0xa0ffe0, 0xffa0e0,
    0xe0ffa0, 0xa0e0ff, 0xff60a0, 0x60a0ff, 0xa0ff60, 0xffa060, 0x60ffa0,
    0xa060ff, 0xffa000, 0xa0ff00, 0xa000ff, 0xff00a0, 0x00a0ff, 0x00ffa0,
    0xffa0ff, 0xa0ffff, 0xffffa0, 0xa0ffa0, 0xffa0a0, 0xa0a0ff, 0x80c0a0,
    0xc0a080, 0xa080c0, 0x80a0c0, 0xc080a0, 0xa0c080, 0x8040a0, 0x40a080,
    0xa08040, 0x80a040, 0x4080a0, 0xa04080, 0x80e0a0, 0xe0a080, 0xa080e0,
    0x80a0e0, 0xe080a0, 0xa0e080, 0x8060a0, 0x60a080, 0xa08060, 0x80a060,
    0x6080a0, 0xa06080, 0x80a000, 0xa08000, 0xa00080, 0x8000a0, 0x00a080,
    0x0080a0, 0x80a080, 0xa08080, 0x8080a0, 0xa080a0, 0x80a0a0, 0xa0a080,
    0xc040a0, 0x40a0c0, 0xa0c040, 0xc0a040, 0x40c0a0, 0xa040c0, 0xc0e0a0,
    0xe0a0c0, 0xa0c0e0, 0xc0a0e0, 0xe0c0a0, 0xa0e0c0, 0xc060a0, 0x60a0c0,
    0xa0c060, 0xc0a060, 0x60c0a0, 0xa060c0, 0xc0a000, 0xa0c000, 0xa000c0,
    0xc000a0, 0x00a0c0, 0x00c0a0, 0xc0a0c0, 0xa0c0c0, 0xc0c0a0, 0xa0c0a0,
    0xc0a0a0, 0xa0a0c0, 0x40e0a0, 0xe0a040, 0xa040e0, 0x40a0e0, 0xe040a0,
    0xa0e040, 0x4060a0, 0x60a040, 0xa04060, 0x40a060, 0x6040a0, 0xa06040,
    0x40a000, 0xa04000, 0xa00040, 0x4000a0, 0x00a040, 0x0040a0, 0x40a040,
    0xa04040, 0x4040a0, 0xa040a0, 0x40a0a0, 0xa0a040, 0xe060a0, 0x60a0e0,
    0xa0e060, 0xe0a060, 0x60e0a0, 0xa060e0, 0xe0a000, 0xa0e000, 0xa000e0,
    0xe000a0, 0x00a0e0, 0x00e0a0, 0xe0a0e0, 0xa0e0e0, 0xe0e0a0, 0xa0e0a0,
    0xe0a0a0, 0xa0a0e0, 0x60a000, 0xa06000, 0xa00060, 0x6000a0, 0x00a060,
    0x0060a0, 0x60a060, 0xa06060, 0x6060a0, 0xa060a0, 0x60a0a0, 0xa0a060,
    0xa0a0a0, 0xa00000, 0x00a000, 0x0000a0, 0xa0a000, 0xa000a0, 0x00a0a0,
    0xff8020, 0x8020ff, 0x20ff80, 0xff2080, 0x80ff20, 0x2080ff, 0xffc020,
    0xc020ff, 0x20ffc0, 0xff20c0, 0xc0ff20, 0x20c0ff, 0xff4020, 0x4020ff,
    0x20ff40, 0xff2040, 0x40ff20, 0x2040ff, 0xffe020, 0xe020ff, 0x20ffe0,
    0xff20e0, 0xe0ff20, 0x20e0ff, 0xff6020, 0x6020ff, 0x20ff60, 0xff2060,
    0x60ff20, 0x2060ff, 0xffa020, 0xa020ff, 0x20ffa0, 0xff20a0, 0xa0ff20,
    0x20a0ff, 0xff2000, 0x20ff00, 0x2000ff, 0xff0020, 0x0020ff, 0x00ff20,
    0xff20ff, 0x20ffff, 0xffff20, 0x20ff20, 0xff2020, 0x2020ff, 0x80c020,
    0xc02080, 0x2080c0, 0x8020c0, 0xc08020, 0x20c080, 0x804020, 0x402080,
    0x208040, 0x802040, 0x408020, 0x204080, 0x80e020, 0xe02080, 0x2080e0,
    0x8020e0, 0xe08020, 0x20e080, 0x806020, 0x602080, 0x208060, 0x802060,
    0x608020, 0x206080, 0x80a020, 0xa02080, 0x2080a0, 0x8020a0, 0xa08020,
    0x20a080, 0x802000, 0x208000, 0x200080, 0x800020, 0x002080, 0x008020,
    0x802080, 0x208080, 0x808020, 0x208020, 0x802020, 0x202080, 0xc04020,
    0x4020c0, 0x20c040, 0xc02040, 0x40c020, 0x2040c0, 0xc0e020, 0xe020c0,
    0x20c0e0, 0xc020e0, 0xe0c020, 0x20e0c0, 0xc06020, 0x6020c0, 0x20c060,
    0xc02060, 0x60c020, 0x2060c0, 0xc0a020, 0xa020c0, 0x20c0a0, 0xc020a0,
    0xa0c020, 0x20a0c0, 0xc02000, 0x20c000, 0x2000c0, 0xc00020, 0x0020c0,
    0x00c020, 0xc020c0, 0x20c0c0, 0xc0c020, 0x20c020, 0xc02020, 0x2020c0,
    0x40e020, 0xe02040, 0x2040e0, 0x4020e0, 0xe04020, 0x20e040, 0x406020,
    0x602040, 0x204060, 0x402060, 0x604020, 0x206040, 0x40a020, 0xa02040,
    0x2040a0, 0x4020a0, 0xa04020, 0x20a040, 0x402000, 0x204000, 0x200040,
    0x400020, 0x002040, 0x004020, 0x402040, 0x204040, 0x404020, 0x204020,
    0x402020, 0x202040, 0xe06020, 0x6020e0, 0x20e060, 0xe02060, 0x60e020,
    0x2060e0, 0xe0a020, 0xa020e0, 0x20e0a0, 0xe020a0, 0xa0e020, 0x20a0e0,
    0xe02000, 0x20e000, 0x2000e0, 0xe00020, 0x0020e0, 0x00e020, 0xe020e0,
    0x20e0e0, 0xe0e020, 0x20e020, 0xe02020, 0x2020e0, 0x60a020, 0xa02060,
    0x2060a0, 0x6020a0, 0xa06020, 0x20a060, 0x602000, 0x206000, 0x200060,
    0x600020, 0x002060, 0x006020, 0x602060, 0x206060, 0x606020, 0x206020,
    0x602020, 0x202060, 0xa02000, 0x20a000, 0x2000a0, 0xa00020, 0x0020a0,
    0x00a020, 0xa020a0, 0x20a0a0, 0xa0a020, 0x20a020, 0xa02020, 0x2020a0,
    0x202020, 0x200000, 0x002000, 0x000020, 0x202000, 0x200020, 0x002020,
    0xff80f0, 0x80f0ff, 0xf0ff80, 0xfff080, 0x80fff0, 0xf080ff, 0xffc0f0,
    0xc0f0ff, 0xf0ffc0, 0xfff0c0, 0xc0fff0, 0xf0c0ff, 0xff40f0, 0x40f0ff,
    0xf0ff40, 0xfff040, 0x40fff0, 0xf040ff, 0xffe0f0, 0xe0f0ff, 0xf0ffe0,
    0xfff0e0, 0xe0fff0, 0xf0e0ff, 0xff60f0, 0x60f0ff, 0xf0ff60, 0xfff060,
    0x60fff0, 0xf060ff, 0xffa0f0, 0xa0f0ff, 0xf0ffa0, 0xfff0a0, 0xa0fff0,
    0xf0a0ff, 0xff20f0, 0x20f0ff, 0xf0ff20, 0xfff020, 0x20fff0, 0xf020ff,
    0xfff000, 0xf0ff00, 0xf000ff, 0xff00f0, 0x00f0ff, 0x00fff0, 0xfff0ff,
    0xf0ffff, 0xfffff0, 0xf0fff0, 0xfff0f0, 0xf0f0ff, 0x80c0f0, 0xc0f080,
    0xf080c0, 0x80f0c0, 0xc080f0, 0xf0c080, 0x8040f0, 0x40f080, 0xf08040,
    0x80f040, 0x4080f0, 0xf04080, 0x80e0f0, 0xe0f080, 0xf080e0, 0x80f0e0,
    0xe080f0, 0xf0e080, 0x8060f0, 0x60f080, 0xf08060, 0x80f060, 0x6080f0,
    0xf06080, 0x80a0f0, 0xa0f080, 0xf080a0, 0x80f0a0, 0xa080f0, 0xf0a080,
    0x8020f0, 0x20f080, 0xf08020, 0x80f020, 0x2080f0, 0xf02080, 0x80f000,
    0xf08000, 0xf00080, 0x8000f0, 0x00f080, 0x0080f0, 0x80f080, 0xf08080,
    0x8080f0, 0xf080f0, 0x80f0f0, 0xf0f080, 0xc040f0, 0x40f0c0, 0xf0c040,
    0xc0f040, 0x40c0f0, 0xf040c0, 0xc0e0f0, 0xe0f0c0, 0xf0c0e0, 0xc0f0e0,
    0xe0c0f0, 0xf0e0c0, 0xc060f0, 0x60f0c0, 0xf0c060, 0xc0f060, 0x60c0f0,
    0xf060c0, 0xc0a0f0, 0xa0f0c0, 0xf0c0a0, 0xc0f0a0, 0xa0c0f0, 0xf0a0c0,
    0xc020f0, 0x20f0c0, 0xf0c020, 0xc0f020, 0x20c0f0, 0xf020c0, 0xc0f000,
    0xf0c000, 0xf000c0, 0xc000f0, 0x00f0c0, 0x00c0f0, 0xc0f0c0, 0xf0c0c0,
    0xc0c0f0, 0xf0c0f0, 0xc0f0f0, 0xf0f0c0, 0x40e0f0, 0xe0f040, 0xf040e0,
    0x40f0e0, 0xe040f0, 0xf0e040, 0x4060f0, 0x60f040, 0xf04060, 0x40f060,
    0x6040f0, 0xf06040, 0x40a0f0, 0xa0f040, 0xf040a0, 0x40f0a0, 0xa040f0,
    0xf0a040, 0x4020f0, 0x20f040, 0xf04020, 0x40f020, 0x2040f0, 0xf02040,
    0x40f000, 0xf04000, 0xf00040, 0x4000f0, 0x00f040, 0x0040f0, 0x40f040,
    0xf04040, 0x4040f0, 0xf040f0, 0x40f0f0, 0xf0f040, 0xe060f0, 0x60f0e0,
    0xf0e060, 0xe0f060, 0x60e0f0, 0xf060e0, 0xe0a0f0, 0xa0f0e0, 0xf0e0a0,
    0xe0f0a0, 0xa0e0f0, 0xf0a0e0, 0xe020f0, 0x20f0e0, 0xf0e020, 0xe0f020,
    0x20e0f0, 0xf020e0, 0xe0f000, 0xf0e000, 0xf000e0, 0xe000f0, 0x00f0e0,
    0x00e0f0, 0xe0f0e0, 0xf0e0e0, 0xe0e0f0, 0xf0e0f0, 0xe0f0f0, 0xf0f0e0,
    0x60a0f0, 0xa0f060, 0xf060a0, 0x60f0a0, 0xa060f0, 0xf0a060, 0x6020f0,
    0x20f060, 0xf06020, 0x60f020, 0x2060f0, 0xf02060, 0x60f000, 0xf06000,
    0xf00060, 0x6000f0, 0x00f060, 0x0060f0, 0x60f060, 0xf06060, 0x6060f0,
    0xf060f0, 0x60f0f0, 0xf0f060, 0xa020f0, 0x20f0a0, 0xf0a020, 0xa0f020,
    0x20a0f0, 0xf020a0, 0xa0f000, 0xf0a000, 0xf000a0, 0xa000f0, 0x00f0a0,
    0x00a0f0, 0xa0f0a0, 0xf0a0a0, 0xa0a0f0, 0xf0a0f0, 0xa0f0f0, 0xf0f0a0,
    0x20f000, 0xf02000, 0xf00020, 0x2000f0, 0x00f020, 0x0020f0, 0x20f020,
    0xf02020, 0x2020f0, 0xf020f0, 0x20f0f0, 0xf0f020, 0xf0f0f0, 0xf00000,
    0x00f000, 0x0000f0, 0xf0f000, 0xf000f0, 0x00f0f0, 0xff80b0, 0x80b0ff,
    0xb0ff80, 0xffb080, 0x80ffb0, 0xb080ff, 0xffc0b0, 0xc0b0ff, 0xb0ffc0,
    0xffb0c0, 0xc0ffb0, 0xb0c0ff, 0xff40b0, 0x40b0ff, 0xb0ff40, 0xffb040,
    0x40ffb0, 0xb040ff, 0xffe0b0, 0xe0b0ff, 0xb0ffe0, 0xffb0e0, 0xe0ffb0,
    0xb0e0ff, 0xff60b0, 0x60b0ff, 0xb0ff60, 0xffb060, 0x60ffb0, 0xb060ff,
    0xffa0b0, 0xa0b0ff, 0xb0ffa0, 0xffb0a0, 0xa0ffb0, 0xb0a0ff, 0xff20b0,
    0x20b0ff, 0xb0ff20, 0xffb020, 0x20ffb0, 0xb020ff, 0xfff0b0, 0xf0b0ff,
    0xb0fff0, 0xffb0f0, 0xf0ffb0, 0xb0f0ff, 0xffb000, 0xb0ff00, 0xb000ff,
    0xff00b0, 0x00b0ff, 0x00ffb0, 0xffb0ff, 0xb0ffff, 0xffffb0, 0xb0ffb0,
    0xffb0b0, 0xb0b0ff, 0x80c0b0, 0xc0b080, 0xb080c0, 0x80b0c0, 0xc080b0,
    0xb0c080, 0x8040b0, 0x40b080, 0xb08040, 0x80b040, 0x4080b0, 0xb04080,
    0x80e0b0, 0xe0b080, 0xb080e0, 0x80b0e0, 0xe080b0, 0xb0e080, 0x8060b0,
    0x60b080, 0xb08060, 0x80b060, 0x6080b0, 0xb06080, 0x80a0b0, 0xa0b080,
    0xb080a0, 0x80b0a0, 0xa080b0, 0xb0a080, 0x8020b0, 0x20b080, 0xb08020,
    0x80b020, 0x2080b0, 0xb02080, 0x80f0b0, 0xf0b080, 0xb080f0, 0x80b0f0,
    0xf080b0, 0xb0f080, 0x80b000, 0xb08000, 0xb00080, 0x8000b0, 0x00b080,
    0x0080b0, 0x80b080, 0xb08080, 0x8080b0, 0xb080b0, 0x80b0b0, 0xb0b080,
    0xc040b0, 0x40b0c0, 0xb0c040, 0xc0b040, 0x40c0b0, 0xb040c0, 0xc0e0b0,
    0xe0b0c0, 0xb0c0e0, 0xc0b0e0, 0xe0c0b0, 0xb0e0c0, 0xc060b0, 0x60b0c0,
    0xb0c060, 0xc0b060, 0x60c0b0, 0xb060c0, 0xc0a0b0, 0xa0b0c0, 0xb0c0a0,
    0xc0b0a0, 0xa0c0b0, 0xb0a0c0, 0xc020b0, 0x20b0c0, 0xb0c020, 0xc0b020,
    0x20c0b0, 0xb020c0, 0xc0f0b0, 0xf0b0c0, 0xb0c0f0, 0xc0b0f0, 0xf0c0b0,
    0xb0f0c0, 0xc0b000, 0xb0c000, 0xb000c0, 0xc000b0, 0x00b0c0, 0x00c0b0,
    0xc0b0c0, 0xb0c0c0, 0xc0c0b0, 0xb0c0b0, 0xc0b0b0, 0xb0b0c0, 0x40e0b0,
    0xe0b040, 0xb040e0, 0x40b0e0, 0xe040b0, 0xb0e040, 0x4060b0, 0x60b040,
    0xb04060, 0x40b060, 0x6040b0, 0xb06040, 0x40a0b0, 0xa0b040, 0xb040a0,
    0x40b0a0, 0xa040b0, 0xb0a040, 0x4020b0, 0x20b040, 0xb04020, 0x40b020,
    0x2040b0, 0xb02040, 0x40f0b0, 0xf0b040, 0xb040f0, 0x40b0f0, 0xf040b0,
    0xb0f040, 0x40b000, 0xb04000, 0xb00040, 0x4000b0, 0x00b040, 0x0040b0,
    0x40b040, 0xb04040, 0x4040b0, 0xb040b0, 0x40b0b0, 0xb0b040, 0xe060b0,
    0x60b0e0, 0xb0e060, 0xe0b060, 0x60e0b0, 0xb060e0, 0xe0a0b0, 0xa0b0e0,
    0xb0e0a0, 0xe0b0a0, 0xa0e0b0, 0xb0a0e0, 0xe020b0, 0x20b0e0, 0xb0e020,
    0xe0b020, 0x20e0b0, 0xb020e0, 0xe0f0b0, 0xf0b0e0, 0xb0e0f0, 0xe0b0f0,
    0xf0e0b0, 0xb0f0e0, 0xe0b000, 0xb0e000, 0xb000e0, 0xe000b0, 0x00b0e0,
    0x00e0b0, 0xe0b0e0, 0xb0e0e0, 0xe0e0b0, 0xb0e0b0, 0xe0b0b0, 0xb0b0e0,
    0x60a0b0, 0xa0b060, 0xb060a0, 0x60b0a0, 0xa060b0, 0xb0a060, 0x6020b0,
    0x20b060, 0xb06020, 0x60b020, 0x2060b0, 0xb02060, 0x60f0b0, 0xf0b060,
    0xb060f0, 0x60b0f0, 0xf060b0, 0xb0f060, 0x60b000, 0xb06000, 0xb00060,
    0x6000b0, 0x00b060, 0x0060b0, 0x60b060, 0xb06060, 0x6060b0, 0xb060b0,
    0x60b0b0, 0xb0b060, 0xa020b0, 0x20b0a0, 0xb0a020, 0xa0b020, 0x20a0b0,
    0xb020a0, 0xa0f0b0, 0xf0b0a0, 0xb0a0f0, 0xa0b0f0, 0xf0a0b0, 0xb0f0a0,
    0xa0b000, 0xb0a000, 0xb000a0, 0xa000b0, 0x00b0a0, 0x00a0b0, 0xa0b0a0,
    0xb0a0a0, 0xa0a0b0, 0xb0a0b0, 0xa0b0b0, 0xb0b0a0, 0x20f0b0, 0xf0b020,
    0xb020f0, 0x20b0f0, 0xf020b0, 0xb0f020, 0x20b000, 0xb02000, 0xb00020,
    0x2000b0, 0x00b020, 0x0020b0, 0x20b020, 0xb02020, 0x2020b0, 0xb020b0,
    0x20b0b0, 0xb0b020, 0xf0b000, 0xb0f000, 0xb000f0, 0xf000b0, 0x00b0f0,
    0x00f0b0, 0xf0b0f0, 0xb0f0f0, 0xf0f0b0, 0xb0f0b0, 0xf0b0b0, 0xb0b0f0,
    0xb0b0b0, 0xb00000, 0x00b000, 0x0000b0, 0xb0b000, 0xb000b0, 0x00b0b0,
    0xff8050, 0x8050ff, 0x50ff80, 0xff5080, 0x80ff50, 0x5080ff, 0xffc050,
    0xc050ff, 0x50ffc0, 0xff50c0, 0xc0ff50, 0x50c0ff, 0xff4050, 0x4050ff,
    0x50ff40, 0xff5040, 0x40ff50, 0x5040ff, 0xffe050, 0xe050ff, 0x50ffe0,
    0xff50e0, 0xe0ff50, 0x50e0ff, 0xff6050, 0x6050ff, 0x50ff60, 0xff5060,
    0x60ff50, 0x5060ff, 0xffa050, 0xa050ff, 0x50ffa0, 0xff50a0, 0xa0ff50,
    0x50a0ff, 0xff2050, 0x2050ff, 0x50ff20, 0xff5020, 0x20ff50, 0x5020ff,
    0xfff050, 0xf050ff, 0x50fff0, 0xff50f0, 0xf0ff50, 0x50f0ff, 0xffb050,
    0xb050ff, 0x50ffb0, 0xff50b0, 0xb0ff50, 0x50b0ff, 0xff5000, 0x50ff00,
    0x5000ff, 0xff0050, 0x0050ff, 0x00ff50, 0xff50ff, 0x50ffff, 0xffff50,
    0x50ff50, 0xff5050, 0x5050ff, 0x80c050, 0xc05080, 0x5080c0, 0x8050c0,
    0xc08050, 0x50c080, 0x804050, 0x405080, 0x508040, 0x805040, 0x408050,
    0x504080, 0x80e050, 0xe05080, 0x5080e0, 0x8050e0, 0xe08050, 0x50e080,
    0x806050, 0x605080, 0x508060, 0x805060, 0x608050, 0x506080, 0x80a050,
    0xa05080, 0x5080a0, 0x8050a0, 0xa08050, 0x50a080, 0x802050, 0x205080,
    0x508020, 0x805020, 0x208050, 0x502080, 0x80f050, 0xf05080, 0x5080f0,
    0x8050f0, 0xf08050, 0x50f080, 0x80b050, 0xb05080, 0x5080b0, 0x8050b0,
    0xb08050, 0x50b080, 0x805000, 0x508000, 0x500080, 0x800050, 0x005080,
    0x008050, 0x805080, 0x508080, 0x808050, 0x508050, 0x805050, 0x505080,
    0xc04050, 0x4050c0, 0x50c040, 0xc05040, 0x40c050, 0x5040c0, 0xc0e050,
    0xe050c0, 0x50c0e0, 0xc050e0, 0xe0c050, 0x50e0c0, 0xc06050, 0x6050c0,
    0x50c060, 0xc05060, 0x60c050, 0x5060c0, 0xc0a050, 0xa050c0, 0x50c0a0,
    0xc050a0, 0xa0c050, 0x50a0c0, 0xc02050, 0x2050c0, 0x50c020, 0xc05020,
    0x20c050, 0x5020c0, 0xc0f050, 0xf050c0, 0x50c0f0, 0xc050f0, 0xf0c050,
    0x50f0c0, 0xc0b050, 0xb050c0, 0x50c0b0, 0xc050b0, 0xb0c050, 0x50b0c0,
    0xc05000, 0x50c000, 0x5000c0, 0xc00050, 0x0050c0, 0x00c050, 0xc050c0,
    0x50c0c0, 0xc0c050, 0x50c050, 0xc05050, 0x5050c0, 0x40e050, 0xe05040,
    0x5040e0, 0x4050e0, 0xe04050, 0x50e040, 0x406050, 0x605040, 0x504060,
    0x405060, 0x604050, 0x506040, 0x40a050, 0xa05040, 0x5040a0, 0x4050a0,
    0xa04050, 0x50a040, 0x402050, 0x205040, 0x504020, 0x405020, 0x204050,
    0x502040, 0x40f050, 0xf05040, 0x5040f0, 0x4050f0, 0xf04050, 0x50f040,
    0x40b050, 0xb05040, 0x5040b0, 0x4050b0, 0xb04050, 0x50b040, 0x405000,
    0x504000, 0x500040, 0x400050, 0x005040, 0x004050, 0x405040, 0x504040,
    0x404050, 0x504050, 0x405050, 0x505040, 0xe06050, 0x6050e0, 0x50e060,
    0xe05060, 0x60e050, 0x5060e0, 0xe0a050, 0xa050e0, 0x50e0a0, 0xe050a0,
    0xa0e050, 0x50a0e0, 0xe02050, 0x2050e0, 0x50e020, 0xe05020, 0x20e050,
    0x5020e0, 0xe0f050, 0xf050e0, 0x50e0f0, 0xe050f0, 0xf0e050, 0x50f0e0,
    0xe0b050, 0xb050e0, 0x50e0b0, 0xe050b0, 0xb0e050, 0x50b0e0, 0xe05000,
    0x50e000, 0x5000e0, 0xe00050, 0x0050e0, 0x00e050, 0xe050e0, 0x50e0e0,
    0xe0e050, 0x50e050, 0xe05050, 0x5050e0, 0x60a050, 0xa05060, 0x5060a0,
    0x6050a0, 0xa06050, 0x50a060, 0x602050, 0x205060, 0x506020, 0x605020,
    0x206050, 0x502060, 0x60f050, 0xf05060, 0x5060f0, 0x6050f0, 0xf06050,
    0x50f060, 0x60b050, 0xb05060, 0x5060b0, 0x6050b0, 0xb06050, 0x50b060,
    0x605000, 0x506000, 0x500060, 0x600050, 0x005060, 0x006050, 0x605060,
    0x506060, 0x606050, 0x506050, 0x605050, 0x505060, 0xa02050, 0x2050a0,
    0x50a020, 0xa05020, 0x20a050, 0x5020a0, 0xa0f050, 0xf050a0, 0x50a0f0,
    0xa050f0, 0xf0a050, 0x50f0a0, 0xa0b050, 0xb050a0, 0x50a0b0, 0xa050b0,
    0xb0a050, 0x50b0a0, 0xa05000, 0x50a000, 0x5000a0, 0xa00050, 0x0050a0,
    0x00a050, 0xa050a0, 0x50a0a0, 0xa0a050, 0x50a050, 0xa05050, 0x5050a0,
    0x20f050, 0xf05020, 0x5020f0, 0x2050f0, 0xf02050, 0x50f020, 0x20b050,
    0xb05020, 0x5020b0, 0x2050b0, 0xb02050, 0x50b020, 0x205000, 0x502000,
    0x500020, 0x200050, 0x005020, 0x002050, 0x205020, 0x502020, 0x202050,
    0x502050, 0x205050, 0x505020, 0xf0b050, 0xb050f0, 0x50f0b0, 0xf050b0,
    0xb0f050, 0x50b0f0, 0xf05000, 0x50f000, 0x5000f0, 0xf00050, 0x0050f0,
    0x00f050, 0xf050f0, 0x50f0f0, 0xf0f050, 0x50f050, 0xf05050, 0x5050f0,
    0xb05000, 0x50b000, 0x5000b0, 0xb00050, 0x0050b0, 0x00b050, 0xb050b0,
    0x50b0b0, 0xb0b050, 0x50b050, 0xb05050, 0x5050b0, 0x505050, 0x500000,
    0x005000, 0x000050, 0x505000, 0x500050, 0x005050, 0xff80d0, 0x80d0ff,
    0xd0ff80, 0xffd080, 0x80ffd0, 0xd080ff, 0xffc0d0, 0xc0d0ff, 0xd0ffc0,
    0xffd0c0, 0xc0ffd0, 0xd0c0ff, 0xff40d0, 0x40d0ff, 0xd0ff40, 0xffd040,
    0x40ffd0, 0xd040ff, 0xffe0d0, 0xe0d0ff, 0xd0ffe0, 0xffd0e0, 0xe0ffd0,
    0xd0e0ff, 0xff60d0, 0x60d0ff, 0xd0ff60, 0xffd060, 0x60ffd0, 0xd060ff,
    0xffa0d0, 0xa0d0ff, 0xd0ffa0, 0xffd0a0, 0xa0ffd0, 0xd0a0ff, 0xff20d0,
    0x20d0ff, 0xd0ff20, 0xffd020, 0x20ffd0, 0xd020ff, 0xfff0d0, 0xf0d0ff,
    0xd0fff0, 0xffd0f0, 0xf0ffd0, 0xd0f0ff, 0xffb0d0, 0xb0d0ff, 0xd0ffb0,
    0xffd0b0, 0xb0ffd0, 0xd0b0ff, 0xff50d0, 0x50d0ff, 0xd0ff50, 0xffd050,
    0x50ffd0, 0xd050ff, 0xffd000, 0xd0ff00, 0xd000ff, 0xff00d0, 0x00d0ff,
    0x00ffd0, 0xffd0ff, 0xd0ffff, 0xffffd0, 0xd0ffd0, 0xffd0d0, 0xd0d0ff,
    0x80c0d0, 0xc0d080, 0xd080c0, 0x80d0c0, 0xc080d0, 0xd0c080, 0x8040d0,
    0x40d080, 0xd08040, 0x80d040, 0x4080d0, 0xd04080, 0x80e0d0, 0xe0d080,
    0xd080e0, 0x80d0e0, 0xe080d0, 0xd0e080, 0x8060d0, 0x60d080, 0xd08060,
    0x80d060, 0x6080d0, 0xd06080, 0x80a0d0, 0xa0d080, 0xd080a0, 0x80d0a0,
    0xa080d0, 0xd0a080, 0x8020d0, 0x20d080, 0xd08020, 0x80d020, 0x2080d0,
    0xd02080, 0x80f0d0, 0xf0d080, 0xd080f0, 0x80d0f0, 0xf080d0, 0xd0f080,
    0x80b0d0, 0xb0d080, 0xd080b0, 0x80d0b0, 0xb080d0, 0xd0b080, 0x8050d0,
    0x50d080, 0xd08050, 0x80d050, 0x5080d0, 0xd05080, 0x80d000, 0xd08000,
    0xd00080, 0x8000d0, 0x00d080, 0x0080d0, 0x80d080, 0xd08080, 0x8080d0,
    0xd080d0, 0x80d0d0, 0xd0d080, 0xc040d0, 0x40d0c0, 0xd0c040, 0xc0d040,
    0x40c0d0, 0xd040c0, 0xc0e0d0, 0xe0d0c0, 0xd0c0e0, 0xc0d0e0, 0xe0c0d0,
    0xd0e0c0, 0xc060d0, 0x60d0c0, 0xd0c060, 0xc0d060, 0x60c0d0, 0xd060c0,
    0xc0a0d0, 0xa0d0c0, 0xd0c0a0, 0xc0d0a0, 0xa0c0d0, 0xd0a0c0, 0xc020d0,
    0x20d0c0, 0xd0c020, 0xc0d020, 0x20c0d0, 0xd020c0, 0xc0f0d0, 0xf0d0c0,
    0xd0c0f0, 0xc0d0f0, 0xf0c0d0, 0xd0f0c0, 0xc0b0d0, 0xb0d0c0, 0xd0c0b0,
    0xc0d0b0, 0xb0c0d0, 0xd0b0c0, 0xc050d0, 0x50d0c0, 0xd0c050, 0xc0d050,
    0x50c0d0, 0xd050c0, 0xc0d000, 0xd0c000, 0xd000c0, 0xc000d0, 0x00d0c0,
    0x00c0d0, 0xc0d0c0, 0xd0c0c0, 0xc0c0d0, 0xd0c0d0, 0xc0d0d0, 0xd0d0c0,
    0x40e0d0, 0xe0d040, 0xd040e0, 0x40d0e0, 0xe040d0, 0xd0e040, 0x4060d0,
    0x60d040, 0xd04060, 0x40d060, 0x6040d0, 0xd06040, 0x40a0d0, 0xa0d040,
    0xd040a0, 0x40d0a0, 0xa040d0, 0xd0a040, 0x4020d0, 0x20d040, 0xd04020,
    0x40d020, 0x2040d0, 0xd02040, 0x40f0d0, 0xf0d040, 0xd040f0, 0x40d0f0,
    0xf040d0, 0xd0f040, 0x40b0d0, 0xb0d040, 0xd040b0, 0x40d0b0, 0xb040d0,
    0xd0b040, 0x4050d0, 0x50d040, 0xd04050, 0x40d050, 0x5040d0, 0xd05040,
    0x40d000, 0xd04000, 0xd00040, 0x4000d0, 0x00d040, 0x0040d0, 0x40d040,
    0xd04040, 0x4040d0, 0xd040d0, 0x40d0d0, 0xd0d040, 0xe060d0, 0x60d0e0,
    0xd0e060, 0xe0d060, 0x60e0d0, 0xd060e0, 0xe0a0d0, 0xa0d0e0, 0xd0e0a0,
    0xe0d0a0, 0xa0e0d0, 0xd0a0e0, 0xe020d0, 0x20d0e0, 0xd0e020, 0xe0d020,
    0x20e0d0, 0xd020e0, 0xe0f0d0, 0xf0d0e0, 0xd0e0f0, 0xe0d0f0, 0xf0e0d0,
    0xd0f0e0, 0xe0b0d0, 0xb0d0e0, 0xd0e0b0, 0xe0d0b0, 0xb0e0d0, 0xd0b0e0,
    0xe050d0, 0x50d0e0, 0xd0e050, 0xe0d050, 0x50e0d0, 0xd050e0, 0xe0d000,
    0xd0e000, 0xd000e0, 0xe000d0, 0x00d0e0, 0x00e0d0, 0xe0d0e0, 0xd0e0e0,
    0xe0e0d0, 0xd0e0d0, 0xe0d0d0, 0xd0d0e0, 0x60a0d0, 0xa0d060, 0xd060a0,
    0x60d0a0, 0xa060d0, 0xd0a060, 0x6020d0, 0x20d060, 0xd06020, 0x60d020,
    0x2060d0, 0xd02060, 0x60f0d0, 0xf0d060, 0xd060f0, 0x60d0f0, 0xf060d0,
    0xd0f060, 0x60b0d0, 0xb0d060, 0xd060b0, 0x60d0b0, 0xb060d0, 0xd0b060,
    0x6050d0, 0x50d060, 0xd06050, 0x60d050, 0x5060d0, 0xd05060, 0x60d000,
    0xd06000, 0xd00060, 0x6000d0, 0x00d060, 0x0060d0, 0x60d060, 0xd06060,
    0x6060d0, 0xd060d0, 0x60d0d0, 0xd0d060, 0xa020d0, 0x20d0a0, 0xd0a020,
    0xa0d020, 0x20a0d0, 0xd020a0, 0xa0f0d0, 0xf0d0a0, 0xd0a0f0, 0xa0d0f0,
    0xf0a0d0, 0xd0f0a0, 0xa0b0d0, 0xb0d0a0, 0xd0a0b0, 0xa0d0b0, 0xb0a0d0,
    0xd0b0a0, 0xa050d0, 0x50d0a0, 0xd0a050, 0xa0d050, 0x50a0d0, 0xd050a0,
    0xa0d000, 0xd0a000, 0xd000a0, 0xa000d0, 0x00d0a0, 0x00a0d0, 0xa0d0a0,
    0xd0a0a0, 0xa0a0d0, 0xd0a0d0, 0xa0d0d0, 0xd0d0a0, 0x20f0d0, 0xf0d020,
    0xd020f0, 0x20d0f0, 0xf020d0, 0xd0f020, 0x20b0d0, 0xb0d020, 0xd020b0,
    0x20d0b0, 0xb020d0, 0xd0b020, 0x2050d0, 0x50d020, 0xd02050, 0x20d050,
    0x5020d0, 0xd05020, 0x20d000, 0xd02000, 0xd00020, 0x2000d0, 0x00d020,
    0x0020d0, 0x20d020, 0xd02020, 0x2020d0, 0xd020d0, 0x20d0d0, 0xd0d020,
    0xf0b0d0, 0xb0d0f0, 0xd0f0b0, 0xf0d0b0, 0xb0f0d0, 0xd0b0f0, 0xf050d0,
    0x50d0f0, 0xd0f050, 0xf0d050, 0x50f0d0, 0xd050f0, 0xf0d000, 0xd0f000,
    0xd000f0, 0xf000d0, 0x00d0f0, 0x00f0d0, 0xf0d0f0, 0xd0f0f0, 0xf0f0d0,
    0xd0f0d0, 0xf0d0d0, 0xd0d0f0, 0xb050d0, 0x50d0b0, 0xd0b050, 0xb0d050,
    0x50b0d0, 0xd050b0, 0xb0d000, 0xd0b000, 0xd000b0, 0xb000d0, 0x00d0b0,
    0x00b0d0, 0xb0d0b0, 0xd0b0b0, 0xb0b0d0, 0xd0b0d0, 0xb0d0d0, 0xd0d0b0,
    0x50d000, 0xd05000, 0xd00050, 0x5000d0, 0x00d050, 0x0050d0, 0x50d050,
    0xd05050, 0x5050d0, 0xd050d0, 0x50d0d0, 0xd0d050, 0xd0d0d0, 0xd00000,
    0x00d000, 0x0000d0, 0xd0d000, 0xd000d0, 0x00d0d0, 0xff8070, 0x8070ff,
    0x70ff80, 0xff7080, 0x80ff70, 0x7080ff, 0xffc070, 0xc070ff, 0x70ffc0,
    0xff70c0, 0xc0ff70, 0x70c0ff, 0xff4070, 0x4070ff, 0x70ff40, 0xff7040,
    0x40ff70, 0x7040ff, 0xffe070, 0xe070ff, 0x70ffe0, 0xff70e0, 0xe0ff70,
    0x70e0ff, 0xff6070, 0x6070ff, 0x70ff60, 0xff7060, 0x60ff70, 0x7060ff,
    0xffa070, 0xa070ff, 0x70ffa0, 0xff70a0, 0xa0ff70, 0x70a0ff, 0xff2070,
    0x2070ff, 0x70ff20, 0xff7020, 0x20ff70, 0x7020ff, 0xfff070, 0xf070ff,
    0x70fff0, 0xff70f0, 0xf0ff70, 0x70f0ff, 0xffb070, 0xb070ff, 0x70ffb0,
    0xff70b0, 0xb0ff70, 0x70b0ff, 0xff5070, 0x5070ff, 0x70ff50, 0xff7050,
    0x50ff70, 0x7050ff, 0xffd070, 0xd070ff, 0x70ffd0, 0xff70d0, 0xd0ff70,
    0x70d0ff, 0xff7000, 0x70ff00, 0x7000ff, 0xff0070, 0x0070ff, 0x00ff70,
    0xff70ff, 0x70ffff, 0xffff70, 0x70ff70, 0xff7070, 0x7070ff, 0x80c070,
    0xc07080, 0x7080c0, 0x8070c0, 0xc08070, 0x70c080, 0x804070, 0x407080,
    0x708040, 0x807040, 0x408070, 0x704080, 0x80e070, 0xe07080, 0x7080e0,
    0x8070e0, 0xe08070, 0x70e080, 0x806070, 0x607080, 0x708060, 0x807060,
    0x608070, 0x706080, 0x80a070, 0xa07080, 0x7080a0, 0x8070a0, 0xa08070,
    0x70a080, 0x802070, 0x207080, 0x708020, 0x807020, 0x208070, 0x702080,
    0x80f070, 0xf07080, 0x7080f0, 0x8070f0, 0xf08070, 0x70f080, 0x80b070,
    0xb07080, 0x7080b0, 0x8070b0, 0xb08070, 0x70b080, 0x805070, 0x507080,
    0x708050, 0x807050, 0x508070, 0x705080, 0x80d070, 0xd07080, 0x7080d0,
    0x8070d0, 0xd08070, 0x70d080, 0x807000, 0x708000, 0x700080, 0x800070,
    0x007080, 0x008070, 0x807080, 0x708080, 0x808070, 0x708070, 0x807070,
    0x707080, 0xc04070, 0x4070c0, 0x70c040, 0xc07040, 0x40c070, 0x7040c0,
    0xc0e070, 0xe070c0, 0x70c0e0, 0xc070e0, 0xe0c070, 0x70e0c0, 0xc06070,
    0x6070c0, 0x70c060, 0xc07060, 0x60c070, 0x7060c0, 0xc0a070, 0xa070c0,
    0x70c0a0, 0xc070a0, 0xa0c070, 0x70a0c0, 0xc02070, 0x2070c0, 0x70c020,
    0xc07020, 0x20c070, 0x7020c0, 0xc0f070, 0xf070c0, 0x70c0f0, 0xc070f0,
    0xf0c070, 0x70f0c0, 0xc0b070, 0xb070c0, 0x70c0b0, 0xc070b0, 0xb0c070,
    0x70b0c0, 0xc05070, 0x5070c0, 0x70c050, 0xc07050, 0x50c070, 0x7050c0,
    0xc0d070, 0xd070c0, 0x70c0d0, 0xc070d0, 0xd0c070, 0x70d0c0, 0xc07000,
    0x70c000, 0x7000c0, 0xc00070, 0x0070c0, 0x00c070, 0xc070c0, 0x70c0c0,
    0xc0c070, 0x70c070, 0xc07070, 0x7070c0, 0x40e070, 0xe07040, 0x7040e0,
    0x4070e0, 0xe04070, 0x70e040, 0x406070, 0x607040, 0x704060, 0x407060,
    0x604070, 0x706040, 0x40a070, 0xa07040, 0x7040a0, 0x4070a0, 0xa04070,
    0x70a040, 0x402070, 0x207040, 0x704020, 0x407020, 0x204070, 0x702040,
    0x40f070, 0xf07040, 0x7040f0, 0x4070f0, 0xf04070, 0x70f040, 0x40b070,
    0xb07040, 0x7040b0, 0x4070b0, 0xb04070, 0x70b040, 0x405070, 0x507040,
    0x704050, 0x407050, 0x504070, 0x705040, 0x40d070, 0xd07040, 0x7040d0,
    0x4070d0, 0xd04070, 0x70d040, 0x407000, 0x704000, 0x700040, 0x400070,
    0x007040, 0x004070, 0x407040, 0x704040, 0x404070, 0x704070, 0x407070,
    0x707040, 0xe06070, 0x6070e0, 0x70e060, 0xe07060, 0x60e070, 0x7060e0,
    0xe0a070, 0xa070e0, 0x70e0a0, 0xe070a0, 0xa0e070, 0x70a0e0, 0xe02070,
    0x2070e0, 0x70e020, 0xe07020, 0x20e070, 0x7020e0, 0xe0f070, 0xf070e0,
    0x70e0f0, 0xe070f0, 0xf0e070, 0x70f0e0, 0xe0b070, 0xb070e0, 0x70e0b0,
    0xe070b0, 0xb0e070, 0x70b0e0, 0xe05070, 0x5070e0, 0x70e050, 0xe07050,
    0x50e070, 0x7050e0, 0xe0d070, 0xd070e0, 0x70e0d0, 0xe070d0, 0xd0e070,
    0x70d0e0, 0xe07000, 0x70e000, 0x7000e0, 0xe00070, 0x0070e0, 0x00e070,
    0xe070e0, 0x70e0e0, 0xe0e070, 0x70e070, 0xe07070, 0x7070e0, 0x60a070,
    0xa07060, 0x7060a0, 0x6070a0, 0xa06070, 0x70a060, 0x602070, 0x207060,
    0x706020, 0x607020, 0x206070, 0x702060, 0x60f070, 0xf07060, 0x7060f0,
    0x6070f0, 0xf06070, 0x70f060, 0x60b070, 0xb07060, 0x7060b0, 0x6070b0,
    0xb06070, 0x70b060, 0x605070, 0x507060, 0x706050, 0x607050, 0x506070,
    0x705060, 0x60d070, 0xd07060, 0x7060d0, 0x6070d0, 0xd06070, 0x70d060,
    0x607000, 0x706000, 0x700060, 0x600070, 0x007060, 0x006070, 0x607060,
    0x706060, 0x606070, 0x706070, 0x607070, 0x707060, 0xa02070, 0x2070a0,
    0x70a020, 0xa07020, 0x20a070, 0x7020a0, 0xa0f070, 0xf070a0, 0x70a0f0,
    0xa070f0, 0xf0a070, 0x70f0a0, 0xa0b070, 0xb070a0, 0x70a0b0, 0xa070b0,
    0xb0a070, 0x70b0a0, 0xa05070, 0x5070a0, 0x70a050, 0xa07050, 0x50a070,
    0x7050a0, 0xa0d070, 0xd070a0, 0x70a0d0, 0xa070d0, 0xd0a070, 0x70d0a0,
    0xa07000, 0x70a000, 0x7000a0, 0xa00070, 0x0070a0, 0x00a070, 0xa070a0,
    0x70a0a0, 0xa0a070, 0x70a070, 0xa07070, 0x7070a0, 0x20f070, 0xf07020,
    0x7020f0, 0x2070f0, 0xf02070, 0x70f020, 0x20b070, 0xb07020, 0x7020b0,
    0x2070b0, 0xb02070, 0x70b020, 0x205070, 0x507020, 0x702050, 0x207050,
    0x502070, 0x705020, 0x20d070, 0xd07020, 0x7020d0, 0x2070d0, 0xd02070,
    0x70d020, 0x207000, 0x702000, 0x700020, 0x200070, 0x007020, 0x002070,
    0x207020, 0x702020, 0x202070, 0x702070, 0x207070, 0x707020, 0xf0b070,
    0xb070f0, 0x70f0b0, 0xf070b0, 0xb0f070, 0x70b0f0, 0xf05070, 0x5070f0,
    0x70f050, 0xf07050, 0x50f070, 0x7050f0, 0xf0d070, 0xd070f0, 0x70f0d0,
    0xf070d0, 0xd0f070, 0x70d0f0, 0xf07000, 0x70f000, 0x7000f0, 0xf00070,
    0x0070f0, 0x00f070, 0xf070f0, 0x70f0f0, 0xf0f070, 0x70f070, 0xf07070,
    0x7070f0, 0xb05070, 0x5070b0, 0x70b050, 0xb07050, 0x50b070, 0x7050b0,
    0xb0d070, 0xd070b0, 0x70b0d0, 0xb070d0, 0xd0b070, 0x70d0b0, 0xb07000,
    0x70b000, 0x7000b0, 0xb00070, 0x0070b0, 0x00b070, 0xb070b0, 0x70b0b0,
    0xb0b070, 0x70b070, 0xb07070, 0x7070b0, 0x50d070, 0xd07050, 0x7050d0,
    0x5070d0, 0xd05070, 0x70d050, 0x507000, 0x705000, 0x700050, 0x500070,
    0x007050, 0x005070, 0x507050, 0x705050, 0x505070, 0x705070, 0x507070,
    0x707050, 0xd07000, 0x70d000, 0x7000d0, 0xd00070, 0x0070d0, 0x00d070,
    0xd070d0, 0x70d0d0, 0xd0d070, 0x70d070, 0xd07070, 0x7070d0, 0x707070,
    0x700000, 0x007000, 0x000070, 0x707000, 0x700070, 0x007070, 0xff8030,
    0x8030ff, 0x30ff80, 0xff3080, 0x80ff30, 0x3080ff, 0xffc030, 0xc030ff,
    0x30ffc0, 0xff30c0, 0xc0ff30, 0x30c0ff, 0xff4030, 0x4030ff, 0x30ff40,
    0xff3040, 0x40ff30, 0x3040ff, 0xffe030, 0xe030ff, 0x30ffe0, 0xff30e0,
    0xe0ff30, 0x30e0ff, 0xff6030, 0x6030ff, 0x30ff60, 0xff3060, 0x60ff30,
    0x3060ff, 0xffa030, 0xa030ff, 0x30ffa0, 0xff30a0, 0xa0ff30, 0x30a0ff,
    0xff2030, 0x2030ff, 0x30ff20, 0xff3020, 0x20ff30, 0x3020ff, 0xfff030,
    0xf030ff, 0x30fff0, 0xff30f0, 0xf0ff30, 0x30f0ff, 0xffb030, 0xb030ff,
    0x30ffb0, 0xff30b0, 0xb0ff30, 0x30b0ff, 0xff5030, 0x5030ff, 0x30ff50,
    0xff3050, 0x50ff30, 0x3050ff, 0xffd030, 0xd030ff, 0x30ffd0, 0xff30d0,
    0xd0ff30, 0x30d0ff, 0xff7030, 0x7030ff, 0x30ff70, 0xff3070, 0x70ff30,
    0x3070ff, 0xff3000, 0x30ff00, 0x3000ff, 0xff0030, 0x0030ff, 0x00ff30,
    0xff30ff, 0x30ffff, 0xffff30, 0x30ff30, 0xff3030, 0x3030ff, 0x80c030,
    0xc03080, 0x3080c0, 0x8030c0, 0xc08030, 0x30c080, 0x804030, 0x403080,
    0x308040, 0x803040, 0x408030, 0x304080, 0x80e030, 0xe03080, 0x3080e0,
    0x8030e0, 0xe08030, 0x30e080, 0x806030, 0x603080, 0x308060, 0x803060,
    0x608030, 0x306080, 0x80a030, 0xa03080, 0x3080a0, 0x8030a0, 0xa08030,
    0x30a080, 0x802030, 0x203080, 0x308020, 0x803020, 0x208030, 0x302080,
    0x80f030, 0xf03080, 0x3080f0, 0x8030f0, 0xf08030, 0x30f080, 0x80b030,
    0xb03080, 0x3080b0, 0x8030b0, 0xb08030, 0x30b080, 0x805030, 0x503080,
    0x308050, 0x803050, 0x508030, 0x305080, 0x80d030, 0xd03080, 0x3080d0,
    0x8030d0, 0xd08030, 0x30d080, 0x807030, 0x703080, 0x308070, 0x803070,
    0x708030, 0x307080, 0x803000, 0x308000, 0x300080, 0x800030, 0x003080,
    0x008030, 0x803080, 0x308080, 0x808030, 0x308030, 0x803030, 0x303080,
    0xc04030, 0x4030c0, 0x30c040, 0xc03040, 0x40c030, 0x3040c0, 0xc0e030,
    0xe030c0, 0x30c0e0, 0xc030e0, 0xe0c030, 0x30e0c0, 0xc06030, 0x6030c0,
    0x30c060, 0xc03060, 0x60c030, 0x3060c0, 0xc0a030, 0xa030c0, 0x30c0a0,
    0xc030a0, 0xa0c030, 0x30a0c0, 0xc02030, 0x2030c0, 0x30c020, 0xc03020,
    0x20c030, 0x3020c0, 0xc0f030, 0xf030c0, 0x30c0f0, 0xc030f0, 0xf0c030,
    0x30f0c0, 0xc0b030, 0xb030c0, 0x30c0b0, 0xc030b0, 0xb0c030, 0x30b0c0,
    0xc05030, 0x5030c0, 0x30c050, 0xc03050, 0x50c030, 0x3050c0, 0xc0d030,
    0xd030c0, 0x30c0d0, 0xc030d0, 0xd0c030, 0x30d0c0, 0xc07030, 0x7030c0,
    0x30c070, 0xc03070, 0x70c030, 0x3070c0, 0xc03000, 0x30c000, 0x3000c0,
    0xc00030, 0x0030c0, 0x00c030, 0xc030c0, 0x30c0c0, 0xc0c030, 0x30c030,
    0xc03030, 0x3030c0, 0x40e030, 0xe03040, 0x3040e0, 0x4030e0, 0xe04030,
    0x30e040, 0x406030, 0x603040, 0x304060, 0x403060, 0x604030, 0x306040,
    0x40a030, 0xa03040, 0x3040a0, 0x4030a0, 0xa04030, 0x30a040, 0x402030,
    0x203040, 0x304020, 0x403020, 0x204030, 0x302040, 0x40f030, 0xf03040,
    0x3040f0, 0x4030f0, 0xf04030, 0x30f040, 0x40b030, 0xb03040, 0x3040b0,
    0x4030b0, 0xb04030, 0x30b040, 0x405030, 0x503040, 0x304050, 0x403050,
    0x504030, 0x305040, 0x40d030, 0xd03040, 0x3040d0, 0x4030d0, 0xd04030,
    0x30d040, 0x407030, 0x703040, 0x304070, 0x403070, 0x704030, 0x307040,
    0x403000, 0x304000, 0x300040, 0x400030, 0x003040, 0x004030, 0x403040,
    0x304040, 0x404030, 0x304030, 0x403030, 0x303040, 0xe06030, 0x6030e0,
    0x30e060, 0xe03060, 0x60e030, 0x3060e0, 0xe0a030, 0xa030e0, 0x30e0a0,
    0xe030a0, 0xa0e030, 0x30a0e0, 0xe02030, 0x2030e0, 0x30e020, 0xe03020,
    0x20e030, 0x3020e0, 0xe0f030, 0xf030e0, 0x30e0f0, 0xe030f0, 0xf0e030,
    0x30f0e0, 0xe0b030, 0xb030e0, 0x30e0b0, 0xe030b0, 0xb0e030, 0x30b0e0,
    0xe05030, 0x5030e0, 0x30e050, 0xe03050, 0x50e030, 0x3050e0, 0xe0d030,
    0xd030e0, 0x30e0d0, 0xe030d0, 0xd0e030, 0x30d0e0, 0xe07030, 0x7030e0,
    0x30e070, 0xe03070, 0x70e030, 0x3070e0, 0xe03000, 0x30e000, 0x3000e0,
    0xe00030, 0x0030e0, 0x00e030, 0xe030e0, 0x30e0e0, 0xe0e030, 0x30e030,
    0xe03030, 0x3030e0, 0x60a030, 0xa03060, 0x3060a0, 0x6030a0, 0xa06030,
    0x30a060, 0x602030, 0x203060, 0x306020, 0x603020, 0x206030, 0x302060,
    0x60f030, 0xf03060, 0x3060f0, 0x6030f0, 0xf06030, 0x30f060, 0x60b030,
    0xb03060, 0x3060b0, 0x6030b0, 0xb06030, 0x30b060, 0x605030, 0x503060,
    0x306050, 0x603050, 0x506030, 0x305060, 0x60d030, 0xd03060, 0x3060d0,
    0x6030d0, 0xd06030, 0x30d060, 0x607030, 0x703060, 0x306070, 0x603070,
    0x706030, 0x307060, 0x603000, 0x306000, 0x300060, 0x600030, 0x003060,
    0x006030, 0x603060, 0x306060, 0x606030, 0x306030, 0x603030, 0x303060,
    0xa02030, 0x2030a0, 0x30a020, 0xa03020, 0x20a030, 0x3020a0, 0xa0f030,
    0xf030a0, 0x30a0f0, 0xa030f0, 0xf0a030, 0x30f0a0, 0xa0b030, 0xb030a0,
    0x30a0b0, 0xa030b0, 0xb0a030, 0x30b0a0, 0xa05030, 0x5030a0, 0x30a050,
    0xa03050, 0x50a030, 0x3050a0, 0xa0d030, 0xd030a0, 0x30a0d0, 0xa030d0,
    0xd0a030, 0x30d0a0, 0xa07030, 0x7030a0, 0x30a070, 0xa03070, 0x70a030,
    0x3070a0, 0xa03000, 0x30a000, 0x3000a0, 0xa00030, 0x0030a0, 0x00a030,
    0xa030a0, 0x30a0a0, 0xa0a030, 0x30a030, 0xa03030, 0x3030a0, 0x20f030,
    0xf03020, 0x3020f0, 0x2030f0, 0xf02030, 0x30f020, 0x20b030, 0xb03020,
    0x3020b0, 0x2030b0, 0xb02030, 0x30b020, 0x205030, 0x503020, 0x302050,
    0x203050, 0x502030, 0x305020, 0x20d030, 0xd03020, 0x3020d0, 0x2030d0,
    0xd02030, 0x30d020, 0x207030, 0x703020, 0x302070, 0x203070, 0x702030,
    0x307020, 0x203000, 0x302000, 0x300020, 0x200030, 0x003020, 0x002030,
    0x203020, 0x302020, 0x202030, 0x302030, 0x203030, 0x303020, 0xf0b030,
    0xb030f0, 0x30f0b0, 0xf030b0, 0xb0f030, 0x30b0f0, 0xf05030, 0x5030f0,
    0x30f050, 0xf03050, 0x50f030, 0x3050f0, 0xf0d030, 0xd030f0, 0x30f0d0,
    0xf030d0, 0xd0f030, 0x30d0f0, 0xf07030, 0x7030f0, 0x30f070, 0xf03070,
    0x70f030, 0x3070f0, 0xf03000, 0x30f000, 0x3000f0, 0xf00030, 0x0030f0,
    0x00f030, 0xf030f0, 0x30f0f0, 0xf0f030, 0x30f030, 0xf03030, 0x3030f0,
    0xb05030, 0x5030b0, 0x30b050, 0xb03050, 0x50b030, 0x3050b0, 0xb0d030,
    0xd030b0, 0x30b0d0, 0xb030d0, 0xd0b030, 0x30d0b0, 0xb07030, 0x7030b0,
    0x30b070, 0xb03070, 0x70b030, 0x3070b0, 0xb03000, 0x30b000, 0x3000b0,
    0xb00030, 0x0030b0, 0x00b030, 0xb030b0, 0x30b0b0, 0xb0b030, 0x30b030,
    0xb03030, 0x3030b0, 0x50d030, 0xd03050, 0x3050d0, 0x5030d0, 0xd05030,
    0x30d050, 0x507030, 0x703050, 0x305070, 0x503070, 0x705030, 0x307050,
    0x503000, 0x305000, 0x300050, 0x500030, 0x003050, 0x005030, 0x503050,
    0x305050, 0x505030, 0x305030, 0x503030, 0x303050, 0xd07030, 0x7030d0,
    0x30d070, 0xd03070, 0x70d030, 0x3070d0, 0xd03000, 0x30d000, 0x3000d0,
    0xd00030, 0x0030d0, 0x00d030, 0xd030d0, 0x30d0d0, 0xd0d030, 0x30d030,
    0xd03030, 0x3030d0, 0x703000, 0x307000, 0x300070, 0x700030, 0x003070,
    0x007030, 0x703070, 0x307070, 0x707030, 0x307030, 0x703030, 0x303070,
    0x303030, 0x300000, 0x003000, 0x000030, 0x303000, 0x300030, 0x003030,
    0xff8090, 0x8090ff, 0x90ff80, 0xff9080, 0x80ff90, 0x9080ff, 0xffc090,
    0xc090ff, 0x90ffc0, 0xff90c0, 0xc0ff90, 0x90c0ff, 0xff4090, 0x4090ff,
    0x90ff40, 0xff9040, 0x40ff90, 0x9040ff, 0xffe090, 0xe090ff, 0x90ffe0,
    0xff90e0, 0xe0ff90, 0x90e0ff, 0xff6090, 0x6090ff, 0x90ff60, 0xff9060,
    0x60ff90, 0x9060ff, 0xffa090, 0xa090ff, 0x90ffa0, 0xff90a0, 0xa0ff90,
    0x90a0ff, 0xff2090, 0x2090ff, 0x90ff20, 0xff9020, 0x20ff90, 0x9020ff,
    0xfff090, 0xf090ff, 0x90fff0, 0xff90f0, 0xf0ff90, 0x90f0ff, 0xffb090,
    0xb090ff, 0x90ffb0, 0xff90b0, 0xb0ff90, 0x90b0ff, 0xff5090, 0x5090ff,
    0x90ff50, 0xff9050, 0x50ff90, 0x9050ff, 0xffd090, 0xd090ff, 0x90ffd0,
    0xff90d0, 0xd0ff90, 0x90d0ff, 0xff7090, 0x7090ff, 0x90ff70, 0xff9070,
    0x70ff90, 0x9070ff, 0xff3090, 0x3090ff, 0x90ff30, 0xff9030, 0x30ff90,
    0x9030ff, 0xff9000, 0x90ff00, 0x9000ff, 0xff0090, 0x0090ff, 0x00ff90,
    0xff90ff, 0x90ffff, 0xffff90, 0x90ff90, 0xff9090, 0x9090ff, 0x80c090,
    0xc09080, 0x9080c0, 0x8090c0, 0xc08090, 0x90c080, 0x804090, 0x409080,
    0x908040, 0x809040, 0x408090, 0x904080, 0x80e090, 0xe09080, 0x9080e0,
    0x8090e0, 0xe08090, 0x90e080, 0x806090, 0x609080, 0x908060, 0x809060,
    0x608090, 0x906080, 0x80a090, 0xa09080, 0x9080a0, 0x8090a0, 0xa08090,
    0x90a080, 0x802090, 0x209080, 0x908020, 0x809020, 0x208090, 0x902080,
    0x80f090, 0xf09080, 0x9080f0, 0x8090f0, 0xf08090, 0x90f080, 0x80b090,
    0xb09080, 0x9080b0, 0x8090b0, 0xb08090, 0x90b080, 0x805090, 0x509080,
    0x908050, 0x809050, 0x508090, 0x905080, 0x80d090, 0xd09080, 0x9080d0,
    0x8090d0, 0xd08090, 0x90d080, 0x807090, 0x709080, 0x908070, 0x809070,
    0x708090, 0x907080, 0x803090, 0x309080, 0x908030, 0x809030, 0x308090,
    0x903080, 0x809000, 0x908000, 0x900080, 0x800090, 0x009080, 0x008090,
    0x809080, 0x908080, 0x808090, 0x908090, 0x809090, 0x909080, 0xc04090,
    0x4090c0, 0x90c040, 0xc09040, 0x40c090, 0x9040c0, 0xc0e090, 0xe090c0,
    0x90c0e0, 0xc090e0, 0xe0c090, 0x90e0c0, 0xc06090, 0x6090c0, 0x90c060,
    0xc09060, 0x60c090, 0x9060c0, 0xc0a090, 0xa090c0, 0x90c0a0, 0xc090a0,
    0xa0c090, 0x90a0c0, 0xc02090, 0x2090c0, 0x90c020, 0xc09020, 0x20c090,
    0x9020c0, 0xc0f090, 0xf090c0, 0x90c0f0, 0xc090f0, 0xf0c090, 0x90f0c0,
    0xc0b090, 0xb090c0, 0x90c0b0, 0xc090b0, 0xb0c090, 0x90b0c0, 0xc05090,
    0x5090c0, 0x90c050, 0xc09050, 0x50c090, 0x9050c0, 0xc0d090, 0xd090c0,
    0x90c0d0, 0xc090d0, 0xd0c090, 0x90d0c0, 0xc07090, 0x7090c0, 0x90c070,
    0xc09070, 0x70c090, 0x9070c0, 0xc03090, 0x3090c0, 0x90c030, 0xc09030,
    0x30c090, 0x9030c0, 0xc09000, 0x90c000, 0x9000c0, 0xc00090, 0x0090c0,
    0x00c090, 0xc090c0, 0x90c0c0, 0xc0c090, 0x90c090, 0xc09090, 0x9090c0,
    0x40e090, 0xe09040, 0x9040e0, 0x4090e0, 0xe04090, 0x90e040, 0x406090,
    0x609040, 0x904060, 0x409060, 0x604090, 0x906040, 0x40a090, 0xa09040,
    0x9040a0, 0x4090a0, 0xa04090, 0x90a040, 0x402090, 0x209040, 0x904020,
    0x409020, 0x204090, 0x902040, 0x40f090, 0xf09040, 0x9040f0, 0x4090f0,
    0xf04090, 0x90f040, 0x40b090, 0xb09040, 0x9040b0, 0x4090b0, 0xb04090,
    0x90b040, 0x405090, 0x509040, 0x904050, 0x409050, 0x504090, 0x905040,
    0x40d090, 0xd09040, 0x9040d0, 0x4090d0, 0xd04090, 0x90d040, 0x407090,
    0x709040, 0x904070, 0x409070, 0x704090, 0x907040, 0x403090, 0x309040,
    0x904030, 0x409030, 0x304090, 0x903040, 0x409000, 0x904000, 0x900040,
    0x400090, 0x009040, 0x004090, 0x409040, 0x904040, 0x404090, 0x904090,
    0x409090, 0x909040, 0xe06090, 0x6090e0, 0x90e060, 0xe09060, 0x60e090,
    0x9060e0, 0xe0a090, 0xa090e0, 0x90e0a0, 0xe090a0, 0xa0e090, 0x90a0e0,
    0xe02090, 0x2090e0, 0x90e020, 0xe09020, 0x20e090, 0x9020e0, 0xe0f090,
    0xf090e0, 0x90e0f0, 0xe090f0, 0xf0e090, 0x90f0e0, 0xe0b090, 0xb090e0,
    0x90e0b0, 0xe090b0, 0xb0e090, 0x90b0e0, 0xe05090, 0x5090e0, 0x90e050,
    0xe09050, 0x50e090, 0x9050e0, 0xe0d090, 0xd090e0, 0x90e0d0, 0xe090d0,
    0xd0e090, 0x90d0e0, 0xe07090, 0x7090e0, 0x90e070, 0xe09070, 0x70e090,
    0x9070e0, 0xe03090, 0x3090e0, 0x90e030, 0xe09030, 0x30e090, 0x9030e0,
    0xe09000, 0x90e000, 0x9000e0, 0xe00090, 0x0090e0, 0x00e090, 0xe090e0,
    0x90e0e0, 0xe0e090, 0x90e090, 0xe09090, 0x9090e0, 0x60a090, 0xa09060,
    0x9060a0, 0x6090a0, 0xa06090, 0x90a060, 0x602090, 0x209060, 0x906020,
    0x609020, 0x206090, 0x902060, 0x60f090, 0xf09060, 0x9060f0, 0x6090f0,
    0xf06090, 0x90f060, 0x60b090, 0xb09060, 0x9060b0, 0x6090b0, 0xb06090,
    0x90b060, 0x605090, 0x509060, 0x906050, 0x609050, 0x506090, 0x905060,
    0x60d090, 0xd09060, 0x9060d0, 0x6090d0, 0xd06090, 0x90d060, 0x607090,
    0x709060, 0x906070, 0x609070, 0x706090, 0x907060, 0x603090, 0x309060,
    0x906030, 0x609030, 0x306090, 0x903060, 0x609000, 0x906000, 0x900060,
    0x600090, 0x009060, 0x006090, 0x609060, 0x906060, 0x606090, 0x906090,
    0x609090, 0x909060, 0xa02090, 0x2090a0, 0x90a020, 0xa09020, 0x20a090,
    0x9020a0, 0xa0f090, 0xf090a0, 0x90a0f0, 0xa090f0, 0xf0a090, 0x90f0a0,
    0xa0b090, 0xb090a0, 0x90a0b0, 0xa090b0, 0xb0a090, 0x90b0a0, 0xa05090,
    0x5090a0, 0x90a050, 0xa09050, 0x50a090, 0x9050a0, 0xa0d090, 0xd090a0,
    0x90a0d0, 0xa090d0, 0xd0a090, 0x90d0a0, 0xa07090, 0x7090a0, 0x90a070,
    0xa09070, 0x70a090, 0x9070a0, 0xa03090, 0x3090a0, 0x90a030, 0xa09030,
    0x30a090, 0x9030a0, 0xa09000, 0x90a000, 0x9000a0, 0xa00090, 0x0090a0,
    0x00a090, 0xa090a0, 0x90a0a0, 0xa0a090, 0x90a090, 0xa09090, 0x9090a0,
    0x20f090, 0xf09020, 0x9020f0, 0x2090f0, 0xf02090, 0x90f020, 0x20b090,
    0xb09020, 0x9020b0, 0x2090b0, 0xb02090, 0x90b020, 0x205090, 0x509020,
    0x902050, 0x209050, 0x502090, 0x905020, 0x20d090, 0xd09020, 0x9020d0,
    0x2090d0, 0xd02090, 0x90d020, 0x207090, 0x709020, 0x902070, 0x209070,
    0x702090, 0x907020, 0x203090, 0x309020, 0x902030, 0x209030, 0x302090,
    0x903020, 0x209000, 0x902000, 0x900020, 0x200090, 0x009020, 0x002090,
    0x209020, 0x902020, 0x202090, 0x902090, 0x209090, 0x909020, 0xf0b090,
    0xb090f0, 0x90f0b0, 0xf090b0, 0xb0f090, 0x90b0f0, 0xf05090, 0x5090f0,
    0x90f050, 0xf09050, 0x50f090, 0x9050f0, 0xf0d090, 0xd090f0, 0x90f0d0,
    0xf090d0, 0xd0f090, 0x90d0f0, 0xf07090, 0x7090f0, 0x90f070, 0xf09070,
    0x70f090, 0x9070f0, 0xf03090, 0x3090f0, 0x90f030, 0xf09030, 0x30f090,
    0x9030f0, 0xf09000, 0x90f000, 0x9000f0, 0xf00090, 0x0090f0, 0x00f090,
    0xf090f0, 0x90f0f0, 0xf0f090, 0x90f090, 0xf09090, 0x9090f0, 0xb05090,
    0x5090b0, 0x90b050, 0xb09050, 0x50b090, 0x9050b0, 0xb0d090, 0xd090b0,
    0x90b0d0, 0xb090d0, 0xd0b090, 0x90d0b0, 0xb07090, 0x7090b0, 0x90b070,
    0xb09070, 0x70b090, 0x9070b0, 0xb03090, 0x3090b0, 0x90b030, 0xb09030,
    0x30b090, 0x9030b0, 0xb09000, 0x90b000, 0x9000b0, 0xb00090, 0x0090b0,
    0x00b090, 0xb090b0, 0x90b0b0, 0xb0b090, 0x90b090, 0xb09090, 0x9090b0,
    0x50d090, 0xd09050, 0x9050d0, 0x5090d0, 0xd05090, 0x90d050, 0x507090,
    0x709050, 0x905070, 0x509070, 0x705090, 0x907050, 0x503090, 0x309050,
    0x905030, 0x509030, 0x305090, 0x903050, 0x509000, 0x905000, 0x900050,
    0x500090, 0x009050, 0x005090, 0x509050, 0x905050, 0x505090, 0x905090,
    0x509090, 0x909050, 0xd07090, 0x7090d0, 0x90d070, 0xd09070, 0x70d090,
    0x9070d0, 0xd03090, 0x3090d0, 0x90d030, 0xd09030, 0x30d090, 0x9030d0,
    0xd09000, 0x90d000, 0x9000d0, 0xd00090, 0x0090d0, 0x00d090, 0xd090d0,
    0x90d0d0, 0xd0d090, 0x90d090, 0xd09090, 0x9090d0, 0x703090, 0x309070,
    0x907030, 0x709030, 0x307090, 0x903070, 0x709000, 0x907000, 0x900070,
    0x700090, 0x009070, 0x007090, 0x709070, 0x907070, 0x707090, 0x907090,
    0x709090, 0x909070, 0x309000, 0x903000, 0x900030, 0x300090, 0x009030,
    0x003090, 0x309030, 0x903030, 0x303090, 0x903090, 0x309090, 0x909030,
    0x909090, 0x900000, 0x009000, 0x000090, 0x909000, 0x900090, 0x009090,
    0xff8010
};

static unsigned char const pickMapComponent[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
    0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
    0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
    0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
    0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
    0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0,
    0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static unsigned char const pickMapComponent444[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x00, 0x00, 0x60, 0x60, 0x60,
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x60, 0x60,
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
    0x00, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
    0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
    0x00, 0x00, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
    0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
    0xb0, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

#define MAX_PICK_COLORS 4096

// Get the value at "index" to use for color picking.  Returns a zero
// QRgb when the table has been exhausted.
QRgb qt_qgl_pick_color(int index)
{
    if (index >= 0 && index < MAX_PICK_COLORS)
        return QRgb(pickColors[index] | 0xff000000);
    else
        return 0;
}

// Normalize a color that was picked out of a screen color buffer
// so that it is a better match for something that was generated
// by qt_qgl_pick_color().  Rounding discrepancies in the
// low bits due to floating-point conversions are factored out.
QRgb qt_qgl_normalize_pick_color(QRgb color, bool is444)
{
    int red, green, blue;
    if (!is444) {
        // RGB565, RGB555, and RGB888 screens (alpha is ignored).
        red = pickMapComponent[qRed(color)];
        green = pickMapComponent[qGreen(color)];
        blue = pickMapComponent[qBlue(color)];
    } else {
        // RGB444 screens need a little more care when normalizing.
        red = pickMapComponent444[qRed(color)];
        green = pickMapComponent444[qGreen(color)];
        blue = pickMapComponent444[qBlue(color)];
    }
    return qRgb(red, green, blue);
}

QT_END_NAMESPACE

#else // QGL_GENERATOR_PROGRAM

#include <stdio.h>

static unsigned char singlePatterns[] = {
    1, 1, 1,

    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
    1, 1, 0,
    1, 0, 1,
    0, 1, 1
};
#define NUM_SINGLE_PATTERNS 7

static unsigned char doublePatterns[] = {
    1, 2, 0,
    2, 1, 0,
    2, 0, 1,
    1, 0, 2,
    0, 2, 1,
    0, 1, 2,

    1, 2, 1,
    2, 1, 1,
    1, 1, 2,
    2, 1, 2,
    1, 2, 2,
    2, 2, 1
};
#define NUM_DOUBLE_PATTERNS 12

static unsigned char triplePatterns[] = {
    1, 2, 3,
    2, 3, 1,
    3, 1, 2,
    1, 3, 2,
    2, 1, 3,
    3, 2, 1
};
#define NUM_TRIPLE_PATTERNS 6

static unsigned char values[] = {
    0x00,
    0xff, 0x80, 0xc0, 0x40, 0xe0, 0x60, 0xa0, 0x20,
    0xf0, 0xb0, 0x50, 0xd0, 0x70, 0x30, 0x90, 0x10
};
#define NUM_VALUES 16
#define NUM_VALUES_444 10

#define MAX_GENERATE 4096

static unsigned char used[17][17][17];
static int generated = 0;

static void genPattern(int red, int green, int blue)
{
    ++red;
    ++green;
    ++blue;
    if (used[red][green][blue] || generated >= MAX_GENERATE)
    return;
    used[red][green][blue] = 1;
    if ((generated % 7) == 0)
    printf("\n    ");
    printf("0x%02x%02x%02x", values[red], values[green], values[blue]);
    ++generated;
    if (generated < MAX_GENERATE && (generated % 7) != 0)
    printf(", ");
    else if (generated < MAX_GENERATE)
    printf(",");
}

static void genSinglePatterns(int value)
{
    int index, red, green, blue;
    for (index = 0; index < NUM_SINGLE_PATTERNS; ++index) {
    if (singlePatterns[index * 3] == 0)
        red = -1;
    else
        red = value;
    if (singlePatterns[index * 3 + 1] == 0)
        green = -1;
    else
        green = value;
    if (singlePatterns[index * 3 + 2] == 0)
        blue = -1;
    else
        blue = value;
    genPattern(red, green, blue);
    }
}

static void genDoublePatterns(int value1, int value2)
{
    int index, red, green, blue;
    for (index = 0; index < NUM_DOUBLE_PATTERNS; ++index) {
    if (doublePatterns[index * 3] == 0)
        red = -1;
    else if (doublePatterns[index * 3] == 1)
        red = value1;
    else
        red = value2;
    if (doublePatterns[index * 3 + 1] == 0)
        green = -1;
    else if (doublePatterns[index * 3 + 1] == 1)
        green = value1;
    else
        green = value2;
    if (doublePatterns[index * 3 + 2] == 0)
        blue = -1;
    else if (doublePatterns[index * 3 + 2] == 1)
        blue = value1;
    else
        blue = value2;
    genPattern(red, green, blue);
    }
}

static void genTriplePatterns(int value1, int value2, int value3)
{
    int index, red, green, blue;
    for (index = 0; index < NUM_TRIPLE_PATTERNS; ++index) {
    if (triplePatterns[index * 3] == 0)
        red = -1;
    else if (triplePatterns[index * 3] == 1)
        red = value1;
    else if (triplePatterns[index * 3] == 2)
        red = value2;
    else
        red = value3;
    if (triplePatterns[index * 3 + 1] == 0)
        green = -1;
    else if (triplePatterns[index * 3 + 1] == 1)
        green = value1;
    else if (triplePatterns[index * 3 + 1] == 2)
        green = value2;
    else
        green = value3;
    if (triplePatterns[index * 3 + 2] == 0)
        blue = -1;
    else if (triplePatterns[index * 3 + 2] == 1)
        blue = value1;
    else if (triplePatterns[index * 3 + 2] == 2)
        blue = value2;
    else
        blue = value3;
    genPattern(red, green, blue);
    }
}

static void genPatternRange(int limit)
{
    // This will generate 4912 unique color values which are
    // reasonably well-spaced in the RGB color cube.
    int first, second, third;
    for (first = 0; first < limit; ++first) {
    genSinglePatterns(first);
    for (second = first + 1; second < limit; ++second) {
        genDoublePatterns(first, second);
        for (third = second + 1; third < limit; ++third) {
        genTriplePatterns(first, second, third);
        }
    }
    }
}

static void generateComponentMap(void)
{
    int map[256];
    int index, value, index2;

    for (index = 0; index < 256; ++index)
    map[index] = 0;

    for (index = 0; index < NUM_VALUES; ++index) {
    value = values[index + 1];
    for (index2 = value - 8; index2 < (value + 8); ++index2) {
        if (index2 >= 0 && index2 < 256)
        map[index2] = value;
    }
    }

    for (index = 0; index < 256; ++index) {
    if ((index % 8) == 0)
        printf("    ");
    printf("0x%02x", map[index]);
    if (index < 255)
        printf(",");
    if ((index % 8) == 7)
        printf("\n");
    else if (index < 255)
        printf(" ");
    }

    // Validate the reversibility of RGB565 and RGB555 mappings.
    for (index = 0; index < 17; ++index) {
    // Integer truncation test - 5-bit for red and blue (and green RGB555).
    value = values[index] * 31 / 255;
    index2 = value * 255 / 31;
    if (values[index] != map[index2]) {
        qWarning("RGB565 (i5) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Integer truncation test - 6-bit for green.
    value = values[index] * 63 / 255;
    index2 = value * 255 / 63;
    if (values[index] != map[index2]) {
        qWarning("RGB565 (i6) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Floating point rounding test - 5-bit for red and blue.
    value = (int)((values[index] * 31.0 / 255.0) + 0.5);
    index2 = (int)((value * 255.0 / 31.0) + 0.5);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (f5) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Floating point rounding test - 6-bit for green.
    value = (int)((values[index] * 63.0 / 255.0) + 0.5);
    index2 = (int)((value * 255.0 / 63.0) + 0.5);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (f6) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Test 5-bit to 8-bit conversion using doubling (ABCDE -> ABCDEABC).
    value = values[index] * 31 / 255;
    index2 = (value << 3) | (value >> 2);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (di5) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }
    value = (int)((values[index] * 31.0 / 255.0) + 0.5);
    index2 = (value << 3) | (value >> 2);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (df5) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Test 6-bit to 8-bit conversion using doubling (ABCDEF -> ABCDEFAB).
    value = values[index] * 63 / 255;
    index2 = (value << 2) | (value >> 4);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (di6) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }
    value = (int)((values[index] * 63.0 / 255.0) + 0.5);
    index2 = (value << 2) | (value >> 4);
    if (values[index] != map[index2]) {
        qWarning("RGB565 (df6) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }
    }
}

static void generateComponentMap444(void)
{
    int map[256];
    int index, value, index2;

    for (index = 0; index < 256; ++index)
    map[index] = 0;

    // Populate mappings for integer conversion with truncation.
    for (index = 0; index < NUM_VALUES_444; ++index) {
    value = values[index + 1] * 15 / 255;
    value = value * 255 / 15;
    if (value > 255)
        value = 255;
    else if (value < 0)
        value = 0;
    for (index2 = value - 8; index2 < (value + 7); ++index2) {
        if (index2 >= 0 && index2 < 256)
        map[index2] = values[index + 1];
    }
    }

    // Add some extra mappings for floating-point conversion with rounding.
    for (index = 0; index < NUM_VALUES_444; ++index) {
    value = (int)((values[index + 1] * 15.0 / 255.0) + 0.5);
    value = (int)((value * 255.0 / 15.0) + 0.5);
    if (value > 255)
        value = 255;
    else if (value < 0)
        value = 0;
    for (index2 = value - 8; index2 < (value + 7); ++index2) {
        if (index2 >= 0 && index2 < 256 && map[index2] == 0)
        map[index2] = values[index + 1];
    }
    }

    for (index = 0; index < 256; ++index) {
    if ((index % 8) == 0)
        printf("    ");
    printf("0x%02x", map[index]);
    if (index < 255)
        printf(",");
    if ((index % 8) == 7)
        printf("\n");
    else if (index < 255)
        printf(" ");
    }

    // Validate the reversibility of RGB444 mappings.
    for (index = 0; index <= NUM_VALUES_444; ++index) {
    // Integer truncation test.
    value = values[index] * 15 / 255;
    index2 = value * 255 / 15;
    if (values[index] != map[index2]) {
        qWarning("RGB444 (i) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Floating point rounding test.
    value = (int)((values[index] * 15.0 / 255.0) + 0.5);
    index2 = (int)((value * 255.0 / 15.0) + 0.5);
    if (values[index] != map[index2]) {
        qWarning("RGB444 (f) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }

    // Test 4-bit to 8-bit conversion using doubling (ABCD -> ABCDABCD).
    value = values[index] * 15 / 255;
    index2 = value | (value << 4);
    if (values[index] != map[index2]) {
        qWarning("RGB444 (di) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }
    value = (int)((values[index] * 15.0 / 255.0) + 0.5);
    index2 = value | (value << 4);
    if (values[index] != map[index2]) {
        qWarning("RGB444 (df) failure: 0x%02X -> 0x%02X -> 0x%02X\n",
            values[index], index2, map[index2]);
    }
    }
}

int main(int argc, char *argv[])
{
    int limit;

    // Run the generator multiple times using more and more of
    // the elements in the "values" table, and limit to a maximum
    // of 1024 colors.
    //
    // This will sort the output so that colors that involve elements
    // early in the table will be generated first.  All combinations
    // of early elements are exhausted before moving onto later values.
    //
    // The result of this sorting should be to maximize the spacing
    // between colors that appear early in the generated output.
    // This should produce better results for color picking on
    // low-end devices with RGB565, RGB555, and RGB444 displays.
    printf("static int const pickColors[%d] = {", MAX_GENERATE);
    for (limit = 1; limit <= NUM_VALUES; ++limit)
    genPatternRange(limit);
    printf("\n};\n\n");

    // Generate a component mapping table for mapping 8-bit RGB
    // components to the nearest normalized value.
    printf("static unsigned char const pickMapComponent[256] = {\n");
    generateComponentMap();
    printf("};\n\n");

    // Generate a separate mapping table for RGB444, which needs a
    // little more care to deal with truncation errors.
    printf("static unsigned char const pickMapComponent444[256] = {\n");
    generateComponentMap444();
    printf("};\n\n");

    printf("#define MAX_PICK_COLORS %d\n\n", MAX_GENERATE);

    return 0;
}

#endif // QGL_GENERATOR_PROGRAM
