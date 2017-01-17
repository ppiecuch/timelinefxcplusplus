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

#ifndef QGLEFFECT_H
#define QGLEFFECT_H

#include "qglpainter.h"
#include <QScopedPointer.h>
#include <QVector>
#include <QDir>
#include <QXmlStreamReader>
#include <qglobal.h>
#include <QString>
#include <QStringList>
#include <QtCore/qscopedpointer.h>

Q_DECLARE_METATYPE(QArray<float>)

QT_BEGIN_NAMESPACE

class QOpenGLShaderProgram;
class QGLColladaFxEffect;
class QGLColladaParam;
class QGLColladaImageParam;
class QGLColladaSurfaceParam;
class QGLColladaSampler2DParam;
class QGLColladaFxEffectPrivate;
class QGLColladaFxEffectLoaderPrivate;
class QGLShaderProgramEffectPrivate;

class QGLShaderProgramEffect : public QGLAbstractEffect
{
public:
    QGLShaderProgramEffect();
    virtual ~QGLShaderProgramEffect();

    void setActive(QGLPainter *painter, bool flag);
    void update(QGLPainter *painter, QGLPainter::Updates updates);

    QByteArray vertexShader() const;
    void setVertexShader(const QByteArray &source);
    void setVertexShaderFromFile(const QString &fileName);

    QByteArray geometryShader() const;
    void setGeometryShader(const QByteArray &source);
    void setGeometryShaderFromFile(const QString &fileName);

    QByteArray fragmentShader() const;
    void setFragmentShader(const QByteArray &source);
    void setFragmentShaderFromFile(const QString &fileName);

    int maximumLights() const;
    void setMaximumLights(int value);

    QOpenGLShaderProgram *program() const;

protected:
    virtual bool beforeLink();
    virtual void afterLink();

private:
    QScopedPointer<QGLShaderProgramEffectPrivate> d_ptr;

    Q_DISABLE_COPY(QGLShaderProgramEffect)
    Q_DECLARE_PRIVATE(QGLShaderProgramEffect)
};

class QGLColladaFxEffect : public QGLShaderProgramEffect
{
    friend class QGLColladaFxEffectFactory;

public:
    enum Lighting
    {
        NoLighting,
        BlinnLighting,
        PhongLighting,
        ConstantLighting,
        LambertLighting,
        CustomLighting
    };

    QGLColladaFxEffect();
    ~QGLColladaFxEffect();
    void update(QGLPainter *painter, QGLPainter::Updates updates);
    void generateShaders();
    void addBlinnPhongLighting();

    void setId(QString);
    void setSid(QString);
    QString id();
    QString sid();

    void setLighting(int lighting);
    int lighting();
    void setMaterial(QGLMaterial* newMaterial);
    QGLMaterial* material();

    QGLTexture2D* diffuseTexture();

    bool isActive();
    void setActive(QGLPainter *painter, bool flag);
private:
    QGLColladaFxEffect(const QGLColladaFxEffect&);
    QGLColladaFxEffectPrivate* d;
};

typedef struct _ResultState
{
    QMap<QString, QVariant> paramSids;
    QMap<QString, QVariant> paramIds;
    QMap<QString, QVariant> paramNames;
    QDir sourceDir;
    QMap<QGLTexture2D*, QString> unresolvedTexture2Ds;
} ResultState;

class QGLColladaFxEffectFactory
{
public:
    // Collada parsing functions
    static QList<QGLColladaFxEffect*> loadEffectsFromFile(const QString& fileName );

protected:
    static QList<QGLColladaFxEffect*> loadEffectsFromXml( QXmlStreamReader& xml, QDir homeDirectory = QDir());
    static void processLibraryImagesElement( QXmlStreamReader& xml, ResultState* stateStack );
    static QList<QGLColladaFxEffect*> processLibraryEffectsElement( QXmlStreamReader& xml, ResultState* stateStack );
    static QList<QGLColladaFxEffect*> processEffectElement( QXmlStreamReader& xml, ResultState* stateStack );
    static QList<QGLColladaFxEffect*> processProfileElement( QXmlStreamReader& xml, ResultState* stateStack );

