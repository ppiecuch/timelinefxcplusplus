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

#include "qgleffect_p.h"
#include "qglext_p.h"

#include <QOpenGLShaderProgram>
#include <QFile>


QT_BEGIN_NAMESPACE

/*!
    \class QGLAbstractEffect
    \since 4.8
    \brief The QGLAbstractEffect class provides a standard interface for rendering surface material effects with GL.
    \ingroup qt3d
    \ingroup qt3d::painting

    \section1 Vertex attributes

    Vertex attributes for the effect are specified using
    QGLPainter::setVertexAttribute() and QGLPainter::setVertexBundle(),
    and may be independent of the effect itself.  Those functions
    will bind standard attributes to specific indexes within the
    GL state.  For example, the QGL::Position will be bound
    to index 0, QGL::TextureCoord0 will be bound to index 3, etc.

    Effect subclasses that use vertex shaders should bind their attributes
    to these indexes using QGLShaderProgram::bindAttributeLocation()
    just before the program is linked.  For example:

    \code
    QGLShaderProgram *program = new QGLShaderProgram();
    program->addShaderFromSourceCode(QGLShader::Vertex, vshaderSource);
    program->addShaderFromSourceCode(QGLShader::Fragment, fshaderSource);
    program->bindAttributeLocation("vertex", QGL::Position);
    program->bindAttributeLocation("normal", QGL::Normal);
    program->bindAttributeLocation("texcoord", QGL::TextureCoord0);
    program->link();
    \endcode

    The QGLShaderProgramEffect class can assist with writing
    shader-based effects.  It will automatically bind special
    variable names, such as \c{qt_Vertex}, \c{qt_MultiTexCoord0}, etc,
    to the standard indexes.  This alleviates the need for the
    application to bind the names itself.
*/

/*!
    Constructs a new effect object.
*/
QGLAbstractEffect::QGLAbstractEffect()
{
}

/*!
    Destroys this effect object.
*/
QGLAbstractEffect::~QGLAbstractEffect()
{
}

/*!
    Returns true if this effect supports object picking; false otherwise.
    The default implementation returns false, which causes QGLPainter
    to use the effect associated with QGL::FlatColor to perform
    object picking.

    Effects that support object picking render fragments with
    QGLPainter::pickColor() when QGLPainter::isPicking() returns true.
    By default, only the effect associated with QGL::FlatColor does this,
    rendering the entire fragment with the flat pick color.

    In some cases, rendering the entire fragment with the pick color
    may not be appropriate.  An alpha-blended icon texture that is
    drawn to the screen as a quad may have an irregular shape smaller
    than the quad.  For picking, the application may not want the
    entire quad to be "active" for object selection as it would appear
    to allow the user to click off the icon to select it.

    This situation can be handled by implementing an icon rendering
    effect that draws the icon normally when QGLPainter::isPicking()
    is false, and draws a mask texture defining the outline of the icon
    with QGLPainter::pickColor() when QGLPainter::isPicking() is true.

    \sa QGLPainter::setPicking()
*/
bool QGLAbstractEffect::supportsPicking() const
{
    return false;
}

/*!
    \fn void QGLAbstractEffect::setActive(QGLPainter *painter, bool flag)

    Activates or deactivates this effect on \a painter,
    according to \a flag, on the current GL context by selecting
    shader programs, setting lighting and material parameters, etc.

    \sa update()
*/

/*!
    \fn void QGLAbstractEffect::update(QGLPainter *painter, QGLPainter::Updates updates)

    Updates the current GL context with information from \a painter
    just prior to the drawing of triangles, quads, etc.

    The \a updates parameter specifies the properties on \a painter
    that have changed since the last call to update() or setActive().

    \sa setActive()
*/


/*!
    \class QGLFlatColorEffect
    \since 4.8
    \brief The QGLFlatColorEffect class provides a standard effect that draws fragments with a flat unlit color.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

/*!
    \class QGLPerVertexColorEffect
    \since 4.8
    \brief The QGLPerVertexColorEffect class provides a standard effect that draws fragments with a per-vertex unlit color.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

class QGLFlatColorEffectPrivate
{
public:
    QGLFlatColorEffectPrivate()
        : program(0)
        , matrixUniform(-1)
        , colorUniform(-1)
        , isFixedFunction(false)
    {
    }

    QOpenGLShaderProgram *program;
    int matrixUniform;
    int colorUniform;
    bool isFixedFunction;
};

/*!
    Constructs a new flat color effect.
*/
QGLFlatColorEffect::QGLFlatColorEffect()
    : d_ptr(new QGLFlatColorEffectPrivate)
{
}

/*!
    Destroys this flat color effect.
*/
QGLFlatColorEffect::~QGLFlatColorEffect()
{
}

/*!
    \reimp
*/
bool QGLFlatColorEffect::supportsPicking() const
{
    return true;
}

/*!
    \reimp
*/
void QGLFlatColorEffect::setActive(QGLPainter *painter, bool flag)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    Q_UNUSED(painter);
    if (flag)
        glEnableClientState(GL_VERTEX_ARRAY);
    else
        glDisableClientState(GL_VERTEX_ARRAY);
#else
    Q_UNUSED(painter);
    Q_D(QGLFlatColorEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (painter->isFixedFunction()) {
        d->isFixedFunction = true;
        if (flag)
            glEnableClientState(GL_VERTEX_ARRAY);
        else
            glDisableClientState(GL_VERTEX_ARRAY);
        return;
    }
#endif
    static char const flatColorVertexShader[] =
        "attribute highp vec4 vertex;\n"
        "uniform highp mat4 matrix;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = matrix * vertex;\n"
        "}\n";

    static char const flatColorFragmentShader[] =
        "uniform mediump vec4 color;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = color;\n"
        "}\n";

    QOpenGLShaderProgram *program =
        painter->cachedProgram(QLatin1String("qt.color.flat"));
    d->program = program;
    if (!program) {
        if (!flag)
            return;
        program = new QOpenGLShaderProgram;
        program->addShaderFromSourceCode(QOpenGLShader::Vertex, flatColorVertexShader);
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, flatColorFragmentShader);
        program->bindAttributeLocation("vertex", QGL::Position);
        if (!program->link()) {
            qWarning("QGLFlatColorEffect::setActive(): could not link shader program");
            delete program;
            return;
        }
        painter->setCachedProgram(QLatin1String("qt.color.flat"), program);
        d->program = program;
        d->colorUniform = program->uniformLocation("color");
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->enableAttributeArray(QGL::Position);
    } else if (flag) {
        d->colorUniform = program->uniformLocation("color");
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->enableAttributeArray(QGL::Position);
    } else {
        program->disableAttributeArray(QGL::Position);
        program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLFlatColorEffect::update
        (QGLPainter *painter, QGLPainter::Updates updates)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    painter->updateFixedFunction
        (updates & (QGLPainter::UpdateColor |
                    QGLPainter::UpdateMatrices));
#else
    Q_D(QGLFlatColorEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->isFixedFunction) {
        painter->updateFixedFunction
            (updates & (QGLPainter::UpdateColor |
                        QGLPainter::UpdateMatrices));
        return;
    }
#endif
    if (!d->program)
        return;
    if ((updates & QGLPainter::UpdateColor) != 0) {
        if (painter->isPicking())
            d->program->setUniformValue(d->colorUniform, painter->pickColor());
        else
            d->program->setUniformValue(d->colorUniform, painter->color());
    }
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        QMatrix4x4 proj = painter->projectionMatrix();
        QMatrix4x4 mv = painter->modelViewMatrix();
        d->program->setUniformValue(d->matrixUniform, proj * mv);
    }
#endif
}

class QGLPerVertexColorEffectPrivate
{
public:
    QGLPerVertexColorEffectPrivate()
        : program(0)
        , matrixUniform(-1)
        , isFixedFunction(false)
    {
    }

    QOpenGLShaderProgram *program;
    int matrixUniform;
    bool isFixedFunction;
};

/*!
    Constructs a new per-vertex color effect.
*/
QGLPerVertexColorEffect::QGLPerVertexColorEffect()
    : d_ptr(new QGLPerVertexColorEffectPrivate)
{
}

/*!
    Destroys this per-vertex color effect.
*/
QGLPerVertexColorEffect::~QGLPerVertexColorEffect()
{
}

/*!
    \reimp
*/
void QGLPerVertexColorEffect::setActive(QGLPainter *painter, bool flag)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    Q_UNUSED(painter);
    if (flag) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
    }
#else
    Q_UNUSED(painter);
    Q_D(QGLPerVertexColorEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (painter->isFixedFunction()) {
        d->isFixedFunction = true;
        if (flag) {
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_COLOR_ARRAY);
        } else {
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
        }
        return;
    }
#endif
    static char const pvColorVertexShader[] =
        "attribute highp vec4 vertex;\n"
        "attribute mediump vec4 color;\n"
        "uniform highp mat4 matrix;\n"
        "varying mediump vec4 qColor;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = matrix * vertex;\n"
        "    qColor = color;\n"
        "}\n";

    static char const pvColorFragmentShader[] =
        "varying mediump vec4 qColor;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = qColor;\n"
        "}\n";

    QOpenGLShaderProgram *program =
        painter->cachedProgram(QLatin1String("qt.color.pervertex"));
    d->program = program;
    if (!program) {
        if (!flag)
            return;
        program = new QOpenGLShaderProgram();
        program->addShaderFromSourceCode(QOpenGLShader::Vertex, pvColorVertexShader);
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, pvColorFragmentShader);
        program->bindAttributeLocation("vertex", QGL::Position);
        program->bindAttributeLocation("color", QGL::Color);
        if (!program->link()) {
            qWarning("QGLPerVertexColorEffect::setActive(): could not link shader program");
            delete program;
            program = 0;
            return;
        }
        painter->setCachedProgram(QLatin1String("qt.color.pervertex"), program);
        d->program = program;
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::Color);
    } else if (flag) {
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::Color);
    } else {
        program->disableAttributeArray(QGL::Position);
        program->disableAttributeArray(QGL::Color);
        program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLPerVertexColorEffect::update
        (QGLPainter *painter, QGLPainter::Updates updates)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    painter->updateFixedFunction(updates & QGLPainter::UpdateMatrices);
#else
    Q_UNUSED(painter);
    Q_D(QGLPerVertexColorEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->isFixedFunction) {
        painter->updateFixedFunction(updates & QGLPainter::UpdateMatrices);
        return;
    }
#endif
    if (!d->program)
        return;
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        d->program->setUniformValue
            (d->matrixUniform, painter->combinedMatrix());
    }
