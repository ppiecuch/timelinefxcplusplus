// 
//	File	   	-> tlParticleManager.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_MANAGER__H
#define __PARTICLE_MANAGER__H

//------------------------------------------------------------------------------

#include "Types.h"
#include "ezxml.h"

#include "tlEntity.h"
#include "tlEffect.h"
#include "tlEmitter.h"
#include "tlParticle.h"
#include "tlLibrary.h"

//------------------------------------------------------------------------------

#define tlPOINT_EFFECT				0
#define tlAREA_EFFECT				1
#define tlLINE_EFFECT				2
#define tlELLIPSE_EFFECT			3

#define tlCONTINUOUS				0
#define tlFINITE					1

#define tlANGLE_ALIGN				0
#define tlANGLE_RANDOM				1
#define tlANGLE_SPECIFY				2

#define tlEMISSION_INWARDS			0
#define tlEMISSION_OUTWARDS			1
#define tlEMISSION_SPECIFIED		2
#define tlEMISSION_IN_AND_OUT		3

#define tlEND_KILL					0
#define tlEND_LOOPAROUND			1
#define tlEND_LETFREE				2

#define tlAREA_EFFECT_TOP_EDGE		0
#define tlAREA_EFFECT_RIGHT_EDGE	1
#define tlAREA_EFFECT_BOTTOM_EDGE	2
#define tlAREA_EFFECT_LEFT_EDGE		3

//------------------------------------------------------------------------------

#define	tlMAX_DIRECTION_VARIATION	22.5f
#define	tlMAX_VELOCITY_VARIATION	30.0f
#define	tlMOTION_VARIATION_INTERVAL	(1.0/30.0f)

//------------------------------------------------------------------------------

class tlParticleManager
{
public:
	
	tlParticleManager( );
	~tlParticleManager( );

	static tlParticleManager * Create( const char * atlasName, int maxBlend, int maxAdd, u32 drawMask=NODE_DRAWMASK_PARTICLE );
	void Destroy();
	void ClearAllEffects();

	tlParticle * GrabParticle( tlEffect * effect, bool groupIt, int layer=0 );
	void ReleaseParticle( tlParticle * p );

	void Update( void * eventData );
	void DrawParticles( void * eventData );
	void DrawParticle( tlParticle * e );

	void SetAngle( f32 angle );
	void SetIdleTimeLimit( int ticks );
	int GetParticlesInUse( );
	int GetParticlesUnUsed( );

	void AddEffect( tlEffect * e );
	void RemoveEffect( tlEffect * e );

	void ClearInUse();
	void ReleaseParticles();
	void DrawEffects();
	void DrawEffect( tlEffect * effect );

	tlEmitter * CreateParticle( tlEffect * parent );
	tlEmitter * CreateEmitter();
	tlEffect * CreateEffect( tlEmitter * parent );

	tlEffect * GetEffectFromLibrary( const char * name );

	tlEffect * CopyEffect( tlEffect * e );
	tlEmitter * CopyEmitter(tlEmitter * em );

	void LinkEffectArrays( tlEffect * efrom, tlEffect * eto );
	void LinkEmitterArrays( tlEmitter * efrom, tlEmitter * eto );


	void LoadEffects( const char * filename );

	tlEffect * LoadEffectXmlTree( ezxml_t xml, tlLibrary * lib, tlEmitter * parent );
	tlEmitter * LoadEmitterXmlTree( ezxml_t effectXML, tlLibrary * lib, tlEffect * e );

	void SetDrawMask( u32 mask ) {m_DrawMask=mask;}

public:
	u32 m_DrawMask;					// For CULLing against various camera masks

	tlLibrary * m_Lib;				// the effects available to this particleManager

	tlParticle * m_ParticleArray;	// allocated memory for particles
	tlParticle * m_UnUsed;			// next free particle
	tlParticle * m_InUse[9];		// head of active particle list for each layer

	int m_UnUsedCount;
	int m_InUseCount;

	tlEffect * m_Effects;			// list of all actively playing effects.
	
	f32 m_Angle;

	f32 m_Scale;					// Retina scale factor
	
	bool m_SpawningAllowed;
	
	int m_IdleTimeLimit;			// The time in game ticks before idle effects are automatically deleted
	
	f32 m_LookupFreq;				// table lookup frequency ( DEFAULT: 60, for 60fps resolution )
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
