// 
//	File	   	-> tlEffect.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_EFFECT__H__
#define __PARTICLE_EFFECT__H__

//------------------------------------------------------------------------------

#include "gradient.h"
#include "tltypes.h"
#include "tlentity.h"

//------------------------------------------------------------------------------

class tlEmitter;
class tlParticle;

//------------------------------------------------------------------------------

class tlEffect : public tlEntity
{
public:
	tlEffect();
	~tlEffect();

	void Destroy();
	void ShowAll();
	void HideAll();
	void ShowOne(tlEmitter * em);
	int EmitterCount();

	void AssignParticleManager( tlParticleManager * pm );
	void SetParticleManager( tlParticleManager * pm );

	void SetClass( int classType );
	void SetLockAspect( bool state );
	void SetMGX( int max );
	void SetMGY( int max );
	void SetEmitAtPoints( bool state );
	void SetEmissionType( int type );
	void SetEffectLength( f32 seconds );
	void SetParentEmitter( tlEmitter * parent );
	void SetTraverseEdge( bool state );
	void SetEndBehaviour( int value );
	void SetDistanceSetByLife( bool state );
	void SetHandleX( f32 value );
	void SetHandleY( f32 value );
	void SetHandleCenter( bool state );
	void SetReverseSpawn( bool state );
	void SetSpawnDirection();

	void SetAreaSize( f32 width, f32 height );
	void SetLineLength( f32 length );
	void SetEmissionAngle( f32 angle );
	void SetEffectAngle( f32 angle );
	void SetLife( f32 life );
	void SetAmount( f32 amount );
	void SetVelocity( f32 velocity );
	void SetSpin( f32 spin );
	void SetWeight( f32 weight );
	void SetEffectParticleSize( f32 sizex, f32 sizey );
	void SetSizeX( f32 sizex );
	void SetSizeY( f32 sizey );
	void SetEffectAlpha( f32 alpha );
	void SetEffectEmissionRange( f32 emissionRange );
	void SetEllipseArc( f32 degrees );
	void SetZoom( f32 zoom );
	void SetStretch( f32 stretch );
	void SetGroupParticles( bool state );

	int GetClass();
	bool GetLockAspect();
	int GetMGX();
	int GetMGY();
	int GetEmitAtPoints();
	int GetEmissionType();
	f32 GetEffectLength();
	tlEmitter * GetParentEmitter();
	tlEntity * GetParent();
	bool GetTraverseEdge();
	int GetEndBehaviour();
	bool GetDistanceSetByLife();
	bool GetHandleCenter();
	bool GetReverseSpawn();
	int GetParticleCount();
	f32 GetEllipseArc();
	bool HasParticles();

	void AddEffect( tlEffect * e );
	void AddEmitter( tlEmitter * e );

	void Update( );

	void SoftKill();
	void HardKill();

	f32 GetLongestLife();

	void Compile_All( );
public:
	tlParticle * m_InUse[9];			// This stores particles created by the effect, for drawing purposes only ( 9 layers )

	int m_Class;						// The type of effect - point, area, line or ellipse
	int m_GradientIndex;				// the lookup into gradient tables.

	//int m_Complete;
	
	bool m_LockAspect;					// Set to true if the effect should scale uniformly

	int m_GX;							// Grid x coords for emitting at points
	int m_GY;							// Grid y coords for emitting at points
	int m_MGX;							// The maximum value of gx
	int m_MGY;							// The maximum value of gy
	int m_EmitAtPoints;					// True to set the effect to emit at points
	int m_EmissionType;					// Set to either inwards, outwards or specified according to emmision angle
	f32 m_EffectLength;					// How long the effect lasts before looping back round to the beginning ( in seconds )
	tlEmitter * m_ParentEmitter;		// If the effect is a sub effect then this is set to the emitter that it's a sub effect of

	//int m_ParticleCount;				// Number of particles this effect has active
	int m_IdleTime;						// Length of time the effect has been idle for without any particles ( in ticks)
	bool m_TraverseEdge;				// Whether the particles within this effect should traverse the edge of the line (lline effects only)
	int m_EndBehaviour;					// Set to whatever the particles should do when they reach the end of the line
	bool m_DistanceSetByLife;			// True if the distance travelled along the line is set accoding to the m_Age of the particles traversing it
	bool m_ReverseSpawn;				// True if the particles should spawn from right to left or anti clockwise (n/a for point effects)
	bool m_Dying;						// Set to true if the effect is in the process of dying, ie no long producing particles.
	bool m_AllowSpawning;				// Set to false to disable emitters from spawning any new particles
	//bool m_DoesNotTimeOut;				// Whether the effect never timeouts automatically

	f32 m_SpawnDirection;				// set to 1 or -1 if reverse spawn is true or false
	f32 m_EllipseArc;					// With ellipse effects this sets the degrees of which particles emit around the edge
	int m_EllipseOffset;				// This is the offset needed to make arc center at the top of the circle.
	//int m_EffectLayer;					// The layer that the effect resides on in its particle manager

	// Compiled arrays of global settings
	bool m_OwnGradients;
	ScalarGradient * c_Life;
	ScalarGradient * c_Amount;
	ScalarGradient * c_SizeX;
	ScalarGradient * c_SizeY;
	ScalarGradient * c_Velocity;
	ScalarGradient * c_Weight;
	ScalarGradient * c_Spin;
	ScalarGradient * c_Alpha;
	ScalarGradient * c_EmissionAngle;
	ScalarGradient * c_EmissionRange;
	ScalarGradient * c_Width;
	ScalarGradient * c_Height;
	ScalarGradient * c_Angle;
	ScalarGradient * c_Stretch;
	ScalarGradient * c_GlobalZoom;
	
	f32 m_CurrentLife;
	f32 m_CurrentAmount;
	f32 m_CurrentSizeX;
	f32 m_CurrentSizeY;
	f32 m_CurrentVelocity;
	f32 m_CurrentSpin;
	f32 m_CurrentWeight;
	f32 m_CurrentWidth;
	f32 m_CurrentHeight;
	f32 m_CurrentAlpha;
	f32 m_CurrentEmissionAngle;
	f32 m_CurrentEmissionRange;
	f32 m_CurrentStretch;
	f32 m_CurrentGlobalZoom;
	
	bool m_OverrideSize;
	bool m_OverrideEmissionAngle;
	bool m_OverrideEmissionRange;
	bool m_OverrideAngle;

	bool m_OverrideLife;
	bool m_OverrideAmount;
	bool m_OverrideVelocity;
	bool m_OverrideSpin;
	bool m_OverrideSizeX;
	bool m_OverrideSizeY;
	bool m_OverrideWeight;
	bool m_OverrideAlpha;
	bool m_OverrideStretch;
	bool m_OverrideGlobalZoom;
	
	int m_Bypass_Weight;

	tlEffect * m_Next;
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