#endif
}


/*!
    \class QGLFlatTextureEffect
    \since 4.8
    \brief The QGLFlatTextureEffect class provides a standard effect that draws fragments with a flat unlit texture.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

/*!
    \class QGLFlatDecalTextureEffect
    \since 4.8
    \brief The QGLFlatDecalTextureEffect class provides a standard effect that decals fragments with a flat unlit texture.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

class QGLFlatTextureEffectPrivate
{
public:
    QGLFlatTextureEffectPrivate()
        : program(0)
        , matrixUniform(-1)
        , isFixedFunction(false)
    {
    }

    QOpenGLShaderProgram *program;
    int matrixUniform;
    bool isFixedFunction;
};

/*!
    Constructs a new flat texture effect.
*/
QGLFlatTextureEffect::QGLFlatTextureEffect()
    : d_ptr(new QGLFlatTextureEffectPrivate)
{
}

/*!
    Destroys this flat texture effect.
*/
QGLFlatTextureEffect::~QGLFlatTextureEffect()
{
}

#if !defined(QGL_FIXED_FUNCTION_ONLY)

static char const flatTexVertexShader[] =
    "attribute highp vec4 vertex;\n"
    "attribute highp vec4 texcoord;\n"
    "uniform highp mat4 matrix;\n"
    "varying highp vec4 qt_TexCoord0;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = matrix * vertex;\n"
    "    qt_TexCoord0 = texcoord;\n"
    "}\n";

static char const flatTexFragmentShader[] =
    "uniform sampler2D tex;\n"
    "varying highp vec4 qt_TexCoord0;\n"
    "void main(void)\n"
    "{\n"
    "    gl_FragColor = texture2D(tex, qt_TexCoord0.st);\n"
    "}\n";

static char const flatDecalFragmentShader[] =
    "uniform sampler2D tex;\n"
    "uniform mediump vec4 color;\n"
    "varying highp vec4 qt_TexCoord0;\n"
    "void main(void)\n"
    "{\n"
    "    mediump vec4 col = texture2D(tex, qt_TexCoord0.st);\n"
    "    gl_FragColor = vec4(clamp(color.rgb * (1.0 - col.a) + col.rgb, 0.0, 1.0), color.a);\n"
    "}\n";

#endif

/*!
    \reimp
*/
void QGLFlatTextureEffect::setActive(QGLPainter *painter, bool flag)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    Q_UNUSED(painter);
    if (flag) {
        glEnableClientState(GL_VERTEX_ARRAY);
        qt_gl_ClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
        qt_gl_ClientActiveTexture(GL_TEXTURE0);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
    }
#else
    Q_UNUSED(painter);
    Q_D(QGLFlatTextureEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (painter->isFixedFunction()) {
        d->isFixedFunction = true;
        if (flag) {
            glEnableClientState(GL_VERTEX_ARRAY);
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glEnable(GL_TEXTURE_2D);
        } else {
            glDisableClientState(GL_VERTEX_ARRAY);
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisable(GL_TEXTURE_2D);
        }
        return;
    }
#endif
    QOpenGLShaderProgram *program =
        painter->cachedProgram(QLatin1String("qt.texture.flat.replace"));
    d->program = program;
    if (!program) {
        if (!flag)
            return;
        program = new QOpenGLShaderProgram();
        program->addShaderFromSourceCode(QOpenGLShader::Vertex, flatTexVertexShader);
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, flatTexFragmentShader);
        program->bindAttributeLocation("vertex", QGL::Position);
        program->bindAttributeLocation("texcoord", QGL::TextureCoord0);
        if (!program->link()) {
            qWarning("QGLFlatTextureEffect::setActive(): could not link shader program");
            delete program;
            program = 0;
            return;
        }
        painter->setCachedProgram
            (QLatin1String("qt.texture.flat.replace"), program);
        d->program = program;
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->setUniformValue("tex", 0);
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::TextureCoord0);
    } else if (flag) {
        d->matrixUniform = program->uniformLocation("matrix");
        program->bind();
        program->setUniformValue("tex", 0);
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::TextureCoord0);
    } else {
        program->disableAttributeArray(QGL::Position);
        program->disableAttributeArray(QGL::TextureCoord0);
        program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLFlatTextureEffect::update
        (QGLPainter *painter, QGLPainter::Updates updates)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    painter->updateFixedFunction(updates & QGLPainter::UpdateMatrices);
#else
    Q_D(QGLFlatTextureEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->isFixedFunction) {
        painter->updateFixedFunction(updates & QGLPainter::UpdateMatrices);
        return;
    }
#endif
    if (!d->program)
        return;
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        d->program->setUniformValue
            (d->matrixUniform, painter->combinedMatrix());
    }
#endif
}

class QGLFlatDecalTextureEffectPrivate
{
public:
    QGLFlatDecalTextureEffectPrivate()
        : program(0)
        , matrixUniform(-1)
        , colorUniform(-1)
        , isFixedFunction(false)
    {
    }

    QOpenGLShaderProgram *program;
    int matrixUniform;
    int colorUniform;
    bool isFixedFunction;
};

/*!
    Constructs a new flat decal texture effect.
*/
QGLFlatDecalTextureEffect::QGLFlatDecalTextureEffect()
    : d_ptr(new QGLFlatDecalTextureEffectPrivate)
{
}

/*!
    Destroys this flat decal texture effect.
*/
QGLFlatDecalTextureEffect::~QGLFlatDecalTextureEffect()
{
}

/*!
    \reimp
*/
void QGLFlatDecalTextureEffect::setActive(QGLPainter *painter, bool flag)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    Q_UNUSED(painter);
    if (flag) {
        glEnableClientState(GL_VERTEX_ARRAY);
        qt_gl_ClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
        qt_gl_ClientActiveTexture(GL_TEXTURE0);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
    }
#else
    Q_UNUSED(painter);
    Q_D(QGLFlatDecalTextureEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (painter->isFixedFunction()) {
        d->isFixedFunction = true;
        if (flag) {
            glEnableClientState(GL_VERTEX_ARRAY);
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
            glEnable(GL_TEXTURE_2D);
        } else {
            glDisableClientState(GL_VERTEX_ARRAY);
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisable(GL_TEXTURE_2D);
        }
    }
#endif
    QOpenGLShaderProgram *program =
        painter->cachedProgram(QLatin1String("qt.texture.flat.decal"));
    d->program = program;
    if (!program) {
        if (!flag)
            return;
        program = new QOpenGLShaderProgram();
        program->addShaderFromSourceCode(QOpenGLShader::Vertex, flatTexVertexShader);
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, flatDecalFragmentShader);
        program->bindAttributeLocation("vertex", QGL::Position);
        program->bindAttributeLocation("texcoord", QGL::TextureCoord0);
        if (!program->link()) {
            qWarning("QGLFlatDecalTextureEffect::setActive(): could not link shader program");
            delete program;
            program = 0;
            return;
        }
        painter->setCachedProgram
            (QLatin1String("qt.texture.flat.decal"), program);
        d->program = program;
        d->matrixUniform = program->uniformLocation("matrix");
        d->colorUniform = program->uniformLocation("color");
        program->bind();
        program->setUniformValue("tex", 0);
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::TextureCoord0);
    } else if (flag) {
        d->matrixUniform = program->uniformLocation("matrix");
        d->colorUniform = program->uniformLocation("color");
        program->bind();
        program->setUniformValue("tex", 0);
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::TextureCoord0);
    } else {
        program->disableAttributeArray(QGL::Position);
        program->disableAttributeArray(QGL::TextureCoord0);
        program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLFlatDecalTextureEffect::update
    (QGLPainter *painter, QGLPainter::Updates updates)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    painter->updateFixedFunction
        (updates & (QGLPainter::UpdateColor |
                    QGLPainter::UpdateMatrices));
#else
    Q_D(QGLFlatDecalTextureEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->isFixedFunction) {
        painter->updateFixedFunction
            (updates & (QGLPainter::UpdateColor |
                        QGLPainter::UpdateMatrices));
        return;
    }
#endif
    if (!d->program)
        return;
    if ((updates & QGLPainter::UpdateColor) != 0)
        d->program->setUniformValue(d->colorUniform, painter->color());
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        d->program->setUniformValue
            (d->matrixUniform, painter->combinedMatrix());
    }
#endif
}


