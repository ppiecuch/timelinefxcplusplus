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

#include "qglmaterial.h"
#include "qglmaterial_p.h"
#include "qglpainter.h"
#include "qgltexture2d.h"

#include <qfileinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \class QGLAbstractMaterial
    \since 4.8
    \brief The QGLAbstractMaterial class provides a standard interface for rendering surface materials with GL.
    \ingroup qt3d
    \ingroup qt3d::materials

    Materials are the primary method to specify the surface appearance of an
    object, as distinct from the geometry for the object.  Materials have an
    almost endless variety of parameters:

    \list
    \li Properties of the material under various lighting conditions;
       i.e., the traditional parameters for ambient, diffuse, specular, etc.
    \li Textures in multiple layers, with different combination modes;
       decal, modulate, replace, etc.
    \li Environmental conditions such as fogging.
    \li Alpha values for opacity and blending.
    \li Interpolation factors for animated surface effects.
    \li etc
    \endlist

    QGLAbstractMaterial is the base class for all such materials.
    It provides a very simple API to bind() a material to a QGLPainter
    when the material needs to be rendered, to release() a material
    from a QGLPainter when the material is no longer needed, and to
    prepareToDraw().

    Subclasses of QGLAbstractMaterial implement specific materials.
    QGLMaterial provides the traditional ambient, diffuse, specular, etc
    parameters for lighting properties.

    Materials are distinct from \e effects, which have the base class
    QGLAbstractEffect.  Effects are typically shader programs that are
    used to render a specific type of material.  The material's bind()
    function will use QGLPainter::setStandardEffect() or
    QGLPainter::setUserEffect() to select the exact effect that is
    needed to render the material.

    It is possible that the same material may be rendered with different
    effects depending upon the material parameters.  For example, a lit
    material may select a simpler and more efficient shader program effect
    if the material has the default spotlight properties, or if the
    QGLPainter only has a single light source specified.

    \sa QGLMaterial, QGLAbstractEffect
*/

