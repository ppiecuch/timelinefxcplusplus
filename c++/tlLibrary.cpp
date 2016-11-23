// 
//	File	   	-> tlLibrary.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlLibrary.h"

#include "tlParticleManager.h"
#include "tlEffect.h"
#include "tlEmitter.h"
#include "tlParticle.h"

//------------------------------------------------------------------------------
// Effects library for storing a list of effects and particle images/animations
// about: When using #LoadEffects, all the effects and images that go with them are stored in this type.


//------------------------------------------------------------------------------

tlLibrary::tlLibrary() : m_EffectCount(0), m_EmitterCount(0), m_ShapeCount(0)
{
}

tlLibrary::~tlLibrary()
{
}

//------------------------------------------------------------------------------
// Add a new effect to the library including any sub effects and emitters. Effects can be retrieved using #GetEffect.

void tlLibrary::AddEffect(tlEffect *e)
{
	m_EffectArray[ m_EffectCount++ ] = e;

	tlEmitter * em = (tlEmitter*) e->m_Children;
	while (em)
	{	AddEmitter(em);
		em = (tlEmitter*) em->m_NextSibling;
	}
}
tlEffect * tlLibrary::GetEffect(const char * name)
{
	u32 hash = HashFromString(name);
	for (int i=0; i<m_EffectCount; ++i)
	{
		if (m_EffectArray[i]->m_HashName == hash)
		{
			return m_EffectArray[i];
		}
	}
	return NULL;
}

//------------------------------------------------------------------------------
// Add a new emitter to the library. Emitters are stored using a map and can be retrieved using #GetEmitter.
//
// Generally you don't want to call this at all unless you're building your effects manually, just use #AddEffect and all its emitters will be added also.

void tlLibrary::AddEmitter(tlEmitter *e)
{
	m_EmitterArray[ m_EmitterCount++ ] = e;

	tlEffect * ef = e->m_Effects;
	while ( ef )
	{
		AddEffect(ef);
		ef = ef->m_Next;
	}
}
tlEmitter * tlLibrary::GetEmitter( const char * name )
{
	u32 hash = HashFromString(name);
	for (int i=0; i<m_EmitterCount; ++i)
	{
		if (m_EmitterArray[i]->m_HashName == hash)
		{
			return m_EmitterArray[i];
		}
	}
	return NULL;
}

//------------------------------------------------------------------------------

void tlLibrary::AddShape( u32 index, u32 frames, const char * url )
{
	tlShape * s = new tlShape();
	s->index = index;
	s->frames = frames;
	StringCopy( s->url, FileGetRoot(url) );

	m_ShapeArray[ m_ShapeCount++ ] = s;
}
//------------------------------------------------------------------------------

tlShape * tlLibrary::GetShape( u32 index )
{
	for (int i=0; i<m_ShapeCount; ++i)
	{
		if ( m_ShapeArray[i]->index == index )
		{
			return m_ShapeArray[i];
		}
	}
	return NULL;
}

//------------------------------------------------------------------------------
// Clear all effects in the library
// Use this to empty the library of all effects and shapes.

void tlLibrary::ClearAll( )
{
	for (int i=0; i<m_EffectCount; ++i)
	{
		tlEffect * e = m_EffectArray[i];
		delete e;
	}
	m_EffectCount = 0;

	for (int i=0; i<m_ShapeCount; ++i)
	{
		delete m_ShapeArray[i];
	}
	m_ShapeCount = 0;

	m_EmitterCount = 0;
}