/*!
    \class QGLLitMaterialEffect
    \since 4.8
    \brief The QGLLitMaterialEffect class provides a standard effect that draws fragments with a lit material.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

#if !defined(QGL_FIXED_FUNCTION_ONLY)

static char const litMaterialVertexShader[] =
    "attribute highp vec4 vertex;\n"
    "attribute highp vec3 normal;\n"
    "uniform highp mat4 matrix;\n"
    "uniform highp mat4 modelView;\n"
    "uniform highp mat3 normalMatrix;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = matrix * vertex;\n"
    "    highp vec4 tvertex = modelView * vertex;\n"
    "    highp vec3 norm = normalize(normalMatrix * normal);\n"
    "    qLightVertex(tvertex, norm);\n"
    "}\n";

static char const litMaterialFragmentShader[] =
#if !defined(QT_OPENGL_ES)
    "varying mediump vec4 qColor;\n"
    "varying mediump vec4 qSecondaryColor;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    gl_FragColor = clamp(qColor + vec4(qSecondaryColor.xyz, 0.0), 0.0, 1.0);\n"
    "}\n";
#else
    "varying mediump vec4 qCombinedColor;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    gl_FragColor = qCombinedColor;\n"
    "}\n";
#endif

// Algorithm from section 2.14.1 of OpenGL 2.1 specification.
static char const litMaterialLightingShader[] =
#if !defined(QT_OPENGL_ES)
"uniform mediump vec3 sdli;\n"   // Direction of the light (must be normalized).
"uniform mediump vec3 pli;\n"    // Position of the light
"uniform mediump float pliw;\n"  // 0 for directional, 1 for positional.
"uniform mediump float srli;\n"  // Spotlight exponent for the light
"uniform mediump float crli;\n"  // Spotlight cutoff for the light
"uniform mediump float ccrli;\n" // Cosine of spotlight cutoff for the light
"uniform mediump float k0;\n"    // Constant attentuation factor for the light
"uniform mediump float k1;\n"    // Linear attentuation factor for the light
"uniform mediump float k2;\n"    // Quadratic attentuation factor for the light
"uniform mediump vec4 acm[2];\n" // Ambient color of the material and light
"uniform mediump vec4 dcm[2];\n" // Diffuse color of the material and light
"uniform mediump vec4 scm[2];\n" // Specular color of the material and light
"uniform mediump vec4 ecm[2];\n" // Emissive color and ambient scene color
"uniform mediump float srm[2];\n"// Specular exponent of the material
"uniform bool viewerAtInfinity;\n" // Light model indicates viewer at infinity
"uniform bool twoSided;\n"       // Light model indicates two-sided lighting

"varying mediump vec4 qColor;\n"
"varying mediump vec4 qSecondaryColor;\n"

"void qLightVertex(vec4 vertex, vec3 normal)\n"
"{\n"
"    int i, material;\n"
"    vec3 toEye, toLight, h;\n"
"    float angle, spot, attenuation;\n"
"    vec4 color, scolor;\n"
"    vec4 adcomponent, scomponent;\n"

    // Determine which material to use.
"    if (!twoSided || normal.z >= 0.0) {\n"
"        material = 0;\n"
"    } else {\n"
"        material = 1;\n"
"        normal = -normal;\n"
"    }\n"

    // Start with the material's emissive color and the ambient scene color,
    // which have been combined into the ecm parameter by the C++ code.
"    color = ecm[material];\n"
"    scolor = vec4(0, 0, 0, 0);\n"

    // Vector from the vertex to the eye position (i.e. the origin).
"    if (viewerAtInfinity)\n"
"        toEye = vec3(0, 0, 1);\n"
"    else\n"
"        toEye = normalize(-vertex.xyz);\n"

    // Determine the cosine of the angle between the normal and the
    // vector from the vertex to the light.
"    if (pliw == 0.0)\n"
"        toLight = normalize(pli);\n"
"    else\n"
"        toLight = normalize(pli - vertex.xyz);\n"
"    angle = max(dot(normal, toLight), 0.0);\n"

    // Calculate the ambient and diffuse light components.
"    adcomponent = acm[material] + angle * dcm[material];\n"

    // Calculate the specular light components.
"    if (angle != 0.0) {\n"
"        h = normalize(toLight + toEye);\n"
"        angle = max(dot(normal, h), 0.0);\n"
"        if (srm[material] != 0.0)\n"
"            scomponent = pow(angle, srm[material]) * scm[material];\n"
"        else\n"
"            scomponent = scm[material];\n"
"    } else {\n"
"        scomponent = vec4(0, 0, 0, 0);\n"
"    }\n"

    // Apply the spotlight angle and exponent.
"    if (crli != 180.0) {\n"
"        spot = max(dot(normalize(vertex.xyz - pli), sdli), 0.0);\n"
"        if (spot < ccrli) {\n"
"            adcomponent = vec4(0, 0, 0, 0);\n"
"            scomponent = vec4(0, 0, 0, 0);\n"
"        } else {\n"
"            spot = pow(spot, srli);\n"
"            adcomponent *= spot;\n"
"            scomponent *= spot;\n"
"        }\n"
"    }\n"

    // Apply attenuation to the colors.
"    if (pliw != 0.0) {\n"
"        attenuation = k0;\n"
"        if (k1 != 0.0 || k2 != 0.0) {\n"
"            float len = length(pli - vertex.xyz);\n"
"            attenuation += k1 * len + k2 * len * len;\n"
"        }\n"
"        color += adcomponent / attenuation;\n"
"        scolor += scomponent / attenuation;\n"
"    } else {\n"
"        color += adcomponent;\n"
"        scolor += scomponent;\n"
"    }\n"

    // Generate the final output colors.
"    float alpha = dcm[material].a;\n"
"    qColor = vec4(clamp(color.rgb, 0.0, 1.0), alpha);\n"
"    qSecondaryColor = clamp(scolor, 0.0, 1.0);\n"
"}\n";
#else
"uniform mediump vec3 sdli;\n"   // Direction of the light (must be normalized).
"uniform mediump vec3 pli;\n"    // Position of the light
"uniform mediump float pliw;\n"  // 0 for directional, 1 for positional.
"uniform mediump float srli;\n"  // Spotlight exponent for the light
"uniform mediump float crli;\n"  // Spotlight cutoff for the light
"uniform mediump float ccrli;\n" // Cosine of spotlight cutoff for the light
"uniform mediump vec4 acm;\n"    // Ambient color of the material and light
"uniform mediump vec4 dcm;\n"    // Diffuse color of the material and light
"uniform mediump vec4 scm;\n"    // Specular color of the material and light
"uniform mediump vec4 ecm;\n"    // Emissive color and ambient scene color
"uniform mediump float srm;\n"   // Specular exponent of the material
"uniform bool viewerAtInfinity;\n" // Light model indicates viewer at infinity

"varying mediump vec4 qColor;\n"
"varying mediump vec4 qSecondaryColor;\n"
"varying mediump vec4 qCombinedColor;\n"

"void qLightVertex(vec4 vertex, vec3 normal)\n"
"{\n"
"    vec3 toEye, toLight, h;\n"
"    float angle, spot;\n"
"    vec4 color, scolor;\n"

    // Vector from the vertex to the eye position (i.e. the origin).
"    if (viewerAtInfinity)\n"
"        toEye = vec3(0, 0, 1);\n"
"    else\n"
"        toEye = normalize(-vertex.xyz);\n"

    // Determine the cosine of the angle between the normal and the
    // vector from the vertex to the light.
"    if (pliw == 0.0)\n"
"        toLight = normalize(pli);\n"
"    else\n"
"        toLight = normalize(pli - vertex.xyz);\n"
"    angle = max(dot(normal, toLight), 0.0);\n"

    // Calculate the ambient and diffuse light components.
"    color = acm + angle * dcm;\n"

    // Calculate the specular light components.
"    if (angle != 0.0) {\n"
"        h = normalize(toLight + toEye);\n"
"        angle = max(dot(normal, h), 0.0);\n"
"        if (srm != 0.0)\n"
"            scolor = pow(angle, srm) * scm;\n"
"        else\n"
"            scolor = scm;\n"
"    } else {\n"
"        scolor = vec4(0, 0, 0, 0);\n"
"    }\n"

    // Apply the spotlight angle and exponent.
"    if (crli != 180.0) {\n"
"        spot = max(dot(normalize(vertex.xyz - pli), sdli), 0.0);\n"
"        if (spot < ccrli) {\n"
"            color = vec4(0, 0, 0, 0);\n"
"            scolor = vec4(0, 0, 0, 0);\n"
"        } else {\n"
"            spot = pow(spot, srli);\n"
"            color *= spot;\n"
"            scolor *= spot;\n"
"        }\n"
"    }\n"

    // Generate the final output colors.
"    color += ecm;\n"
"    float alpha = dcm.a;\n"
    // Note: Normally, the qCombinedColor is ignored, and per-pixel
    // value is calculated.
    // If OPENGL_ES is defined, qColor and qSecondaryColor are ignored,
    // and qCombinedColor is calculated here to speed up the fragment shader.
"    qColor = vec4(clamp(color.rgb, 0.0, 1.0), alpha);\n"
"    qSecondaryColor = clamp(scolor, 0.0, 1.0);\n"
"    qCombinedColor = clamp(qColor + vec4(qSecondaryColor.xyz, 0.0), 0.0, 1.0);\n"
"}\n";
#endif

static QByteArray createVertexSource(const char *lighting, const char *extra)
{
    QByteArray contents(lighting);
    return contents + extra;
}

static inline QVector4D colorToVector4(const QColor& color)
{
    return QVector4D(color.redF(), color.greenF(),
                     color.blueF(), color.alphaF());
}

// Combine a material and light color into a single color.
static inline QVector4D colorToVector4
    (const QColor &color, const QColor &lightColor)
{
    return QVector4D(color.redF() * lightColor.redF(),
                     color.greenF() * lightColor.greenF(),
                     color.blueF() * lightColor.blueF(),
                     color.alphaF() * lightColor.alphaF());
}

#endif

class QGLLitMaterialEffectPrivate
{
public:
    QGLLitMaterialEffectPrivate()
        : program(0)
        , matrixUniform(-1)
        , modelViewUniform(-1)
        , normalMatrixUniform(-1)
        , textureMode(0)
#if !defined(QGL_FIXED_FUNCTION_ONLY)
        , vertexShader(litMaterialVertexShader)
        , fragmentShader(litMaterialFragmentShader)
#else
        , vertexShader(0)
        , fragmentShader(0)
#endif
        , programName(QLatin1String("qt.color.material"))
        , isFixedFunction(false)
    {
    }

    QOpenGLShaderProgram *program;
    int matrixUniform;
    int modelViewUniform;
    int normalMatrixUniform;
    GLenum textureMode;
    const char *vertexShader;
    const char *fragmentShader;
    QString programName;
    bool isFixedFunction;
};

/*!
    Constructs a new lit material effect.
*/
QGLLitMaterialEffect::QGLLitMaterialEffect()
    : d_ptr(new QGLLitMaterialEffectPrivate)
{
}