/*!
    Constructs a new material and attaches it to \a parent.
*/
QGLAbstractMaterial::QGLAbstractMaterial(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destroys this material.
*/
QGLAbstractMaterial::~QGLAbstractMaterial()
{
}

/*!
    Returns the material lighting parameters for rendering the front
    faces of fragments with this abstract material.  The default
    implementation returns null.

    This function is provided as a convenience for applications that
    wish to alter the lighting parameters or textures of a material,
    without regard to any wrapping that has been performed to add
    blending or other options.

    \sa back(), QGLMaterial
*/
QGLMaterial *QGLAbstractMaterial::front() const
{
    return 0;
}

/*!
    Returns the material lighting parameters for rendering the back
    faces of fragments with this abstract material.  The default
    implementation returns null, which indicates that front()
    is also used to render back faces.

    \sa front(), QGLMaterial
*/
QGLMaterial *QGLAbstractMaterial::back() const
{
    return 0;
}

/*!
    \fn bool QGLAbstractMaterial::isTransparent() const

    Returns true if this material is transparent and will therefore
    require the \c{GL_BLEND} mode to be enabled to render the material.
    Returns false if the material is fully opaque.
*/

/*!
    \fn void QGLAbstractMaterial::bind(QGLPainter *painter)

    Binds resources to \a painter that are needed to render this
    material; textures, shader programs, blending modes, etc.

    In the following example, lit material parameters and a texture
    are bound to the \a painter, for rendering with the standard
    QGL::LitModulateTexture2D effect:

    \code
    void TexturedLitMaterial::bind(QGLPainter *painter)
    {
        painter->setStandardEffect(QGL::LitModulateTexture2D);
        painter->setFaceMaterial(QGL::AllFaces, material);
        painter->glActiveTexture(GL_TEXTURE0);
        texture->bind();
    }
    \endcode

    Normally the effect is bound to \a painter during the bind()
    function.  However, some materials may need to use different
    effects depending upon which attributes are present in the
    geometry.  For example, a per-vertex color effect instead of a
    uniform color effect if the geometry has the QGL::Color attribute.
    The prepareToDraw() function can be overridden to alter the
    effect once the specific set of geometry attributes are known.

    \sa release(), prepareToDraw()
*/

/*!
    \fn void QGLAbstractMaterial::release(QGLPainter *painter, QGLAbstractMaterial *next)

    Releases resources from \a painter that were used to render this
    material.  The QGLPainter::effect() can be left bound to \a painter,
    but other resources such as textures and blending modes
    should be disabled.

    If \a next is not null, then it indicates the next material that
    will be bound to \a painter.  If \a next is the same type of
    material as this material, then this function can choose not to
    release resources that would be immediately re-bound to
    \a painter in the next call to bind().

    In the following example, texture unit 0 is unbound if \a painter
    is about to switch to another effect that is not an instance
    of \c{TexturedLitMaterial}:

    \code
    void TexturedLitMaterial::release(QGLPainter *painter, QGLAbstractMaterial *next)
    {
        if (!qobject_cast<TexturedLitMaterial *>(next)) {
            painter->glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    \endcode

    \sa bind(), prepareToDraw()
*/

/*!
    Prepares to draw geometry to \a painter that has the specified
    set of vertex \a attributes.  The default implementation
    does nothing.

    Multiple effects may be used to render some materials depending upon
    the available vertex attributes.  For example, if QGL::Color is
    present in \a attributes, then a per-vertex color should be used
    instead of a single uniform color.

    This function is provided for such materials to have one last
    chance during QGLPainter::draw() to alter the QGLPainter::effect()
    to something that is tuned for the specific geometry.  It can
    be assumed that bind() has already been called on this material.

    \sa bind(), release()
*/
void QGLAbstractMaterial::prepareToDraw(QGLPainter *painter, const QGLAttributeSet &attributes)
{
    Q_UNUSED(painter);
    Q_UNUSED(attributes);
}

/*!
    \fn void QGLAbstractMaterial::materialChanged()

    Signal that is emitted when an object that is using this material
    should be redrawn because some property on the material has changed.
*/



/*!
    \class QGLMaterial
    \brief The QGLMaterial class describes one-sided material properties for rendering fragments.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::materials

    \sa QGLTwoSidedMaterial
*/

/*!
    \qmltype Material
    \instantiates QGLMaterial
    \brief The Material item describes material properties for OpenGL drawing.
    \since 4.8
    \ingroup qt3d::qml3d
*/

QGLMaterialPrivate::QGLMaterialPrivate()
    : specularColor(0, 0, 0, 255),
      emittedLight(0, 0, 0, 255),
      shininess(0),
      collection(0),
      index(-1),
      used(false)
{
    ambientColor.setRgbF(0.2f, 0.2f, 0.2f, 1.0f);
    diffuseColor.setRgbF(0.8f, 0.8f, 0.8f, 1.0f);
}

/*!
    Constructs a QGLMaterial object with its default values,
    and attaches it to \a parent.
*/
QGLMaterial::QGLMaterial(QObject *parent)
    : QGLAbstractMaterial(parent)
    , d_ptr(new QGLMaterialPrivate)
{
}

/*!
    Destroys this QGLMaterial object.
*/
QGLMaterial::~QGLMaterial()
{
}

/*!
    \property QGLMaterial::ambientColor
    \brief the ambient color of the material.  The default value
    is (0.2, 0.2, 0.2, 1.0).

    \sa diffuseColor(), specularColor(), ambientColorChanged()
*/

/*!
    \qmlproperty color Material::ambientColor
    The ambient color of the material.  The default value
    is (0.2, 0.2, 0.2, 1.0).

    \sa diffuseColor, specularColor
*/

QColor QGLMaterial::ambientColor() const
{
    Q_D(const QGLMaterial);
    return d->ambientColor;
}

void QGLMaterial::setAmbientColor(const QColor& value)
{
    Q_D(QGLMaterial);
    if (d->ambientColor != value) {
        d->ambientColor = value;
        emit ambientColorChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLMaterial::diffuseColor
    \brief the diffuse color of the material.  The default value
    is (0.8, 0.8, 0.8, 1.0).

    \sa ambientColor(), specularColor(), diffuseColorChanged()
*/

/*!
    \qmlproperty color Material::diffuseColor
    The diffuse color of the material.  The default value
    is (0.8, 0.8, 0.8, 1.0).

    \sa ambientColor, specularColor
*/

QColor QGLMaterial::diffuseColor() const
{
    Q_D(const QGLMaterial);
    return d->diffuseColor;
}

void QGLMaterial::setDiffuseColor(const QColor& value)
{
    Q_D(QGLMaterial);
    if (d->diffuseColor != value) {
        d->diffuseColor = value;
        emit diffuseColorChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLMaterial::specularColor
    \brief the specular color of the material.  The default value
    is (0, 0, 0, 1).

    \sa ambientColor(), diffuseColor(), specularColorChanged()
*/

/*!
    \qmlproperty color Material::specularColor
    The specular color of the material.  The default value
    is (0, 0, 0, 1).

    \sa ambientColor, diffuseColor
*/

QColor QGLMaterial::specularColor() const
{
    Q_D(const QGLMaterial);
    return d->specularColor;
}

void QGLMaterial::setSpecularColor(const QColor& value)
{
    Q_D(QGLMaterial);
    if (d->specularColor != value) {
        d->specularColor = value;
        emit specularColorChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLMaterial::emittedLight
    \brief the emitted light intensity of the material.
    The default value is (0.0, 0.0, 0.0, 1.0), which indicates
    that the material does not emit any light.

    \sa emittedLightChanged()
*/

/*!
    \qmlproperty color Material::emittedLight
    The emitted light intensity of the material.
    The default value is (0.0, 0.0, 0.0, 1.0), which indicates
    that the material does not emit any light.
*/

QColor QGLMaterial::emittedLight() const
{
    Q_D(const QGLMaterial);
    return d->emittedLight;
}

void QGLMaterial::setEmittedLight(const QColor& value)
{
    Q_D(QGLMaterial);
    if (d->emittedLight != value) {
        d->emittedLight = value;
        emit emittedLightChanged();
        emit materialChanged();
    }
}

/*!
    Sets ambientColor() to 20% of \a value, and diffuseColor() to 80% of
    \a value.  This is a convenience function for quickly setting ambient
    and diffuse lighting colors based on a flat color.

    \sa ambientColor(), diffuseColor()
*/
void QGLMaterial::setColor(const QColor& value)
{
    Q_D(QGLMaterial);
    d->ambientColor.setRgbF
        (value.redF() * 0.2f, value.greenF() * 0.2f,
         value.blueF() * 0.2f, value.alphaF());
    d->diffuseColor.setRgbF
        (value.redF() * 0.8f, value.greenF() * 0.8f,
         value.blueF() * 0.8f, value.alphaF());
    emit ambientColorChanged();
    emit diffuseColorChanged();
    emit materialChanged();
}

/*!
    \property QGLMaterial::shininess
    \brief the specular exponent of the material, or how shiny it is.
    Must be between 0 and 128.  The default value is 0.  A value outside
    this range will be clamped to the range when the property is set.

    \sa shininessChanged()
*/

/*!
    \qmlproperty real Material::shininess
    The specular exponent of the material, or how shiny it is.
    Must be between 0 and 128.  The default value is 0.  A value outside
    this range will be clamped to the range when the property is set.
*/

float QGLMaterial::shininess() const
{
    Q_D(const QGLMaterial);
    return d->shininess;
}

void QGLMaterial::setShininess(float value)
{
    Q_D(QGLMaterial);
    if (value < 0)
        value = 0;
    else if (value > 128)
        value = 128;
    if (d->shininess != value) {
        d->shininess = value;
        emit shininessChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLMaterial::texture
    \brief the 2D texture associated with \a layer on this material;
    null if no texture.

    Layer 0 is normally the primary texture associated with the material.
    Multiple texture layers may be specified for materials with special
    blending effects or to specify ambient, diffuse, or specular colors
    pixel-by-pixel.

    \sa texturesChanged()
*/
QGLTexture2D *QGLMaterial::texture(int layer) const
{
    Q_D(const QGLMaterial);
    return d->textures.value(layer, 0);
}

void QGLMaterial::setTexture(QGLTexture2D *value, int layer)
{
    Q_ASSERT(layer >= 0);
    Q_D(QGLMaterial);
    QGLTexture2D *prev = d->textures.value(layer, 0);
    if (prev != value) {
        if (prev) {
            prev->cleanupResources();
        }
        delete prev;
        d->textures[layer] = value;
        connect(value, SIGNAL(textureUpdated()), this, SIGNAL(texturesChanged()));
        connect(value, SIGNAL(textureUpdated()), this, SIGNAL(materialChanged()));
        emit texturesChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLMaterial::textureUrl
    \brief URL of the 2D texture associated with \a layer on this material.

    By default \a layer is 0, the primary texture.

    If the URL has not been specified, then this property is a null QUrl.

    Setting this property to a non-empty URL will replace any existing texture
    with a new texture based on the image at the given \a url.  If that
    image is not a valid texture then the new texture will be a null texture.

    If an empty url is set, this has the same effect as \c{setTexture(0)}.

    \sa texture(), setTexture()
*/
QUrl QGLMaterial::textureUrl(int layer) const
{
    Q_D(const QGLMaterial);
    QGLTexture2D *tex = d->textures.value(layer, 0);
    if (tex)
        return tex->url();
    else
        return QUrl();
}

void QGLMaterial::setTextureUrl(const QUrl &url, int layer)
{
    Q_ASSERT(layer >= 0);
    if (textureUrl(layer) != url)
    {
        QGLTexture2D *tex = 0;
        if (!url.isEmpty())
        {
            tex = new QGLTexture2D(this);
            connect(tex, SIGNAL(textureUpdated()), this, SIGNAL(texturesChanged()));
            connect(tex, SIGNAL(textureUpdated()), this, SIGNAL(materialChanged()));
            tex->setUrl(url);
        }
        setTexture(tex, layer);
    }
}

/*!
    \enum QGLMaterial::TextureCombineMode
    This enum defines the mode to use when combining a texture with
    the material colors on a QGLMaterial object.

    \value Modulate Modulate the texture with the lighting
           conditions to produce a lit texture.
    \value Decal Combine the texture with the lighting conditions
           to produce a decal effect.
    \value Replace Replace with the contents of the texture,
           ignoring colors and lighting conditions.
*/

/*!
    \property QGLMaterial::textureCombineMode
    \brief the texture combine mode associated with \a layer on this material.
    The default value is \l Modulate.

    \sa texturesChanged()
*/

QGLMaterial::TextureCombineMode QGLMaterial::textureCombineMode(int layer) const
{
    Q_D(const QGLMaterial);
    return d->textureModes.value(layer, Modulate);
}

void QGLMaterial::setTextureCombineMode(QGLMaterial::TextureCombineMode mode, int layer)
{
    Q_D(QGLMaterial);
    if (d->textureModes.value(layer, Modulate) != mode) {
        d->textureModes[layer] = mode;
        emit texturesChanged();
        emit materialChanged();
    }
}

/*!
    Returns the number of texture layers associated with this material.

    The return value may be larger than the number of actual texture
    layers if some of the intermediate layers are null.  For example,
    setting layers 0 and 2 will report textureLayerCount() as 3.
    The main use of this value is to iterate over all layers.

    \sa texture()
*/
int QGLMaterial::textureLayerCount() const
{
    Q_D(const QGLMaterial);
    int maxLayer = -1;
    if (!d->textures.isEmpty())
        maxLayer = qMax(maxLayer, (d->textures.end() - 1).key());
    return maxLayer + 1;
}

/*!
    \reimp
    Returns this material.
*/
QGLMaterial *QGLMaterial::front() const
{
    return const_cast<QGLMaterial *>(this);
}

/*!
    \reimp
*/
bool QGLMaterial::isTransparent() const
{
    Q_D(const QGLMaterial);
    bool transparent = (d->diffuseColor.alpha() != 255);
    QMap<int, QGLTexture2D *>::ConstIterator it;
    for (it = d->textures.begin(); it != d->textures.end(); ++it) {
        TextureCombineMode mode = d->textureModes.value(it.key(), Modulate);
        if (mode == Modulate) {
            // Texture alpha adds to the current alpha.
            if (it.value() && it.value()->hasAlphaChannel())
                transparent = true;
        } else if (mode == Replace) {
            // Replace the current alpha with the texture's alpha.
            if (it.value())
                transparent = it.value()->hasAlphaChannel();
        }
    }
    return transparent;
}

/*!
    \reimp
*/
void QGLMaterial::bind(QGLPainter *painter)
{
    painter->setFaceMaterial(QGL::AllFaces, this);
    const_cast<QGLLightModel *>(painter->lightModel())
        ->setModel(QGLLightModel::OneSided); // FIXME
    bindTextures(painter);
}

/*!
    \internal
*/
void QGLMaterial::bindTextures(QGLPainter *painter)
{
    Q_D(const QGLMaterial);
    QMap<int, QGLTexture2D *>::ConstIterator it;
    for (it = d->textures.begin(); it != d->textures.end(); ++it) {
        QGLTexture2D *tex = it.value();
        painter->glActiveTexture(GL_TEXTURE0 + it.key());
        if (tex)
            tex->bind();
        else
            glBindTexture(GL_TEXTURE_2D, 0);
    }
}

/*!
    \reimp
*/
void QGLMaterial::release(QGLPainter *painter, QGLAbstractMaterial *next)
{
    Q_UNUSED(next);
    Q_D(const QGLMaterial);
    QMap<int, QGLTexture2D *>::ConstIterator it;
    for (it = d->textures.begin(); it != d->textures.end(); ++it) {
        painter->glActiveTexture(GL_TEXTURE0 + it.key());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

/*!
    \reimp
*/
void QGLMaterial::prepareToDraw
    (QGLPainter *painter, const QGLAttributeSet &attributes)
{
    bindEffect(painter, attributes, false);
}

/*!
    \internal
*/
void QGLMaterial::bindEffect
    (QGLPainter *painter, const QGLAttributeSet &attributes, bool twoSided)
{
    Q_D(const QGLMaterial);
    Q_UNUSED(twoSided);
    QGL::StandardEffect effect = QGL::LitMaterial;
    if (!d->textures.isEmpty() && attributes.contains(QGL::TextureCoord0)) {
        // TODO: different combine modes for each layer.
        QGLMaterial::TextureCombineMode mode =
            d->textureModes.value(0, Modulate);
        if (mode == Replace)
            effect = QGL::FlatReplaceTexture2D;
        else if (mode == Decal)
            effect = QGL::LitDecalTexture2D;
        else
            effect = QGL::LitModulateTexture2D;
    }
    painter->setStandardEffect(effect);
}

/*!
    \fn void QGLMaterial::ambientColorChanged()

    This signal is emitted when ambientColor() changes.

    \sa ambientColor(), setAmbientColor(), materialChanged()
*/

/*!
    \fn void QGLMaterial::diffuseColorChanged()

    This signal is emitted when diffuseColor() changes.

    \sa diffuseColor(), setDiffuseColor(), materialChanged()
*/

/*!
    \fn void QGLMaterial::specularColorChanged()

    This signal is emitted when specularColor() changes.

    \sa specularColor(), setSpecularColor(), materialChanged()
*/

/*!
    \fn void QGLMaterial::emittedLightChanged()

    This signal is emitted when emittedLight() changes.

    \sa emittedLight(), setEmittedLight(), materialChanged()
*/

/*!
    \fn void QGLMaterial::shininessChanged()

    This signal is emitted when shininess() changes.

    \sa shininess(), setShininess(), materialChanged()
*/

/*!
    \fn void QGLMaterial::texturesChanged()

    This signal is emitted when the texture layers associated with
    this material change.

    \sa texture(), setTexture(), materialChanged()
*/

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug dbg, const QGLMaterial &material)
{
    dbg << &material <<
            "-- Amb:" << material.ambientColor() <<
            "-- Diff:" << material.diffuseColor() <<
            "-- Spec:" << material.specularColor() <<
            "-- Shin:" << material.shininess();
    for (int i = 0; i < material.textureLayerCount(); ++i)
        if (material.texture(i) != 0)
            dbg << "\n    -- Tex" << i << ":" << material.texture(i)
            << material.texture(i)->objectName();
    dbg << "\n";
    return dbg;
}

#endif



/*!
    \class QGLColorMaterial
    \since 4.8
    \brief The QGLColorMaterial class implements flat or per-vertex color materials for 3D rendering.
    \ingroup qt3d
    \ingroup qt3d::materials

    When bound to a QGLPainter, QGLColorMaterial will select a flat
    color drawing effect, to render fragments with color(), ignoring
    any lights or textures that may be active on the QGLPainter state.
    If the geometry has the QGL::Color attribute, then a per-vertex
    color will be used instead and color() is ignored.

    \sa QGLMaterial
*/

class QGLColorMaterialPrivate
{
public:
    QGLColorMaterialPrivate() : color(255, 255, 255, 255) {}

    QColor color;
};

/*!
    Constructs a new flat color material and attaches it to \a parent.
*/
QGLColorMaterial::QGLColorMaterial(QObject *parent)
    : QGLAbstractMaterial(parent)
    , d_ptr(new QGLColorMaterialPrivate)
{
}

/*!
    Destroys this flat color material.
*/
QGLColorMaterial::~QGLColorMaterial()
{
}

/*!
    \property QGLColorMaterial::color
    \brief the flat color to use to render the material.  The default
    color is white.

    If the geometry has per-vertex colors, then this property is ignored.

    \sa colorChanged()
*/

QColor QGLColorMaterial::color() const
{
    Q_D(const QGLColorMaterial);
    return d->color;
}

void QGLColorMaterial::setColor(const QColor &color)
{
    Q_D(QGLColorMaterial);
    if (d->color != color) {
        d->color = color;
        emit colorChanged();
        emit materialChanged();
    }
}

/*!
    \reimp
*/
bool QGLColorMaterial::isTransparent() const
{
    Q_D(const QGLColorMaterial);
    return d->color.alpha() != 255;
}

/*!
    \reimp
*/
void QGLColorMaterial::bind(QGLPainter *painter)
{
    Q_D(const QGLColorMaterial);
    painter->setColor(d->color);
    // Effect is set during prepareToDraw().
}

/*!
    \reimp
*/
void QGLColorMaterial::release(QGLPainter *painter, QGLAbstractMaterial *next)
{
    // No textures or other modes, so nothing to do here.
    Q_UNUSED(painter);
    Q_UNUSED(next);
}

/*!
    \reimp
*/
void QGLColorMaterial::prepareToDraw
    (QGLPainter *painter, const QGLAttributeSet &attributes)
{
    if (attributes.contains(QGL::Color))
        painter->setStandardEffect(QGL::FlatPerVertexColor);
    else
        painter->setStandardEffect(QGL::FlatColor);
}

/*!
    \fn void QGLColorMaterial::colorChanged()

    Signal that is emitted when color() changes.

    \sa color()
*/



/*!
    \class QGLTwoSidedMaterial
    \since 4.8
    \brief The QGLTwoSidedMaterial class implemented two-sided materials for 3D rendering.
    \ingroup qt3d
    \ingroup qt3d::materials

    Two-sided materials consist of a front() material and a back()
    material.  The specific material rendered is determined by the
    direction faced by a fragment when it is rendered.  Fragments
    that point towards the viewer are rendered with front(),
    and fragments that point away from the viewer are rendered
    with back().  In both cases, any textures that are used to
    render the material are taken from front().

    If front() and back() are the same, then the same material
    will be used on both sides of the fragment.  However, this
    is not exactly the same as using a one-sided QGLMaterial in
    place of the two-sided material.  One-sided materials will
    render the back of the fragment as black because the normal
    is always pointing away from the viewer.  Two-sided materials
    reverse the back-facing normal so that back() is lit as
    though it was on a front-facing face.

    \sa QGLMaterial
*/

class QGLTwoSidedMaterialPrivate
{
public:
    QGLTwoSidedMaterialPrivate() : front(0), back(0), defaultMaterial(0) {}

    QGLMaterial *front;
    QGLMaterial *back;
    QGLMaterial *defaultMaterial;
};

/*!
    Constructs a two-sided material object and attaches it to \a parent.

    \sa setFront(), setBack()
*/
QGLTwoSidedMaterial::QGLTwoSidedMaterial(QObject *parent)
    : QGLAbstractMaterial(parent)
    , d_ptr(new QGLTwoSidedMaterialPrivate)
{
}

/*!
    Destroys this two-sided material object.
*/
QGLTwoSidedMaterial::~QGLTwoSidedMaterial()
{
}

/*!
    \property QGLTwoSidedMaterial::front
    \brief the material for the front side of the object's fragments.

    The default value is null, which will result in a default QGLMaterial
    object being used when bind() is called.

    \sa back(), frontChanged(), materialChanged()
*/

QGLMaterial *QGLTwoSidedMaterial::front() const
{
    Q_D(const QGLTwoSidedMaterial);
    return d->front;
}

void QGLTwoSidedMaterial::setFront(QGLMaterial *material)
{
    Q_D(QGLTwoSidedMaterial);
    if (d->front != material) {
        if (d->front && d->front != d->back) {
            disconnect(d->front, SIGNAL(materialChanged()),
                       this, SIGNAL(materialChanged()));
        }
        d->front = material;
        if (d->front && d->front != d->back) {
            connect(d->front, SIGNAL(materialChanged()),
                    this, SIGNAL(materialChanged()));
        }
        emit frontChanged();
        emit materialChanged();
    }
}

/*!
    \property QGLTwoSidedMaterial::back
    \brief the material for the back side of the object's fragments.

    The default value is null, which indicates that front() should
    be used on both the front and back sides of the object's fragments.

    Textures are taken from the front() material.  Any textures that
    appear in the back() material are ignored.  Only the material
    lighting parameters from back() will be used.

    \sa front(), backChanged(), materialChanged()
*/

QGLMaterial *QGLTwoSidedMaterial::back() const
{
    Q_D(const QGLTwoSidedMaterial);
    return d->back;
}

void QGLTwoSidedMaterial::setBack(QGLMaterial *material)
{
    Q_D(QGLTwoSidedMaterial);
    if (d->back != material) {
        if (d->back && d->back != d->front) {
            disconnect(d->back, SIGNAL(materialChanged()),
                       this, SIGNAL(materialChanged()));
        }
        d->back = material;
        if (d->back && d->back != d->front) {
            connect(d->back, SIGNAL(materialChanged()),
                    this, SIGNAL(materialChanged()));
        }
        emit backChanged();
        emit materialChanged();
    }
}

/*!
    \reimp
*/
bool QGLTwoSidedMaterial::isTransparent() const
{
    Q_D(const QGLTwoSidedMaterial);
    if (d->front && d->front->isTransparent())
        return true;
    return d->back && d->back->isTransparent();
}

/*!
    \reimp
*/
void QGLTwoSidedMaterial::bind(QGLPainter *painter)
{
    Q_D(QGLTwoSidedMaterial);
    QGLMaterial *front = d->front;
    if (!front) {
        // We need to have something for the front material.
        front = d->defaultMaterial;
        if (!front) {
            d->defaultMaterial = new QGLMaterial(this);
            front = d->defaultMaterial;
        }
    }
    const_cast<QGLLightModel *>(painter->lightModel())
        ->setModel(QGLLightModel::TwoSided); // FIXME
    if (d->back && d->back != front) {
        painter->setFaceMaterial(QGL::FrontFaces, front);
        painter->setFaceMaterial(QGL::BackFaces, d->back);
    } else {
        painter->setFaceMaterial(QGL::AllFaces, front);
    }
    front->bindTextures(painter);
}

/*!
    \reimp
*/
void QGLTwoSidedMaterial::release(QGLPainter *painter, QGLAbstractMaterial *next)
{
    Q_D(const QGLTwoSidedMaterial);
    if (d->front)
        d->front->release(painter, next);
    else if (d->defaultMaterial)
        d->defaultMaterial->release(painter, next);
}

/*!
    \reimp
*/
void QGLTwoSidedMaterial::prepareToDraw
    (QGLPainter *painter, const QGLAttributeSet &attributes)
{
    Q_D(QGLTwoSidedMaterial);
    QGLMaterial *front = d->front;
    if (!front)
        front = d->defaultMaterial;
    front->bindEffect(painter, attributes, true);
}

/*!
    \fn void QGLTwoSidedMaterial::frontChanged()

    Signal that is emitted when the front() material pointer changes.

    This signal is not emitted when a property on front() changes.
    Use materialChanged() for that purpose instead.

    \sa front(), backChanged()
*/

/*!
    \fn void QGLTwoSidedMaterial::backChanged()

    Signal that is emitted when the back() material pointer changes.

    This signal is not emitted when a property on back() changes.
    Use materialChanged() for that purpose instead.

    \sa back(), frontChanged()
*/


/*!
    \class QGLMaterialCollection
    \brief The QGLMaterialCollection class manages groups of materials.
    \since 4.8
    \ingroup qt3d
    \ingroup qt3d::enablers

    Managing more complex 3d graphics with several materials is easier when the
    materials can be referred to as a collection.  This is the role of the
    QGLMaterialCollection class.

    Plug-ins implementing 3D formats may make the materials defined in
    the format available to the application via a QGLMaterialCollection.

    The collection is also optimised for the case where many small objects
    must refer to materials - such as faces in a mesh, or particles.  In
    this case the materials can be specified as a short data type using an
    offset into the collection, rather than the material name.

    When building up a collection, meshes that refer to the various materials
    can check off which ones are used by calling markMaterialAsUsed(), and then
    remove spurious unused materials by calling removeUnusedMaterials().  This
    technique is suitable for models loaded from a model file where a large
    number of materials may be specified but only a few of those materials
    are used by the particular mesh selected from the scene.

    To make a material available from a collection, call addMaterial().  To
    retrieve a material from the collection call removeMaterial().

    The collection takes ownership of the QGLMaterial
    objects passed to it by the addMaterial() function.  These
    objects will be destroyed when the collection is destroyed.
*/

class QGLMaterialCollectionPrivate
{
public:
    QGLMaterialCollectionPrivate()
    {
    }

    QList<QGLMaterial *> materials;
    QHash<QString, int> materialNames;
};

/*!
    Construct a new empty QGLMaterialCollection object.  The \a parent
    is set as the parent of this object.
*/
QGLMaterialCollection::QGLMaterialCollection(QObject *parent)
    : QObject(parent)
    , d_ptr(new QGLMaterialCollectionPrivate)
{
}

/*!
    Destroy this collection.  All material objects referred to by this
    collection will be destroyed.
*/
QGLMaterialCollection::~QGLMaterialCollection()
{
    // The QGLMaterial QObject's are reparented to the collection
    // when addMaterial() is called, so the QObject destructor
    // will take care of cleaning them up for us.
}

/*!
    Returns a pointer to the material corresponding to \a index; or null
    if \a index is out of range or the material has been removed.

    Here's an example of searching for a material with a given ambient
    \c{color} in the collection \c{materials}:

    \code
    for (int colorIndex; colorIndex < materials->size(); ++colorIndex) {
        if (material(colorIndex) &&
                material(colorIndex)->ambientColor() == color)
            break;
    }
    if (colorIndex < materials->size())
        myObject->setMaterial(colorIndex);
    \endcode
*/
QGLMaterial *QGLMaterialCollection::material(int index) const
{
    Q_D(const QGLMaterialCollection);
    return d->materials.value(index, 0);
}

/*!
    \overload

    Returns the material associated with \a name in this collection;
    null if \a name is not present or the material has been removed.
*/
QGLMaterial *QGLMaterialCollection::material(const QString &name) const
{
    Q_D(const QGLMaterialCollection);
    int index = d->materialNames.value(name, -1);
    if (index >= 0)
        return d->materials[index];
    else
        return 0;
}

/*!
    Returns true if this collection contains \a material; false otherwise.

    \sa indexOf()
*/
bool QGLMaterialCollection::contains(QGLMaterial *material) const
{
    return material && material->d_func()->collection == this;
}

/*!
    \overload

    Returns true if this collection contains a material called \a name;
    false otherwise.

    \sa indexOf()
*/
bool QGLMaterialCollection::contains(const QString &name) const
{
    Q_D(const QGLMaterialCollection);
    return d->materialNames.contains(name);
}

/*!
    Returns the index of \a material in this collection; -1 if
    \a material is not present in this collection.

    \sa contains()
*/
int QGLMaterialCollection::indexOf(QGLMaterial *material) const
{
    if (material && material->d_func()->collection == this)
        return material->d_func()->index;
    else
        return -1;
}

/*!
    \overload

    Returns the index of the material called \a name in this collection;
    -1 if \a name is not present in this collection.

    \sa contains()
*/
int QGLMaterialCollection::indexOf(const QString &name) const
{
    Q_D(const QGLMaterialCollection);
    return d->materialNames.value(name, -1);
}

/*!
    Returns the name of the material at \a index in this material collection;
    a null QString if \a index is out of range.
*/
QString QGLMaterialCollection::materialName(int index) const
{
    Q_D(const QGLMaterialCollection);
    if (index >= 0 && index < d->materials.count()) {
        QGLMaterial *material = d->materials[index];
        if (material) {
            // Use the name in the private data block just in case the
            // application has modified objectName() since adding.
            return material->d_func()->name;
        }
    }
    return QString();
}

/*!
    Returns true if the material at \a index in this collection has been
    marked as used by markMaterialAsUsed().

    \sa markMaterialAsUsed()
*/
bool QGLMaterialCollection::isMaterialUsed(int index) const
{
    QGLMaterial *mat = material(index);
    if (mat)
        return mat->d_func()->used;
    else
        return false;
}

/*!
    Flags the material corresponding to the \a index as used.  Some model files
    may contain a range of materials, applying to various objects in the scene.

    When a particular object is loaded from the file, many of those
    materials may not be used in that object.  This wastes space,
    with many spurious materials being stored.

    Use this method during model loading or construction to mark off
    materials that have been used.  Materials so marked will not
    be removed by removeUnusedMaterials().

    \sa removeUnusedMaterials(), isMaterialUsed()
*/
void QGLMaterialCollection::markMaterialAsUsed(int index)
{
    QGLMaterial *mat = material(index);
    if (mat)
        mat->d_func()->used = true;
}

/*!
    Removes and deletes materials which have not been marked as used.

    \sa markMaterialAsUsed(), isMaterialUsed()
*/
void QGLMaterialCollection::removeUnusedMaterials()
{
    Q_D(QGLMaterialCollection);
    for (int index = 0; index < d->materials.size(); ++index) {
        QGLMaterial *material = d->materials[index];
        if (material && !material->d_func()->used)
            delete removeMaterial(index);
    }
}

/*!
    Adds \a material to this collection and returns its new index.  The
    collection takes ownership of the material and will delete it when the
    collection is destroyed.  Initially the \a material is marked as unused.

    The QObject::objectName() of \a material at the time addMaterial()
    is called will be used as the material's name within this collection.
    Changes to the object name after the material is added are ignored.

    If \a material is already present in this collection, then this
    function will return the index that was previously assigned.

    Returns -1 if \a material has been added to another collection.

    \sa removeMaterial(), markMaterialAsUsed()
*/
int QGLMaterialCollection::addMaterial(QGLMaterial *material)
{
    Q_D(QGLMaterialCollection);
    Q_ASSERT(material);

    // Allocate a new index for the material.
    int index = d->materials.count();

    // Record the index in the private data attached to the material.
    // This allows us to find the material's index quickly later.
    QGLMaterialPrivate *dm = material->d_func();
    if (dm->collection) {
        if (dm->collection == this)
            return dm->index;
        return -1;
    }
    dm->collection = this;
    dm->index = index;
    dm->name = material->objectName();
    dm->used = false;

    // Add the material to this collection.
    material->setParent(this);
    d->materials.append(material);
    if (!dm->name.isEmpty())
        d->materialNames[dm->name] = index;
    connect(material, SIGNAL(destroyed()), this, SLOT(materialDeleted()));
    return index;
}

/*!
    Removes all instances of \a material from this collection.
    The \a material object is not deleted and can be reused.

    Does nothing if \a material is null or not a member of
    this collection.

    \sa addMaterial()
*/
void QGLMaterialCollection::removeMaterial(QGLMaterial *material)
{
    Q_D(QGLMaterialCollection);
    if (!material)
        return;

    // Check the material's owning collection.
    QGLMaterialPrivate *dm = material->d_func();
    if (dm->collection != this)
        return;

    // Remove the material from the collection.
    d->materials[dm->index] = 0;
    if (!dm->name.isEmpty())
        d->materialNames.remove(dm->name);
    material->setParent(0);

    // Detach from the owning collection.
    dm->collection = 0;
    dm->index = -1;
}

/*!
    Removes the material at \a index from this collection, and returns
    a pointer to the material.

    Since the collection is designed for fast lookup by index, the
    the stored material pointer is set to null but the \a index
    otherwise remains valid.
*/
QGLMaterial *QGLMaterialCollection::removeMaterial(int index)
{
    Q_D(QGLMaterialCollection);

    // Bail out if the material is invalid.
    if (index < 0 || index >= d->materials.count())
        return 0;
    QGLMaterial *material = d->materials[index];
    if (!material)
        return 0;

    // Remove the material from the collection.
    QGLMaterialPrivate *dm = material->d_func();
    d->materials[index] = 0;
    if (!dm->name.isEmpty())
        d->materialNames.remove(dm->name);
    material->setParent(0);

    // Detach from the owning collection.
    dm->collection = 0;
    dm->index = -1;
    return material;
}

/*!
    Returns true if this collection is empty, false otherwise.

    \sa size()
*/
bool QGLMaterialCollection::isEmpty() const
{
    Q_D(const QGLMaterialCollection);
    return d->materials.isEmpty();
}

/*!
    Returns the number of (possibly null) materials in this collection.
    Null materials result from calling removeMaterial().

    \sa isEmpty()
*/
int QGLMaterialCollection::size() const
{
    Q_D(const QGLMaterialCollection);
    return d->materials.size();
}

/*!
    \internal
    Responds to the destroyed() signal by calling removeMaterial() on the
    material about to be deleted;
*/
void QGLMaterialCollection::materialDeleted()
{
    removeMaterial(qobject_cast<QGLMaterial *>(sender()));
}

QT_END_NAMESPACE
