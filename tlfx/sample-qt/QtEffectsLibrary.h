#pragma once

/*
 * Qt OpenGL for rendering
 * PugiXML for parsing data
 */

#ifndef _QTEFFECTSLIBRARY_H
#define _QTEFFECTSLIBRARY_H

#include <QColor>

#include "TLFXEffectsLibrary.h"
#include "TLFXParticleManager.h"
#include "TLFXAnimImage.h"

#include "qgeometry/qgeometrydata.h"

class QOpenGLTexture;
class QGLPainter;
class XMLLoader;

class QtImage : public TLFX::AnimImage
{
public:
    QtImage();
    ~QtImage();

    virtual bool Load(const char *filename);
    QOpenGLTexture *GetTexture() const { return _texture; }
    QString GetImageName() const { return _image; }

protected:
    QString _image;
    QOpenGLTexture *_texture;
};

class QtEffectsLibrary : public TLFX::EffectsLibrary
{
public:
    virtual TLFX::XMLLoader* CreateLoader() const;
    virtual TLFX::AnimImage* CreateImage() const;
};

class QtParticleManager : public TLFX::ParticleManager
{
public:
    QtParticleManager(QGLPainter *p, int particles = TLFX::ParticleManager::particleLimit, int layers = 1);
    void Reset() { ClearAll(); batch = QGeometryData(); _lastSprite = 0; _lastAdditive = true; }
    void Flush();

protected:
    virtual void DrawSprite(TLFX::AnimImage* sprite, float px, float py, float frame, float x, float y, float rotation,
                            float scaleX, float scaleY, unsigned char r, unsigned char g, unsigned char b, float a, bool additive);

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
    TLFX::AnimImage*_lastSprite;
    bool _lastAdditive;
    
    QGLPainter *_p;
};

#endif // _QTEFFECTSLIBRARY_H