/*!
    \internal
*/
QGLLitMaterialEffect::QGLLitMaterialEffect
        (GLenum mode, const char *vshader, const char *fshader,
         const QString& programName)
    : d_ptr(new QGLLitMaterialEffectPrivate)
{
    Q_D(QGLLitMaterialEffect);
    d->textureMode = mode;
    d->vertexShader = vshader;
    d->fragmentShader = fshader;
    d->programName = programName;
}

/*!
    Destroys this lit material effect.
*/
QGLLitMaterialEffect::~QGLLitMaterialEffect()
{
}

/*!
    \reimp
*/
void QGLLitMaterialEffect::setActive(QGLPainter *painter, bool flag)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    Q_UNUSED(painter);
    Q_D(QGLLitMaterialEffect);
    if (flag) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        if (d->textureMode) {
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, d->textureMode);
            glEnable(GL_TEXTURE_2D);
        }
    } else {
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        if (d->textureMode) {
            qt_gl_ClientActiveTexture(GL_TEXTURE0);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisable(GL_TEXTURE_2D);
        }
    }
#else
    Q_UNUSED(painter);
    Q_D(QGLLitMaterialEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (painter->isFixedFunction()) {
        d->isFixedFunction = true;
        if (flag) {
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_NORMAL_ARRAY);
            if (d->textureMode) {
                qt_gl_ClientActiveTexture(GL_TEXTURE0);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, d->textureMode);
                glEnable(GL_TEXTURE_2D);
            }
        } else {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            if (d->textureMode) {
                qt_gl_ClientActiveTexture(GL_TEXTURE0);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisable(GL_TEXTURE_2D);
            }
        }
        return;
    }
#endif
    QOpenGLShaderProgram *program = painter->cachedProgram(d->programName);
    d->program = program;
    if (!program) {
        if (!flag)
            return;
        program = new QOpenGLShaderProgram;
        program->addShaderFromSourceCode(QOpenGLShader::Vertex, createVertexSource(litMaterialLightingShader, d->vertexShader));
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, d->fragmentShader);
        program->bindAttributeLocation("vertex", QGL::Position);
        program->bindAttributeLocation("normal", QGL::Normal);
        if (d->textureMode != 0)
            program->bindAttributeLocation("texcoord", QGL::TextureCoord0);
        if (!program->link()) {
            qWarning("QGLLitMaterialEffect::setActive(): could not link shader program");
            delete program;
            program = 0;
            return;
        }
        painter->setCachedProgram(d->programName, program);
        d->program = program;
        d->matrixUniform = program->uniformLocation("matrix");
        d->modelViewUniform = program->uniformLocation("modelView");
        d->normalMatrixUniform = program->uniformLocation("normalMatrix");
        program->bind();
        if (d->textureMode != 0) {
            program->setUniformValue("tex", 0);
            program->enableAttributeArray(QGL::TextureCoord0);
        }
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::Normal);
    } else if (flag) {
        d->matrixUniform = program->uniformLocation("matrix");
        d->modelViewUniform = program->uniformLocation("modelView");
        d->normalMatrixUniform = program->uniformLocation("normalMatrix");
        program->bind();
        if (d->textureMode != 0) {
            program->setUniformValue("tex", 0);
            program->enableAttributeArray(QGL::TextureCoord0);
        }
        program->enableAttributeArray(QGL::Position);
        program->enableAttributeArray(QGL::Normal);
    } else {
        program->disableAttributeArray(QGL::Position);
        program->disableAttributeArray(QGL::Normal);
        if (d->textureMode != 0)
            program->disableAttributeArray(QGL::TextureCoord0);
        program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLLitMaterialEffect::update
        (QGLPainter *painter, QGLPainter::Updates updates)
{
#if defined(QGL_FIXED_FUNCTION_ONLY)
    painter->updateFixedFunction
        (updates & (QGLPainter::UpdateMatrices |
                    QGLPainter::UpdateLights |
                    QGLPainter::UpdateMaterials));
#else
    Q_D(QGLLitMaterialEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->isFixedFunction) {
        painter->updateFixedFunction
            (updates & (QGLPainter::UpdateMatrices |
                        QGLPainter::UpdateLights |
                        QGLPainter::UpdateMaterials));
        return;
    }
#endif
    QOpenGLShaderProgram *program = d->program;
    if (!program)
        return;
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        program->setUniformValue(d->matrixUniform, painter->combinedMatrix());
        program->setUniformValue(d->modelViewUniform, painter->modelViewMatrix());
        program->setUniformValue(d->normalMatrixUniform, painter->normalMatrix());
    }
    const QGLLightParameters *lparams = painter->mainLight();
    QMatrix4x4 ltransform = painter->mainLightTransform();
    const QGLLightModel *model = painter->lightModel();
    if ((updates & (QGLPainter::UpdateLights | QGLPainter::UpdateMaterials)) != 0) {
        // Set the uniform variables for the light.
        program->setUniformValue
            ("sdli", lparams->eyeSpotDirection(ltransform).normalized());
        QVector4D pli = lparams->eyePosition(ltransform);
        program->setUniformValue("pli", QVector3D(pli.x(), pli.y(), pli.z()));
        program->setUniformValue("pliw", GLfloat(pli.w()));
        program->setUniformValue("srli", GLfloat(lparams->spotExponent()));
        program->setUniformValue("crli", GLfloat(lparams->spotAngle()));
        program->setUniformValue("ccrli", GLfloat(lparams->spotCosAngle()));
#if !defined(QT_OPENGL_ES)
        // Attenuation is not supported under ES, for performance.
        program->setUniformValue("k0", GLfloat(lparams->constantAttenuation()));
        program->setUniformValue("k1", GLfloat(lparams->linearAttenuation()));
        program->setUniformValue("k2", GLfloat(lparams->quadraticAttenuation()));
#endif

        // Set the uniform variables for the light model.
#if !defined(QT_OPENGL_ES)
        program->setUniformValue("twoSided", (int)(model->model() == QGLLightModel::TwoSided));
#endif
        program->setUniformValue("viewerAtInfinity", (int)(model->viewerPosition() == QGLLightModel::ViewerAtInfinity));
#if !defined(QT_OPENGL_ES)
        if (d->textureMode != 0)
            program->setUniformValue("separateSpecular", (int)(model->colorControl() == QGLLightModel::SeparateSpecularColor));
#endif

        // Set the uniform variables for the front and back materials.
#if defined(QT_OPENGL_ES)
        static const int MaxMaterials = 1;
#else
        static const int MaxMaterials = 2;
#endif
        QVector4D acm[MaxMaterials];
        QVector4D dcm[MaxMaterials];
        QVector4D scm[MaxMaterials];
        QVector4D ecm[MaxMaterials];
        float srm[MaxMaterials];
        const QGLMaterial *mparams = painter->faceMaterial(QGL::FrontFaces);
        acm[0] = colorToVector4(mparams->ambientColor(), lparams->ambientColor());
        dcm[0] = colorToVector4(mparams->diffuseColor(), lparams->diffuseColor());
        scm[0] = colorToVector4(mparams->specularColor(), lparams->specularColor());
        ecm[0] = colorToVector4(mparams->emittedLight()) +
                 colorToVector4(mparams->ambientColor(),
                                model->ambientSceneColor());
        srm[0] = (float)(mparams->shininess());
#if !defined(QT_OPENGL_ES)
        mparams = painter->faceMaterial(QGL::BackFaces);
        acm[1] = colorToVector4(mparams->ambientColor(), lparams->ambientColor());
        dcm[1] = colorToVector4(mparams->diffuseColor(), lparams->diffuseColor());
        scm[1] = colorToVector4(mparams->specularColor(), lparams->specularColor());
        ecm[1] = colorToVector4(mparams->emittedLight()) +
                 colorToVector4(mparams->ambientColor(),
                                model->ambientSceneColor());
        srm[1] = (float)(mparams->shininess());
#endif
        program->setUniformValueArray("acm", (const GLfloat *)acm, MaxMaterials, 4);
        program->setUniformValueArray("dcm", (const GLfloat *)dcm, MaxMaterials, 4);
        program->setUniformValueArray("scm", (const GLfloat *)scm, MaxMaterials, 4);
        program->setUniformValueArray("ecm", (const GLfloat *)ecm, MaxMaterials, 4);
        program->setUniformValueArray("srm", srm, MaxMaterials, 1);
    }
#endif
}


/*!
    \class QGLLitTextureEffect
    \since 4.8
    \brief The QGLLitTextureEffect class provides a standard effect base class for drawing fragments with a lit texture.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal

    \sa QGLLitDecalTextureEffect, QGLLitModulateTextureEffect
*/

/*!
    \class QGLLitDecalTextureEffect
    \since 4.8
    \brief The QGLLitDecalTextureEffect class provides a standard effect for drawing fragments with a texture decaled over a lit material.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

/*!
    \class QGLLitModulateTextureEffect
    \since 4.8
    \brief The QGLLitModulateTextureEffect class provides a standard effect for drawing fragments with a texture modulated with a lit material.
    \ingroup qt3d
    \ingroup qt3d::painting
    \internal
*/

/*!
    \internal
*/
QGLLitTextureEffect::QGLLitTextureEffect
        (GLenum mode, const char *vshader, const char *fshader,
         const QString& programName)
    : QGLLitMaterialEffect(mode, vshader, fshader, programName)
{
}

/*!
    Destroys this lit texture effect.
*/
QGLLitTextureEffect::~QGLLitTextureEffect()
{
}

#if !defined(QGL_FIXED_FUNCTION_ONLY)

