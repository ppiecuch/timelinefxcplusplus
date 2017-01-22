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

#ifndef QGLPAINTER_H
#define QGLPAINTER_H

#include <QOpenGLFunctions>

#include "qglnamespace.h"
#include "qgeometry3d.h"
#include "qglvertexbundle.h"
#include "qglindexbuffer.h"
#include "qglmaterial.h"
#include "qglsurface.h"
#include "qmatrix4x4stack.h"
#include "qglcamera.h"
#include "qarray.h"

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QPainter>

QT_BEGIN_NAMESPACE

class QGLPainter;
class QGLTexture2D;
class QGLTextureCube;
class QGeometryData;
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;
class QGLRenderSequencer;
class QGLAbstractSurface;

class QGLPainterPrivate;
class QGLLightModelPrivate;
class QGLLightParametersPrivate;


class QGLLightParameters : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLLightParameters)
    Q_ENUMS(LightType)
    Q_PROPERTY(LightType type READ type)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(QColor ambientColor READ ambientColor WRITE setAmbientColor NOTIFY ambientColorChanged)
    Q_PROPERTY(QColor diffuseColor READ diffuseColor WRITE setDiffuseColor NOTIFY diffuseColorChanged)
    Q_PROPERTY(QColor specularColor READ specularColor WRITE setSpecularColor NOTIFY specularColorChanged)
    Q_PROPERTY(QVector3D spotDirection READ spotDirection WRITE setSpotDirection NOTIFY spotDirectionChanged)
    Q_PROPERTY(float spotExponent READ spotExponent WRITE setSpotExponent NOTIFY spotExponentChanged)
    Q_PROPERTY(float spotAngle READ spotAngle WRITE setSpotAngle NOTIFY spotAngleChanged)
    Q_PROPERTY(float constantAttenuation READ constantAttenuation WRITE setConstantAttenuation NOTIFY constantAttenuationChanged)
    Q_PROPERTY(float linearAttenuation READ linearAttenuation WRITE setLinearAttenuation NOTIFY linearAttenuationChanged)
    Q_PROPERTY(float quadraticAttenuation READ quadraticAttenuation WRITE setQuadraticAttenuation NOTIFY quadraticAttenuationChanged)
public:
    enum LightType {
        Directional,
        Positional
    };

    QGLLightParameters(QObject *parent = 0);
    ~QGLLightParameters();

    QGLLightParameters::LightType type() const;

    QVector3D position() const;
    void setPosition(const QVector3D& value);

    QVector3D direction() const;
    void setDirection(const QVector3D& value);

    QColor ambientColor() const;
    void setAmbientColor(const QColor& value);

    QColor diffuseColor() const;
    void setDiffuseColor(const QColor& value);

    QColor specularColor() const;
    void setSpecularColor(const QColor& value);

    QVector3D spotDirection() const;
    void setSpotDirection(const QVector3D& value);

    float spotExponent() const;
    void setSpotExponent(float value);

    float spotAngle() const;
    void setSpotAngle(float value);

    float spotCosAngle() const;

    float constantAttenuation() const;
    void setConstantAttenuation(float value);

    float linearAttenuation() const;
    void setLinearAttenuation(float value);

    float quadraticAttenuation() const;
    void setQuadraticAttenuation(float value);

    QVector4D eyePosition(const QMatrix4x4& transform) const;
    QVector3D eyeSpotDirection(const QMatrix4x4& transform) const;

Q_SIGNALS:
    void positionChanged();
    void directionChanged();
    void ambientColorChanged();
    void diffuseColorChanged();
    void specularColorChanged();
    void spotDirectionChanged();
    void spotExponentChanged();
    void spotAngleChanged();
    void constantAttenuationChanged();
    void linearAttenuationChanged();
    void quadraticAttenuationChanged();
    void lightChanged();

private:
    Q_DISABLE_COPY(QGLLightParameters)

    QScopedPointer<QGLLightParametersPrivate> d_ptr;
};

class QGLLightModel : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGLLightModel)
    Q_ENUMS(Model)
    Q_ENUMS(ColorControl)
    Q_ENUMS(ViewerPosition)
    Q_PROPERTY(Model model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(ColorControl colorControl READ colorControl WRITE setColorControl NOTIFY colorControlChanged)
    Q_PROPERTY(ViewerPosition viewerPosition READ viewerPosition WRITE setViewerPosition NOTIFY viewerPositionChanged)
    Q_PROPERTY(QColor ambientSceneColor READ ambientSceneColor WRITE setAmbientSceneColor NOTIFY ambientSceneColorChanged)
public:
    explicit QGLLightModel(QObject *parent = 0);
    ~QGLLightModel();

    enum Model
    {
        OneSided,
        TwoSided
    };

    enum ColorControl
    {
        SingleColor,
        SeparateSpecularColor
    };

    enum ViewerPosition
    {
        ViewerAtInfinity,
        LocalViewer
    };

    QGLLightModel::Model model() const;
    void setModel(QGLLightModel::Model value);

    QGLLightModel::ColorControl colorControl() const;
    void setColorControl(QGLLightModel::ColorControl value);

    QGLLightModel::ViewerPosition viewerPosition() const;
    void setViewerPosition(QGLLightModel::ViewerPosition value);

    QColor ambientSceneColor() const;
    void setAmbientSceneColor(const QColor& value);

Q_SIGNALS:
    void modelChanged();
    void colorControlChanged();
    void viewerPositionChanged();
    void ambientSceneColorChanged();
    void lightModelChanged();

private:
    Q_DISABLE_COPY(QGLLightModel)

    QScopedPointer<QGLLightModelPrivate> d_ptr;
};

