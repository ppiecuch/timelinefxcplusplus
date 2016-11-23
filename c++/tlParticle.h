// 
//	File	   	-> tlParticle.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_PARTICLE__H__
#define __PARTICLE_PARTICLE__H__

//------------------------------------------------------------------------------

#include "Types.h"
#include "tlEntity.h"

class tlEmitter;

//------------------------------------------------------------------------------

class tlParticle : public tlEntity
{
public:
	tlParticle();
	~tlParticle();

	f32 GetCurrentFrame();
	void SetCurrentFrame( f32 frame );

	void Update();
	void Reset();
	void Destroy();

public:
	// ---------------------------------
	tlEmitter * m_Emitter;								// emitter it belongs to
	// ---------------------------------
	f32 m_WeightVariation;								// Particle weight variation
	f32 m_GSizeX;										// Particle global size x
	f32 m_GSizeY;										// Particle global size y
	// ---------------------------------
	//f32 m_VelVariation;								// velocity variation
	//f32 m_VelSeed;									// rnd seed
	// ---------------------------------
	f32 m_CurrentFrame;									// current frame of animation
	// ---------------------------------
	f32 m_SpinVariation;								// variation of spin speed
	// ---------------------------------
	f32 m_DirectionVariation;							// Direction variation at spawn time
	f32 m_TimeTracker;									// This is used to keep track of game ticks so that some things can be updated between specific time intervals
	f32 m_RandomDirection;								// current direction of the random motion that pulls the particle in different directions
	f32 m_RandomSpeed;									// random speed to apply to the particle movement
	f32 m_EmissionAngle;								// Direction variation at spawn time
	int m_ReleaseSingleParticle; 						// set to true to release single particles and let them decay and die
	// ---------------------------------
	int m_Layer;										// layer the particle belongs to
	bool m_GroupParticles;								// whether the particle is added the PM pool or kept in the emitter's pool

	tlParticle * m_Prev;								// for linked lists of active/inactive particle objects.
	tlParticle * m_Next;
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