static char const litTextureVertexShader[] =
    "attribute highp vec4 vertex;\n"
    "attribute highp vec3 normal;\n"
    "attribute highp vec4 texcoord;\n"
    "uniform highp mat4 matrix;\n"
    "uniform highp mat4 modelView;\n"
    "uniform highp mat3 normalMatrix;\n"
    "varying highp vec4 qt_TexCoord0;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = matrix * vertex;\n"
    "    highp vec4 tvertex = modelView * vertex;\n"
    "    highp vec3 norm = normalize(normalMatrix * normal);\n"
    "    qLightVertex(tvertex, norm);\n"
    "    qt_TexCoord0 = texcoord;\n"
    "}\n";

static char const litDecalFragmentShader[] =
    "uniform sampler2D tex;\n"
#if defined(QT_OPENGL_ES)
    "varying mediump vec4 qCombinedColor;\n"
#else
    "uniform bool separateSpecular;\n"
    "varying mediump vec4 qColor;\n"
    "varying mediump vec4 qSecondaryColor;\n"
#endif
    "varying highp vec4 qt_TexCoord0;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    mediump vec4 col = texture2D(tex, qt_TexCoord0.st);\n"
#if defined(QT_OPENGL_ES)
    "    gl_FragColor = vec4(clamp(qCombinedColor.rgb * (1.0 - col.a) + col.rgb * col.a, 0.0, 1.0), qCombinedColor.a);\n"
#else
    "    if (separateSpecular) {\n"
    "        gl_FragColor = vec4(clamp(qColor.rgb * (1.0 - col.a) + col.rgb * col.a + qSecondaryColor.xyz, 0.0, 1.0), qColor.a);\n"
    "    } else {\n"
    "        mediump vec4 lcolor = clamp(qColor + vec4(qSecondaryColor.xyz, 0.0), 0.0, 1.0);\n"
    "        gl_FragColor = vec4(clamp(lcolor.rgb * (1.0 - col.a) + col.rgb * col.a, 0.0, 1.0), lcolor.a);\n"
    "    }\n"
#endif
    "}\n";

static char const litModulateFragmentShader[] =
    "uniform sampler2D tex;\n"
#if defined(QT_OPENGL_ES)
    "varying mediump vec4 qCombinedColor;\n"
#else
    "uniform bool separateSpecular;\n"
    "varying mediump vec4 qColor;\n"
    "varying mediump vec4 qSecondaryColor;\n"
#endif
    "varying highp vec4 qt_TexCoord0;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    mediump vec4 col = texture2D(tex, qt_TexCoord0.st);\n"
#if defined(QT_OPENGL_ES)
    "    gl_FragColor = col * qCombinedColor;\n"
#else
    "    if (separateSpecular) {\n"
    "        gl_FragColor = clamp(col * qColor + vec4(qSecondaryColor.xyz, 0.0), 0.0, 1.0);\n"
    "    } else {\n"
    "        mediump vec4 lcolor = clamp(qColor + vec4(qSecondaryColor.xyz, 0.0), 0.0, 1.0);\n"
    "        gl_FragColor = col * lcolor;\n"
    "    }\n"
#endif
    "}\n";

#endif

#ifndef GL_MODULATE
#define GL_MODULATE 0x2100
#endif
#ifndef GL_DECAL
#define GL_DECAL 0x2101
#endif

/*!
    Constructs a new lit decal texture effect.
*/
QGLLitDecalTextureEffect::QGLLitDecalTextureEffect()
#if defined(QGL_FIXED_FUNCTION_ONLY)
    : QGLLitTextureEffect(GL_DECAL, 0, 0, QString())
#else
    : QGLLitTextureEffect(GL_DECAL,
                          litTextureVertexShader, litDecalFragmentShader,
                          QLatin1String("qt.texture.litdecal"))
#endif
{
}

/*!
    Destroys this lit decal texture effect.
*/
QGLLitDecalTextureEffect::~QGLLitDecalTextureEffect()
{
}

/*!
    Constructs a new lit modulate texture effect.
*/
QGLLitModulateTextureEffect::QGLLitModulateTextureEffect()
#if defined(QGL_FIXED_FUNCTION_ONLY)
    : QGLLitTextureEffect(GL_MODULATE, 0, 0, QString())
#else
    : QGLLitTextureEffect(GL_MODULATE,
                          litTextureVertexShader, litModulateFragmentShader,
                          QLatin1String("qt.texture.litmodulate"))
#endif
{
}

/*!
    Destroys this lit modulate texture effect.
*/
QGLLitModulateTextureEffect::~QGLLitModulateTextureEffect()
{
}


