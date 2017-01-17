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
#include "qgeometry/qglpainter.h"

class QOpenGLTexture;
class XMLLoader;

class QtImage : public TLFX::AnimImage
{
public:
    QtImage();
    ~QtImage();

    bool Load(const char *filename);
    QOpenGLTexture *GetTexture() const;

protected:
    QOpenGLTexture *_texture;
};

class QtEffectsLibrary : public TLFX::EffectsLibrary
{
public:
    virtual TLFX::XMLLoader* CreateLoader() const;
    virtual QtImage* CreateImage() const;
};

class QtParticleManager : public TLFX::ParticleManager
{
public:
    QtParticleManager(int particles = TLFX::ParticleManager::particleLimit, int layers = 1);
    void Flush();

protected:
    virtual void DrawSprite(QtImage* sprite, float px, float py, float frame, float x, float y, float rotation,
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
    QtImage         *_lastSprite;
    bool             _lastAdditive;
private:
    QGLPainter painter;
};

#endif // _QTEFFECTSLIBRARY_H