    static QGLColladaParam* processPassElement( QXmlStreamReader& xml, ResultState* stateStack, QGLColladaFxEffect* effect );
    static QGLColladaFxEffect* processTechniqueElement( QXmlStreamReader& xml, ResultState* stateStack, QString &profileName );
    static QGLColladaParam* processNewparamElement( QXmlStreamReader& xml, ResultState* stateStack );
    static void processImageElement( QXmlStreamReader& xml, ResultState* stateStack );
    static QGLColladaSurfaceParam* processSurfaceElement( QXmlStreamReader& xml, ResultState* stateStack, QString passedInSid = QString());
    static void processSampler2DElement( QXmlStreamReader& xml, ResultState* stateStack, QString passedInSid );
    static QGLTexture2D* processTextureElement( QXmlStreamReader& xml , ResultState* stateStack );
    static QVariant processFloatList( QXmlStreamReader& xml );
    static QColor processColorElement( QXmlStreamReader& xml );
    static float processParamOrFloatElement( QXmlStreamReader& xml );
    static QVariant processColorOrTextureElement( QXmlStreamReader& xml );
    QGLColladaFxEffectFactory();
    static void processProgramElement( QXmlStreamReader& xml, ResultState* stateStack, QGLColladaFxEffect* effect );

    // Collada generation functions
public:
    static QString exportEffect(QGLColladaFxEffect *effect, QString effectId, QString techniqueSid);

protected:
    static QStringList glslProfileFromEffect(QGLColladaFxEffect* effect, QString techniqueSid);
    static QStringList generateProgramElement(QGLColladaFxEffect* effect, QString techniqueSid);
    static QStringList generateShaderElement( QGLColladaFxEffect* effect, QString vertexShaderRefSid, QString fragmentShaderRefSid );
    static QStringList generateBindUniformElement( QGLColladaFxEffect* effect );
    static QStringList generateBindAttributeElement( QGLColladaFxEffect* effect );
    static QStringList generateBindUniformElements( QGLColladaFxEffect* effect );
    static QStringList generateCodeElements( QGLColladaFxEffect* effect, QString baseSid );

    static QImage resolveImageURI(ResultState *resultState, QString imageFileName);
    static bool resolveTexture2DImage(QGLTexture2D *result, ResultState *resultState, QString paramName);
};

class QGLColladaParam
{
    friend class QGLColladaFxEffectFactory;
public:
    enum {
        UnknownType = 0,
        Sampler2DType,
        Texture2DType,
        SurfaceType,
        ImageType,
        UserDefinedType = 100
    };

    virtual ~QGLColladaParam();

    int type();
    QVector<float> value();
    QString sid();
    QString id();

    static QString typeString(int);

protected:
    QGLColladaParam(QString sid, int type);
    QString mSid;
    QString mId;
    int mType;
    QVector<float> mValue;
};

class QGLColladaTextureParam : public QGLColladaParam
{
    friend class QGLColladaFxEffectFactory;
public:
    QGLColladaTextureParam(QString sid, QGLTexture2D* texture);
    QGLTexture2D* texture();
    QString samplerSid();
protected:
    QGLTexture2D* mTexture;
    QString sampler2DSid;
    QString texCoordSid;
};

class QGLColladaSurfaceParam : public QGLColladaParam
{
    friend class QGLColladaFxEffectFactory;
public:
    QGLColladaSurfaceParam(QString sid);
protected:
    QString mInitFrom;
    QString mFormat;
    QString mFormatHint;
    QString mSize;
    QVector<int> mSizeVector;
    QPointF mViewportRatio;
    int mMipLevels;
    bool mMipMapGenerate;
    QString mExtra;
    QString mGenerator;
};

class QGLColladaSampler2DParam : public QGLColladaParam
{
    friend class QGLColladaFxEffectFactory;
public:
    QGLColladaSampler2DParam(QString sid, QGLTexture2D* sampler);
    QGLColladaSampler2DParam(QString sid, QString sourceSid);
    QGLTexture2D sampler();
    QString sourceSid();
protected:
    QGLTexture2D* mTexture;
    QString mSourceSid;
};

// "image" isn't really a param, but it shares enough that it seems sensible
// to re-use the QGLColladaParam base.
class QGLColladaImageParam : public QGLColladaParam
{
    friend class QGLColladaFxEffectFactory;
public:
    QGLColladaImageParam(QString sid, QImage image);
    QImage image();
    QString name();
protected:
    QImage mImage;
    QString mName;
};


class QGLColladaFxEffectLoader
{
public:
    QGLColladaFxEffectLoader();
    ~QGLColladaFxEffectLoader();
    bool load(QString filename);
    QStringList effectNames();
    QGLColladaFxEffect *effect(QString effectName);
    int count();
    QGLColladaFxEffect* operator[](int);
private:
    Q_DECLARE_PRIVATE(QGLColladaFxEffectLoader)
    QScopedPointer<QGLColladaFxEffectLoaderPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QGLEFFECT_H