/*!
    \class QGLShaderProgramEffect
    \since 4.8
    \brief The QGLShaderProgramEffect class provides applications with the ability to use shader programs written in GLSL as effects for 3D rendering.
    \ingroup qt3d
    \ingroup qt3d::painting

    \section1 Writing portable shaders

    Shader programs can be difficult to reuse across OpenGL implementations
    because of varying levels of support for standard vertex attributes and
    uniform variables.  In particular, GLSL/ES lacks all of the
    standard variables that are present on desktop OpenGL systems:
    \c{gl_Vertex}, \c{gl_Normal}, \c{gl_Color}, and so on.  Desktop OpenGL
    lacks the variable qualifiers \c{highp}, \c{mediump}, and \c{lowp}.

    QGLShaderProgramEffect is built on top of
    \l{http://doc.qt.nokia.com/4.7/qglshaderprogram.html}{QGLShaderProgram},
    which makes the process of writing portable shaders easier by
    prefixing all shader programs with the following lines on desktop OpenGL:

    \code
    #define highp
    #define mediump
    #define lowp
    \endcode

    This makes it possible to run most GLSL/ES shader programs
    on desktop systems.  The programmer should also restrict themselves
    to just features that are present in GLSL/ES, and avoid
    standard variable names that only work on the desktop.

    QGLShaderProgramEffect also defines some standard attribute and uniform
    variable names that all shaders are expected to use.  The following
    sections define these standard names.

    \section1 Attributes

    QGLShaderProgramEffect provides a standard set of 8 named vertex
    attributes that can be provided via QGLPainter::setVertexBundle():

    \table
    \header \li Shader Variable \li Mesh Attribute \li Purpose
    \row \li \c qt_Vertex \li QGL::Position
         \li The primary position of the vertex.
    \row \li \c qt_Normal \li QGL::Normal
         \li The normal at each vertex, for lit material effects.
    \row \li \c qt_Color \li QGL::Color
         \li The color at each vertex, for per-vertex color effects.
    \row \li \c qt_MultiTexCoord0 \li QGL::TextureCoord0
         \li The texture co-ordinate at each vertex for texture unit 0.
    \row \li \c qt_MultiTexCoord1 \li QGL::TextureCoord1
         \li Secondary texture co-ordinate at each vertex.
    \row \li \c qt_MultiTexCoord2 \li QGL::TextureCoord2
         \li Tertiary texture co-ordinate at each vertex.
    \row \li \c qt_Custom0 \li QGL::CustomVertex0
         \li First custom vertex attribute that can be used for any
            user-defined purpose.
    \row \li \c qt_Custom1 \li QGL::CustomVertex1
         \li Second custom vertex attribute that can be used for any
            user-defined purpose.
    \endtable

    These attributes may be used in the vertexShader(), as in the following
    example of a simple texture shader:

    \code
    attribute highp vec4 qt_Vertex;
    attribute highp vec4 qt_MultiTexCoord0;
    uniform mediump mat4 qt_ModelViewProjectionMatrix;
    varying highp vec4 qt_TexCoord0;

    void main(void)
    {
        gl_Position = qt_ModelViewProjectionMatrix * qt_Vertex;
        qt_TexCoord0 = qt_MultiTexCoord0;
    }
    \endcode

    \section1 Uniform variables

    QGLShaderProgramEffect provides a standard set of uniform variables for
    common values from the QGLPainter environment:

    \table
    \header \li Shader Variable \li Purpose
    \row \li \c qt_ModelViewProjectionMatrix
         \li Combination of the modelview and projection matrices into a
            single 4x4 matrix.
    \row \li \c qt_ModelViewMatrix
         \li Modelview matrix without the projection.  This is typically
            used for performing calculations in eye co-ordinates.
    \row \li \c qt_ProjectionMatrix
         \li Projection matrix without the modelview.
    \row \li \c qt_NormalMatrix
         \li Normal matrix, which is the transpose of the inverse of the
            top-left 3x3 part of the modelview matrix.  This is typically
            used in lighting calcuations to transform \c qt_Normal.
    \row \li \c qt_WorldMatrix
         \li Modelview matrix without the eye position and orientation
            component.  See QGLPainter::worldMatrix() for further
            information.
    \row \li \c qt_Texture0
         \li Sampler corresponding to the texture on unit 0.
    \row \li \c qt_Texture1
         \li Sampler corresponding to the texture on unit 1.
    \row \li \c qt_Texture2
         \li Sampler corresponding to the texture on unit 2.
    \row \li \c qt_Color
         \li Set to the value of the QGLPainter::color() property.
            This is typically used for flat-color shaders that do
            not involve lighting.  Note that this is different from
            the \c qt_Color attribute, which provides per-vertex colors.
    \endtable

    The above variables are usually declared in the shaders as follows
    (where \c highp may be replaced with \c mediump or \c lowp depending
    upon the shader's precision requirements):

    \code
    uniform highp mat4 qt_ModelViewProjectionMatrix;
    uniform highp mat4 qt_ModelViewMatrix;
    uniform highp mat4 qt_ProjectionMatrix;
    uniform highp mat3 qt_NormalMatrix;
    uniform sampler2D qt_Texture0;
    uniform sampler2D qt_Texture1;
    uniform sampler2D qt_Texture2;
    uniform highp vec4 qt_Color;
    \endcode

    \section1 Material parameters

    QGLShaderProgramEffect will provide information about the front and
    back materials from QGLPainter::faceMaterial() if the
    \c qt_Materials array is present in the shader code.
    The array should be declared as follows:

    \code
    struct qt_MaterialParameters {
        mediump vec4 emission;
        mediump vec4 ambient;
        mediump vec4 diffuse;
        mediump vec4 specular;
        mediump float shininess;
    };
    uniform qt_MaterialParameters qt_Materials[2];
    \endcode

    The front material will be provided as index 0 and the back
    material will be provided as index 1.  If the shader only
    needs a single material, then the \c qt_Material variable
    can be declared instead:

    \code
    uniform qt_MaterialParameters qt_Material;
    \endcode

    The front material will be provided as the value of \c qt_Material
    and the back material will be ignored.

    Note: the \c emission parameter is actually QGLMaterial::emittedLight()
    combined with the QGLLightModel::ambientSceneColor() and
    QGLMaterial::ambientColor().  This helps simplify lighting shaders.

    \section1 Lighting parameters

    QGLShaderProgramEffect will provide information about the current lights
    specified on the QGLPainter if the \c qt_Lights array is present
    in the shader code.  The array should be declared as follows:

    \code
    struct qt_LightParameters {
        mediump vec4 ambient;
        mediump vec4 diffuse;
        mediump vec4 specular;
        mediump vec4 position;
        mediump vec3 spotDirection;
        mediump float spotExponent;
        mediump float spotCutoff;
        mediump float spotCosCutoff;
        mediump float constantAttenuation;
        mediump float linearAttenuation;
        mediump float quadraticAttenuation;
    };
    const int qt_MaxLights = 8;
    uniform qt_LightParameters qt_Lights[qt_MaxLights];
    uniform int qt_NumLights;
    \endcode

    As shown, up to 8 lights can be supported at once.  Usually this is
    more lights than most shaders need, and so the user can change the
    \c qt_MaxLights constant to a smaller value if they wish.  Be sure
    to also call setMaximumLights() to tell QGLShaderProgramEffect about
    the new light count limit.

    The \c qt_NumLights uniform variable will be set to the actual number
    of lights that are active on the QGLPainter when update() is called.

    Because most shaders will only need a single light, the shader can
    declare the \c qt_Light variable instead of the \c qt_Lights array:

    \code
    struct qt_SingleLightParameters {
        mediump vec4 position;
        mediump vec3 spotDirection;
        mediump float spotExponent;
        mediump float spotCutoff;
        mediump float spotCosCutoff;
        mediump float constantAttenuation;
        mediump float linearAttenuation;
        mediump float quadraticAttenuation;
    };
    uniform qt_SingleLightParameters qt_Light;
    \endcode

    Note that we have omitted the \c ambient, \c diffuse, and \c specular
    colors for the single light.  QGLShaderProgramEffect will combine the material
    and light colors ahead of time into \c qt_Material or \c qt_Materials.
    This makes lighting shaders more efficient because they do not have
    to compute \c material_color * \c light_color; just \c material_color
    is sufficient.

    \section1 Varying variables

    The name and purpose of the varying variables is up to the
    author of the vertex and fragment shaders, but the following names
    are recommended for texture co-ordinates:

    \table
    \header \li Varying Variable \li Purpose
    \row \li \c qt_TexCoord0
         \li Texture coordinate for unit 0, copied from the \c qt_MultiTexCoord0
            attribute.
    \row \li \c qt_TexCoord1
         \li Texture coordinate for unit 1, copied from the \c qt_MultiTexCoord1
            attribute.
    \row \li \c qt_TexCoord2
         \li Texture coordinate for unit 2, copied from the \c qt_MultiTexCoord2
            attribute.
    \endtable

    \section1 Lighting shader example

    The following example demonstrates what a lighting shader that
    uses a single light, a single material, and a texture might look like.
    The shader is quite complex but demonstrates most of the features that
    can be found in the lighting implementation of a fixed-function
    OpenGL pipeline:

    \code
    attribute highp vec4 qt_Vertex;
    uniform highp mat4 qt_ModelViewProjectionMatrix;
    attribute highp vec3 qt_Normal;
    uniform highp mat4 qt_ModelViewMatrix;
    uniform highp mat3 qt_NormalMatrix;
    attribute highp vec4 qt_MultiTexCoord0;
    varying highp vec4 qt_TexCoord0;

    struct qt_MaterialParameters {
        mediump vec4 emission;
        mediump vec4 ambient;
        mediump vec4 diffuse;
        mediump vec4 specular;
        mediump float shininess;
    };
    uniform qt_MaterialParameters qt_Material;

    struct qt_SingleLightParameters {
        mediump vec4 position;
        mediump vec3 spotDirection;
        mediump float spotExponent;
        mediump float spotCutoff;
        mediump float spotCosCutoff;
        mediump float constantAttenuation;
        mediump float linearAttenuation;
        mediump float quadraticAttenuation;
    };
    uniform qt_SingleLightParameters qt_Light;

    varying mediump vec4 litColor;
    varying mediump vec4 litSecondaryColor;

    void main(void)
    {
        gl_Position = qt_ModelViewProjectionMatrix * qt_Vertex;
        gTexCoord0 = qt_MultiTexCoord0;

        // Calculate the vertex and normal to use for lighting calculations.
        highp vec4 vertex = qt_ModelViewMatrix * qt_Vertex;
        highp vec3 normal = normalize(qt_NormalMatrix * qt_Normal);

        // Start with the material's emissive color and the ambient scene color,
        // which have been combined into the emission parameter.
        vec4 color = ggl_Material.emission;

        // Viewer is at infinity.
        vec3 toEye = vec3(0, 0, 1);

        // Determine the angle between the normal and the light direction.
        vec4 pli = qt_Light.position;
        vec3 toLight;
        if (pli.w == 0.0)
            toLight = normalize(pli.xyz);
        else
            toLight = normalize(pli.xyz - vertex.xyz);
        float angle = max(dot(normal, toLight), 0.0);

        // Calculate the ambient and diffuse light components.
        vec4 adcomponent = qt_Material.ambient + angle * qt_Material.diffuse;

        // Calculate the specular light components.
        vec4 scomponent;
        if (angle != 0.0) {
            vec3 h = normalize(toLight + toEye);
            angle = max(dot(normal, h), 0.0);
            float srm = qt_Material.shininess;
            vec4 scm = qt_Material.specular;
            if (srm != 0.0)
                scomponent = pow(angle, srm) * scm;
            else
                scomponent = scm;
        } else {
            scomponent = vec4(0, 0, 0, 0);
        }

        // Apply the spotlight angle and exponent.
        if (qt_Light.spotCutoff != 180.0) {
            float spot = max(dot(normalize(vertex.xyz - pli.xyz),
                                 qt_Light.spotDirection), 0.0);
            if (spot < qt_Light.spotCosCutoff) {
                adcomponent = vec4(0, 0, 0, 0);
                scomponent = vec4(0, 0, 0, 0);
            } else {
                spot = pow(spot, qt_Light.spotExponent);
                adcomponent *= spot;
                scomponent *= spot;
            }
        }

        // Apply attenuation to the colors.
        if (pli.w != 0.0) {
            float attenuation = qt_Light.constantAttenuation;
            float k1 = qt_Light.linearAttenuation;
            float k2 = qt_Light.quadraticAttenuation;
            if (k1 != 0.0 || k2 != 0.0) {
                float len = length(pli.xyz - vertex.xyz);
                attenuation += k1 * len + k2 * len * len;
            }
            color += adcomponent / attenuation;
            scolor += scomponent / attenuation;
        } else {
            color += adcomponent;
            scolor += scomponent;
        }

        // Generate the final output colors to pass to the fragment shader.
        float alpha = qt_Material.diffuse.a;
        litColor = vec4(clamp(color.rgb, 0.0, 1.0), alpha);
        litSecondaryColor = vec4(clamp(scolor.rgb, 0.0, 1.0), 0.0);
    }
    \endcode

    The corresponding fragment shader is as follows:

    \code
    varying mediump vec4 litColor;
    varying mediump vec4 litSecondaryColor;
    varying highp vec4 qt_TexCoord0;

    void main(void)
    {
        vec4 color = litColor * texture2D(qt_Texture0, qt_TexCoord0.st);
        gl_FragColor = clamp(color + litSecondaryColor, 0.0, 1.0);
    }
    \endcode

    \section1 Fixed function operation

    If the OpenGL implementation does not support shaders, then
    QGLShaderProgramEffect will fall back to a flat color effect based
    on QGLPainter::color().  It is recommended that the application
    consult QGLPainter::isFixedFunction() to determine if some
    other effect should be used instead.
*/

class QGLShaderProgramEffectPrivate
{
public:
    QGLShaderProgramEffectPrivate()
        : geometryInputType(GL_TRIANGLE_STRIP)
        , geometryOutputType(GL_TRIANGLE_STRIP)
        , maximumLights(8)
        , attributes(0)
        , regenerate(true)
        , fixedFunction(false)
#if !defined(QGL_FIXED_FUNCTION_ONLY)
        , program(0)
        , matrix(-1)
        , mvMatrix(-1)
        , projMatrix(-1)
        , normalMatrix(-1)
        , worldMatrix(-1)
        , texture0(-1)
        , texture1(-1)
        , texture2(-1)
        , color(-1)
        , numLights(-1)
        , haveLight(0)
        , haveLights(0)
        , haveMaterial(0)
        , haveMaterials(0)
#endif
    {
    }
    ~QGLShaderProgramEffectPrivate()
    {
#if !defined(QGL_FIXED_FUNCTION_ONLY)
        delete program;
#endif
    }

    QByteArray vertexShader;
    QByteArray fragmentShader;
    QByteArray geometryShader;
    GLenum geometryInputType;
    GLenum geometryOutputType;
    int maximumLights;
    int attributes;
    bool regenerate;
    bool fixedFunction;
#if !defined(QGL_FIXED_FUNCTION_ONLY)
    QOpenGLShaderProgram *program;
    int matrix;
    int mvMatrix;
    int projMatrix;
    int normalMatrix;
    int worldMatrix;
    int texture0;
    int texture1;
    int texture2;
    int color;
    int numLights;
    int haveLight : 1;
    int haveLights : 1;
    int haveMaterial : 1;
    int haveMaterials : 1;