class QGLPainter : public QOpenGLFunctions
{
public:
    QGLPainter();
    explicit QGLPainter(QOpenGLContext *context);
    explicit QGLPainter(QWindow *widget);
    explicit QGLPainter(QPainter *painter);
    explicit QGLPainter(QGLAbstractSurface *surface);
    virtual ~QGLPainter();

    bool begin();
    bool begin(QOpenGLContext *context);
    bool begin(QWindow *window);
    bool begin(QPainter *painter);
    bool begin(QGLAbstractSurface *surface);
    bool end();
    bool isActive() const;

    QOpenGLContext *context() const;

    bool isFixedFunction() const;

    enum Update
    {
        UpdateColor                 = 0x00000001,
        UpdateModelViewMatrix       = 0x00000002,
        UpdateProjectionMatrix      = 0x00000004,
        UpdateMatrices              = 0x00000006,
        UpdateLights                = 0x00000008,
        UpdateMaterials             = 0x00000010,
        UpdateViewport              = 0x00000020,
        UpdateAll                   = 0x7FFFFFFF
    };
    Q_DECLARE_FLAGS(Updates, Update)

    void setClearColor(const QColor& color);

    void setScissor(const QRect& rect);

    QMatrix4x4Stack& projectionMatrix();
    QMatrix4x4Stack& modelViewMatrix();
    QMatrix4x4 combinedMatrix() const;
    QMatrix3x3 normalMatrix() const;
    QMatrix4x4 worldMatrix() const;

    QGL::Eye eye() const;
    void setEye(QGL::Eye eye);

    void setCamera(const QGLCamera *camera);

    bool isCullable(const QVector3D& point) const;
    bool isCullable(const QBox3D& box) const;
    QGLRenderSequencer *renderSequencer();

    float aspectRatio() const;

    QGLAbstractEffect *effect() const;

    QGLAbstractEffect *userEffect() const;
    void setUserEffect(QGLAbstractEffect *effect);

    QGL::StandardEffect standardEffect() const;
    void setStandardEffect(QGL::StandardEffect effect);

    void disableEffect();

    QOpenGLShaderProgram *cachedProgram(const QString& name) const;
    void setCachedProgram(const QString& name, QOpenGLShaderProgram *program);

    QColor color() const;
    void setColor(const QColor& color);

    QGLAttributeSet attributes() const;
    void clearAttributes();
    
    void clearBoundBuffers();

    void setVertexAttribute
        (QGL::VertexAttribute attribute, const QGLAttributeValue& value);
    void setVertexBundle(const QGLVertexBundle& buffer);

    void update();
    void updateFixedFunction(QGLPainter::Updates updates);

    void draw(QGL::DrawingMode mode, int count, int index = 0);
    void draw(QGL::DrawingMode mode, const ushort *indices, int count);
    void draw(QGL::DrawingMode mode, const QGLIndexBuffer& indices);
    virtual void draw(QGL::DrawingMode mode, const QGLIndexBuffer& indices, int offset, int count);

    void pushSurface(QGLAbstractSurface *surface);
    QGLAbstractSurface *popSurface();
    void setSurface(QGLAbstractSurface *surface);
    QGLAbstractSurface *currentSurface() const;

    const QGLLightModel *lightModel() const;
    void setLightModel(const QGLLightModel *value);

    const QGLLightParameters *mainLight() const;
    void setMainLight(const QGLLightParameters *parameters);
    void setMainLight
        (const QGLLightParameters *parameters, const QMatrix4x4& transform);
    QMatrix4x4 mainLightTransform() const;

    int addLight(const QGLLightParameters *parameters);
    int addLight(const QGLLightParameters *parameters, const QMatrix4x4 &transform);
    void removeLight(int lightId);

    int maximumLightId() const;
    const QGLLightParameters *light(int lightId) const;
    QMatrix4x4 lightTransform(int lightId) const;

    const QGLMaterial *faceMaterial(QGL::Face face) const;
    void setFaceMaterial(QGL::Face face, const QGLMaterial *value);
    void setFaceColor(QGL::Face face, const QColor& color);

    bool isPicking() const;
    void setPicking(bool value);

    int objectPickId() const;
    void setObjectPickId(int value);
    void clearPickObjects();

    QColor pickColor() const;

    int pickObject(int x, int y) const;

private:
    Q_DISABLE_COPY(QGLPainter)

    QGLPainterPrivate *d_ptr;

    QGLPainterPrivate *d_func() const { return d_ptr; }

    friend class QGLAbstractEffect;

    bool begin(QOpenGLContext *context, QGLAbstractSurface *surface,
               bool destroySurface = true);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGLPainter::Updates)

class QGLAbstractEffect
{
public:
    QGLAbstractEffect();
    virtual ~QGLAbstractEffect();

    virtual bool supportsPicking() const;
    virtual void setActive(QGLPainter *painter, bool flag) = 0;
    virtual void update(QGLPainter *painter, QGLPainter::Updates updates) = 0;
};

QT_END_NAMESPACE

#endif
