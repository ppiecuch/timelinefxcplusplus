#pragma once

/*
 * Qt OpenGL for rendering
 * PugiXML for parsing data
 */

#ifndef _QTEFFECTSLIBRARY_H
#define _QTEFFECTSLIBRARY_H

#include <QColor>
#include <QPointer>

#include "TLFXEffectsLibrary.h"
#include "TLFXParticleManager.h"
#include "TLFXAnimImage.h"
#include "TLFXParticle.h"

#include "qgeometry/qgeometrydata.h"
#include "qgeometry/qatlastexture.h"

class QOpenGLTexture;
class QGLPainter;
class XMLLoader;

class QtImage : public TLFX::AnimImage
{
public:
    QtImage(QAtlasManager *atlas, const QString &library = "")
    : _library(library)
    , _texture(0) , _atlas(atlas) { }

    virtual bool Load(const char *filename);
    QTexture *GetTexture() const { return _texture; }
    QString GetImageName() const { return _image; }

protected:
    QString _library;
    QString _image;

    QPointer<QTexture> _texture;
    QPointer<QAtlasManager> _atlas;
};

class QtEffectsLibrary : public TLFX::EffectsLibrary
{
public:
    QtEffectsLibrary();
    ~QtEffectsLibrary();

    bool LoadLibrary(const char *library, const char *filename = 0, bool compile = true);

    virtual TLFX::XMLLoader* CreateLoader() const;
    virtual TLFX::AnimImage* CreateImage() const;
    
    GLuint TextureAtlas() { return _atlas->atlasTextureId(); }

protected:
    QString _library;
    QAtlasManager *_atlas;
};

class QtParticleManager : public TLFX::ParticleManager
{
public:
    enum GlobalBlendModeType {
        FromEffectBlendMode,
        AddBlendMode,
        AlphaBlendMode,
        GlobalBlendModesNum
    };
    QtParticleManager(QGLPainter *p, int particles = TLFX::ParticleManager::particleLimit, int layers = 1);
    void Reset() { Destroy(); batch = QGeometryData(); _lastTexture = 0; _lastAdditive = true; }
    void Flush();
    
    GlobalBlendModeType GlobalBlendMode() { return _globalBlend; }
    QString GlobalBlendModeInfo() {
        switch(_globalBlend)
        {
            case FromEffectBlendMode: return QString("effect blend");
            case AddBlendMode: return QString("addictive blend");
            case AlphaBlendMode: return QString("alpha blend");
            default: return QString("??");
        }
    }
    void SetGlobalBlendMode(GlobalBlendModeType state) { _globalBlend = state; }
    void ToggleGlobalBlendMode() { _globalBlend = GlobalBlendModeType((int(_globalBlend)+1)%GlobalBlendModesNum); }

protected:
    virtual void DrawSprite(TLFX::Particle *p, TLFX::AnimImage* sprite, float px, float py, float frame, float x, float y, float rotation, float scaleX, float scaleY, unsigned char r, unsigned char g, unsigned char b, float a, bool additive);

    // batching
    struct Batch
    {
        float px, py;
        float frame;
        float x, y;
        float rotation;
        float scaleX, scaleY;
        QColor color;
    };
    std::list<Batch> _batch;
    QGeometryData batch;
    QPointer<QTexture> _lastTexture;
    bool _lastAdditive;
    GlobalBlendModeType _globalBlend;
    
    QGLPainter *_p;
};

#endif // _QTEFFECTSLIBRARY_H