    void setUniformValue
        (const char *array, int index, const char *field, GLfloat v);
    void setUniformValue
        (const char *array, int index, const char *field, const QVector3D &v);
    void setUniformValue
        (const char *array, int index, const char *field, const QVector4D &v);
    void setUniformValue
        (const char *array, int index, const char *field, const QColor &v);

    void setLight
        (const QGLLightParameters *lparams, const QMatrix4x4 &ltransform,
         const char *array, int index);
    void setMaterial
        (const QGLMaterial *mparams, const QGLLightModel *model,
         const QGLLightParameters *lparams, const char *array, int index);
#endif
};

#if !defined(QGL_FIXED_FUNCTION_ONLY)

void QGLShaderProgramEffectPrivate::setUniformValue
    (const char *array, int index, const char *field, GLfloat v)
{
    char name[128];
    if (index >= 0)
        qsnprintf(name, sizeof(name), "%s[%d].%s", array, index, field);
    else
        qsnprintf(name, sizeof(name), "%s.%s", array, field);
    program->setUniformValue(name, v);
}

void QGLShaderProgramEffectPrivate::setUniformValue
    (const char *array, int index, const char *field, const QVector3D &v)
{
    char name[128];
    if (index >= 0)
        qsnprintf(name, sizeof(name), "%s[%d].%s", array, index, field);
    else
        qsnprintf(name, sizeof(name), "%s.%s", array, field);
    program->setUniformValue(name, v);
}

void QGLShaderProgramEffectPrivate::setUniformValue
    (const char *array, int index, const char *field, const QVector4D &v)
{
    char name[128];
    if (index >= 0)
        qsnprintf(name, sizeof(name), "%s[%d].%s", array, index, field);
    else
        qsnprintf(name, sizeof(name), "%s.%s", array, field);
    program->setUniformValue(name, v);
}

void QGLShaderProgramEffectPrivate::setUniformValue
    (const char *array, int index, const char *field, const QColor &v)
{
    char name[128];
    if (index >= 0)
        qsnprintf(name, sizeof(name), "%s[%d].%s", array, index, field);
    else
        qsnprintf(name, sizeof(name), "%s.%s", array, field);
    program->setUniformValue(name, v);
}

void QGLShaderProgramEffectPrivate::setLight
    (const QGLLightParameters *lparams, const QMatrix4x4 &ltransform,
     const char *array, int index)
{
    if (index >= 0) {
        // Single lights embed the color values into the material.
        setUniformValue(array, index, "ambient", lparams->ambientColor());
        setUniformValue(array, index, "diffuse", lparams->diffuseColor());
        setUniformValue(array, index, "specular", lparams->specularColor());
    }
    setUniformValue
        (array, index, "position", lparams->eyePosition(ltransform));
    setUniformValue
        (array, index, "spotDirection",
         lparams->eyeSpotDirection(ltransform).normalized());
    setUniformValue
        (array, index, "spotExponent", GLfloat(lparams->spotExponent()));
    setUniformValue
        (array, index, "spotCutoff", GLfloat(lparams->spotAngle()));
    setUniformValue
        (array, index, "spotCosCutoff", GLfloat(lparams->spotCosAngle()));
    setUniformValue
        (array, index, "constantAttenuation",
         GLfloat(lparams->constantAttenuation()));
    setUniformValue
        (array, index, "linearAttenuation",
         GLfloat(lparams->linearAttenuation()));
    setUniformValue
        (array, index, "quadraticAttenuation",
         GLfloat(lparams->quadraticAttenuation()));
}

#if 0
static inline QVector4D colorToVector4(const QColor& color)
{
    return QVector4D(color.redF(), color.greenF(),
                     color.blueF(), color.alphaF());
}

// Combine a material and light color into a single color.
static inline QVector4D colorToVector4
    (const QColor &color, const QColor &lightColor)
{
    return QVector4D(color.redF() * lightColor.redF(),
                     color.greenF() * lightColor.greenF(),
                     color.blueF() * lightColor.blueF(),
                     color.alphaF() * lightColor.alphaF());
}
#endif

void QGLShaderProgramEffectPrivate::setMaterial
    (const QGLMaterial *mparams, const QGLLightModel *model,
     const QGLLightParameters *lparams, const char *array, int index)
{
    if (lparams) {
        setUniformValue
            (array, index, "ambient",
             colorToVector4(mparams->ambientColor(), lparams->ambientColor()));
        setUniformValue
            (array, index, "diffuse",
             colorToVector4(mparams->diffuseColor(), lparams->diffuseColor()));
        setUniformValue
            (array, index, "specular",
             colorToVector4(mparams->specularColor(), lparams->specularColor()));
    } else {
        setUniformValue
            (array, index, "ambient", mparams->ambientColor());
        setUniformValue
            (array, index, "diffuse", mparams->diffuseColor());
        setUniformValue
            (array, index, "specular", mparams->specularColor());
    }
    setUniformValue
        (array, index, "emission",
         colorToVector4(mparams->emittedLight()) +
         colorToVector4(mparams->ambientColor(), model->ambientSceneColor()));
    setUniformValue
        (array, index, "shininess", GLfloat(mparams->shininess()));
}

#endif // !QGL_FIXED_FUNCTION_ONLY

/*!
    Constructs a new shader program effect.  This constructor is typically
    followed by calls to setVertexShader() and setFragmentShader().

    Note that a shader program effect will be bound to the QOpenGLContext that
    is current when setActive() is called for the first time.  After that,
    the effect can only be used with that context or any other QOpenGLContext
    that shares with it.
*/
QGLShaderProgramEffect::QGLShaderProgramEffect()
    : d_ptr(new QGLShaderProgramEffectPrivate)
{
}

/*!
    Destroys this shader program effect.
*/
QGLShaderProgramEffect::~QGLShaderProgramEffect()
{
}

/*!
    \reimp
*/
void QGLShaderProgramEffect::setActive(QGLPainter *painter, bool flag)
{
    Q_D(QGLShaderProgramEffect);

#if !defined(QGL_SHADERS_ONLY)
    d->fixedFunction = painter->isFixedFunction();
    if (d->fixedFunction) {
        // Fixed function emulation is flat color only.
        if (flag)
            glEnableClientState(GL_VERTEX_ARRAY);
        else
            glDisableClientState(GL_VERTEX_ARRAY);
        return;
    }
#endif

#if !defined(QGL_FIXED_FUNCTION_ONLY)
    static const char *const attributes[] = {
        "qt_Vertex",
        "qt_Normal",
        "qt_Color",
        "qt_MultiTexCoord0",
        "qt_MultiTexCoord1",
        "qt_MultiTexCoord2",
        "qt_Custom0",
        "qt_Custom1"
    };
    const int numAttributes = 8;
    Q_UNUSED(painter);
    int attr;
    if (d->regenerate) {
        // The shader source has changed since the last call to setActive().
        delete d->program;
        d->program = 0;
        d->regenerate = false;
    }
    if (!d->program) {
        if (!flag)
            return;
        Q_ASSERT(!d->vertexShader.isEmpty());
        Q_ASSERT(!d->fragmentShader.isEmpty());
        d->program = new QOpenGLShaderProgram;
        d->program->addShaderFromSourceCode
            (QOpenGLShader::Vertex, d->vertexShader);
        d->program->addShaderFromSourceCode
            (QOpenGLShader::Fragment, d->fragmentShader);

        if (beforeLink()) {
            for (attr = 0; attr < numAttributes; ++attr)
                d->program->bindAttributeLocation(attributes[attr], attr);
        }
        if (!d->program->link()) {
            qWarning("QGLShaderProgramEffect::setActive(): could not link shader program");
            delete d->program;
            d->program = 0;
            return;
        }
        afterLink();
        d->attributes = 0;
        for (attr = 0; attr < numAttributes; ++attr) {
            // Determine which attributes were actually present in the program.
            if (d->program->attributeLocation(attributes[attr]) != -1)
                d->attributes |= (1 << attr);
        }
        if (d->program->attributeLocation("qgl_Vertex") != -1)
            qWarning("QGLShaderProgramEffect: qgl_Vertex no longer supported; use qt_Vertex instead");
        d->matrix = d->program->uniformLocation("qt_ModelViewProjectionMatrix");
        d->mvMatrix = d->program->uniformLocation("qt_ModelViewMatrix");
        d->projMatrix = d->program->uniformLocation("qt_ProjectionMatrix");
        d->normalMatrix = d->program->uniformLocation("qt_NormalMatrix");
        d->worldMatrix = d->program->uniformLocation("qt_WorldMatrix");
        d->texture0 = d->program->uniformLocation("qt_Texture0");
        d->texture1 = d->program->uniformLocation("qt_Texture1");
        d->texture2 = d->program->uniformLocation("qt_Texture2");
        d->color = d->program->uniformLocation("qt_Color");
        d->numLights = d->program->uniformLocation("qt_NumLights");
        d->haveLight =
            (d->program->uniformLocation("qt_Light.position") != -1);
        d->haveLights =
            (d->program->uniformLocation("qt_Lights[0].position") != -1);
        d->haveMaterial =
            (d->program->uniformLocation("qt_Material.diffuse") != -1);
        d->haveMaterials =
            (d->program->uniformLocation("qt_Materials[0].diffuse") != -1);
    }
    if (flag) {
        d->program->bind();
        for (attr = 0; attr < numAttributes; ++attr) {
            if ((d->attributes & (1 << attr)) == 0)
                continue;
            d->program->enableAttributeArray(attr);
        }
        if (d->texture0 != -1)
            d->program->setUniformValue(d->texture0, 0);
        if (d->texture1 != -1)
            d->program->setUniformValue(d->texture1, 1);
        if (d->texture2 != -1)
            d->program->setUniformValue(d->texture2, 2);
    } else {
        for (attr = 0; attr < int(QGL::UserVertex); ++attr) {
            if ((d->attributes & (1 << attr)) != 0)
                d->program->disableAttributeArray(attr);
        }
        d->program->release();
    }
#endif
}

