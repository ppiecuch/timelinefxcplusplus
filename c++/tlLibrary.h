// 
//	File	   	-> tlLibrary.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_EFFECT_LIBRARY_H__
#define __PARTICLE_EFFECT_LIBRARY_H__

//------------------------------------------------------------------------------

#include "Types.h"

class tlEmitter;
class tlEffect;
class tlParticle;

//------------------------------------------------------------------------------

typedef struct _tlShape
{
	u32 index;
	u32 frames;
	char url[32];

} tlShape;

//------------------------------------------------------------------------------

class tlLibrary
{
public:
	tlLibrary();
	~tlLibrary();

	void AddEffect(tlEffect * e);
	tlEffect * GetEffect(const char * name);

	void AddEmitter(tlEmitter * e);
	tlEmitter * GetEmitter(const char * name);

	void AddShape( u32 index, u32 frames, const char * url );
	tlShape * GetShape( u32 index );

	void ClearAll();

public:
	u32 m_EffectCount;
	tlEffect * m_EffectArray[256];

	u32 m_EmitterCount;
	tlEmitter * m_EmitterArray[256];

	u32 m_ShapeCount;
	tlShape * m_ShapeArray[256];
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