/*!
    \reimp
*/
void QGLShaderProgramEffect::update(QGLPainter *painter, QGLPainter::Updates updates)
{
    Q_D(QGLShaderProgramEffect);
#if !defined(QGL_SHADERS_ONLY)
    if (d->fixedFunction) {
        // Fixed function emulation is flat color only.
        painter->updateFixedFunction
            (updates & (QGLPainter::UpdateColor | QGLPainter::UpdateMatrices));
        return;
    }
#endif
#if !defined(QGL_FIXED_FUNCTION_ONLY)
    if (!d->program)
        return;
    if ((updates & QGLPainter::UpdateColor) != 0 && d->color != -1)
        d->program->setUniformValue(d->color, painter->color());
    if ((updates & QGLPainter::UpdateMatrices) != 0) {
        if (d->matrix != -1)
            d->program->setUniformValue(d->matrix, painter->combinedMatrix());
    }
    if ((updates & QGLPainter::UpdateModelViewMatrix) != 0) {
        if (d->mvMatrix != -1)
            d->program->setUniformValue(d->mvMatrix, painter->modelViewMatrix());
        if (d->normalMatrix != -1)
            d->program->setUniformValue(d->normalMatrix, painter->normalMatrix());
        if (d->worldMatrix != -1)
            d->program->setUniformValue(d->worldMatrix, painter->worldMatrix());
    }
    if ((updates & QGLPainter::UpdateProjectionMatrix) != 0) {
        if (d->projMatrix != -1)
            d->program->setUniformValue(d->projMatrix, painter->projectionMatrix());
    }
    if ((updates & QGLPainter::UpdateLights) != 0) {
        if (d->haveLight) {
            // Only one light needed so make it the main light.
            d->setLight(painter->mainLight(), painter->mainLightTransform(),
                        "qt_Light", -1);
        } else if (d->haveLights) {
            // Shader supports multiple light sources.
            int numLights = 0;
            int maxLightId = painter->maximumLightId();
            if (maxLightId < 0) {
                // No lights - re-enable the main light so we have something.
                painter->mainLight();
                maxLightId = 0;
            }
            for (int lightId = 0; lightId <= maxLightId; ++lightId) {
                // Is this light currently enabled?
                const QGLLightParameters *lparams = painter->light(lightId);
                if (!lparams)
                    continue;

                // Set the parameters for the next shader light number.
                d->setLight(lparams, painter->lightTransform(lightId),
                            "qt_Lights", numLights);

                // Bail out if we've hit the maximum shader light limit.
                ++numLights;
                if (numLights >= d->maximumLights)
                    break;
            }
            if (d->numLights != -1)
                d->program->setUniformValue(d->numLights, numLights);
        }
    }
    if ((updates & QGLPainter::UpdateMaterials) != 0 ||
            ((updates & QGLPainter::UpdateLights) != 0 && d->haveLight)) {
        if (d->haveLight) {
            // For a single light source, combine the light colors
            // into the material colors.
            if (d->haveMaterial) {
                d->setMaterial(painter->faceMaterial(QGL::FrontFaces),
                               painter->lightModel(), painter->mainLight(),
                               "qt_Material", -1);
            } else if (d->haveMaterials) {
                d->setMaterial(painter->faceMaterial(QGL::FrontFaces),
                               painter->lightModel(), painter->mainLight(),
                               "qt_Materials", 0);
                d->setMaterial(painter->faceMaterial(QGL::BackFaces),
                               painter->lightModel(), painter->mainLight(),
                               "qt_Materials", 1);
            }
        } else {
            // Multiple light sources, so light colors are separate.
            if (d->haveMaterial) {
                d->setMaterial(painter->faceMaterial(QGL::FrontFaces),
                               painter->lightModel(), 0, "qt_Material", -1);
            } else if (d->haveMaterials) {
                d->setMaterial(painter->faceMaterial(QGL::FrontFaces),
                               painter->lightModel(), 0, "qt_Materials", 0);
                d->setMaterial(painter->faceMaterial(QGL::BackFaces),
                               painter->lightModel(), 0, "qt_Materials", 1);
            }
        }
    }
#endif
}

/*!
    Returns the source code for the vertex shader.

    \sa setVertexShader(), geometryShader(), fragmentShader(), setVertexShaderFromFile()
*/
QByteArray QGLShaderProgramEffect::vertexShader() const
{
    Q_D(const QGLShaderProgramEffect);
    return d->vertexShader;
}

/*!
    Sets the \a source code for the vertex shader.

    \sa vertexShader(), setGeometryShader(), setFragmentShader(), setVertexShaderFromFile()
*/
void QGLShaderProgramEffect::setVertexShader(const QByteArray &source)
{
    Q_D(QGLShaderProgramEffect);
    d->vertexShader = source;
    d->regenerate = true;
}

/*!
    Sets the source code for the vertex shader to the contents
    of \a fileName.

    \sa setVertexShader(), setGeometryShaderFromFile(), setFragmentShaderFromFile()
*/
void QGLShaderProgramEffect::setVertexShaderFromFile(const QString &fileName)
{
    Q_D(QGLShaderProgramEffect);
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        d->vertexShader = file.readAll();
        d->regenerate = true;
    } else {
        qWarning() << "QGLShaderProgramEffect::setVertexShaderFromFile: could not open " << fileName;
    }
}

/*!
    Returns the source code for the geometry shader.

    \sa setGeometryShader(), fragmentShader(), setGeometryShaderFromFile()
*/
QByteArray QGLShaderProgramEffect::geometryShader() const
{
    Q_D(const QGLShaderProgramEffect);
    return d->geometryShader;
}

/*!
    Sets the \a source code for the geometry shader.

    \sa geometryShader(), setFragmentShader(), setGeometryShaderFromFile()
*/
void QGLShaderProgramEffect::setGeometryShader(const QByteArray &source)
{
    Q_D(QGLShaderProgramEffect);
    d->geometryShader = source;
    d->regenerate = true;
}

/*!
    Sets the source code for the geometry shader to the contents
    of \a fileName.

    \sa setGeometryShader(), setFragmentShaderFromFile()
*/
void QGLShaderProgramEffect::setGeometryShaderFromFile(const QString &fileName)
{
    Q_D(QGLShaderProgramEffect);
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        d->geometryShader = file.readAll();
        d->regenerate = true;
    } else {
        qWarning() << "QGLShaderProgramEffect::setGeometryShaderFromFile: could not open " << fileName;
    }
}

/*!
    Returns the source code for the fragment shader.

    \sa setFragmentShader(), vertexShader(), geometryShader()
*/
QByteArray QGLShaderProgramEffect::fragmentShader() const
{
    Q_D(const QGLShaderProgramEffect);
    return d->fragmentShader;
}

/*!
    Sets the source code for the fragment shader to the contents
    of \a fileName.

    \sa setFragmentShader(), setVertexShaderFromFile(), setGeometryShaderFromFile()
*/
void QGLShaderProgramEffect::setFragmentShaderFromFile(const QString &fileName)
{
    Q_D(QGLShaderProgramEffect);
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        d->fragmentShader = file.readAll();
        d->regenerate = true;
    } else {
        qWarning() << "QGLShaderProgramEffect::setFragmentShaderFromFile: could not open " << fileName;
    }
}

/*!
    Sets the \a source code for the fragment shader.

    \sa fragmentShader(), setVertexShader(), setGeometryShader()
*/
void QGLShaderProgramEffect::setFragmentShader(const QByteArray &source)
{
    Q_D(QGLShaderProgramEffect);
    d->fragmentShader = source;
    d->regenerate = true;
}

/*!
    Returns the maximum number of lights that are supported by this
    shader program effect.  The default value is 8.

    The actual number of lights will be provided to the vertexShader()
    as the \c{qt_NumLights} uniform variable, which will always be
    less than or equal to maximumLights().

    \sa setMaximumLights()
*/
int QGLShaderProgramEffect::maximumLights() const
{
    Q_D(const QGLShaderProgramEffect);
    return d->maximumLights;
}

/*!
    Sets the maximum number of lights that are supported by this
    shader program effect to \a value.

    \sa maximumLights()
*/
void QGLShaderProgramEffect::setMaximumLights(int value)
{
    Q_D(QGLShaderProgramEffect);
    d->maximumLights = value;
}

/*!
    Returns the shader program object that was created for this effect;
    null if setActive() has not been called yet.

    This function can be used by the application to adjust custom
    uniform variables after the effect is activated on a QGLPainter:

    \code
    painter.setUserEffect(effect);
    effect->program()->setUniformValue("springiness", GLfloat(0.5f));
    \endcode
*/
QOpenGLShaderProgram *QGLShaderProgramEffect::program() const
{
#if !defined(QGL_FIXED_FUNCTION_ONLY)
    Q_D(const QGLShaderProgramEffect);
    return d->program;
#else
    return 0;
#endif
}

/*!
    Called by setActive() just before the program() is linked.
    Returns true if the standard vertex attributes should be bound
    by calls to QGLShaderProgram::bindAttributeLocation().  Returns
    false if the subclass has already bound the attributes.

    This function can be overridden by subclasses to alter the
    vertex attribute bindings, or to add additional shader stages
    to program().

    \sa afterLink()
*/
bool QGLShaderProgramEffect::beforeLink()
{
    return true;
}

/*!
    Called by setActive() just after the program() is linked.
    The default implementation does nothing.

    This function can be overridden by subclasses to resolve uniform
    variable locations and cache them for later use in update().

    \sa beforeLink()
*/
void QGLShaderProgramEffect::afterLink()
{
}

QT_END_NAMESPACE
