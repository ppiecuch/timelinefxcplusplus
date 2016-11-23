// 
//	File	   	-> tlEmitter.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_EMITTER__H__
#define __PARTICLE_EMITTER__H__

//------------------------------------------------------------------------------

#include "Types.h"
#include "Gradient.h"
#include "tlEntity.h"

class tlParticle;
class tlEffect;

//------------------------------------------------------------------------------

class tlEmitter : public tlEntity
{
public:
	tlEmitter();
	~tlEmitter();

	void Destroy();

	void ShowAll();
	void HideAll();

	void AddEffect( tlEffect * e );

	void SetParentEffect( tlEffect * parent );

	void SetFrame( int frame );
	void SetAngleOffset( f32 offset );
	void SetUniform( bool state );
	void SetAngleType( int type );
	void SetUseEffectEmission( bool state );
	void SetVisible( bool state );
	void SetSingleParticle( bool state );
	void SetRandomColor( bool state );
	void SetZLayer( int value );
	void SetAnimate( bool state );
	void SetRandomStartFrame( bool state );
	void SetAnimationDirection( f32 dir );
	void SetColorRepeat( bool state );
	void SetAlphaRepeat( bool state );
	void SetOneShot( bool state );
	void SetHandleCenter( bool state );
	void SetParticlesRelative( bool state );
	void SetLockAngle( bool state );
	void SetAngleRelative( bool state );
	void SetOnce( bool state );
	void SetGroupParticles( bool group );

	tlEffect * GetParentEffect();
	int GetFrame();
	f32 GetAngleOffset();
	bool GetUniform();
	int GetAngleType();
	bool GetUseEffectEmission();
	bool GetVisible();
	bool GetSingleParticle();
	bool GetRandomColor();
	int GetZLayer();
	bool GetAnimate();
	bool GetRandomStartFrame();
	f32 GetAnimationDirection();
	int GetColorRepeat();
	int GetAlphaRepeat();
	bool GetOneShot();
	bool GetHandleCenter();
	bool GetParticlesRelative();
	bool GetLockAngle();
	bool GetAngleRelative();
	bool GetOnce();
	bool GetGroupParticles();

	void Update();
	void UpdateSpawns( tlParticle * esingle = NULL );
	void ControlParticle(tlParticle * e);

	void NextFrame();
	void PreviousFrame();

	void Compile_All( );
	void Analyse_Emitter();
	void ResetBypassers();
	f32 GetLongestLife();

public:
	tlEffect * m_Effects;						// list of sub effects added to each particle when they're spawned

	bool m_Uniform;								// whether it scales uniformly
	tlEffect * m_ParentEffect;					// the effect it belongs to

	int m_Frame;								// frame the particle starts with (ignored if m_RandomStartFrame == true)
	int m_BaseFrame;							// baseFrame # of animation set
	int m_FrameCount;							// # frames associated with this glyph set

	f32 m_AngleOffset;			 				// angle variation and offset
	bool m_LockedAngle;							// entity rotation is locked to the direction it's going
	f32 m_GX;									// Grid Coords from grid spawning in an area
	f32 m_GY;

	f32 m_BaseWidth;							// width/height of 1st glyph.  when scaling, we use this as a basis.
	f32 m_BaseHeight;	

	f32 m_Counter;								// counter for the spawning of particles
	int m_AngleType;							// Set to either Align to motion, Random or Specify
	bool m_AngleRelative;						// Whether the angle of the particles should be drawn relative to the parent
	bool m_UseEffectEmission;					// whether the emitter has it// s own set of emission settings
	bool m_Deleted;								// Whether it// s been deleted and awaiting removal from emitter lsit
	bool m_Visible;								// Whether this children particles will be drawn
	bool m_SingleParticle;						// Whether the emitter spawns just a one-off particle, for glow children and blast waves etc.
	bool m_StartedSpawning;						// Whether any particles have been spawned yet
	bool m_RandomColor;							// Whether or not the particle picks a colour at random from the gradient
	int m_ZLayer;								// The z order that the emitter should be drawn in (1-8 layers)
	bool m_Animate;								// Whether or not to use anly 1 frame of the animation
	bool m_RandomStartFrame;						// should the animation start from a random frame each spawn?
	f32 m_AnimationDirection;					// Play the animation backwards or forwards (1.0 or -1.0)
	int m_ColorRepeat;							// Number of times the color sequence should be repeated over the particles lifetime
	int m_AlphaRepeat;							// Number of times the alpha sequence should be repeated over the particles lifetime
	bool m_DirAlternater;						// can use this to alternate between travelling inwards and outwards.
	bool m_OneShot;								// a singleparticle that just fires once and dies
	bool m_ParticlesRelative;					// Whether or not the particles are relative

	bool m_Dying;								// true if the emitter is in the process of dying ie, no longer spawning particles
	bool m_Once;								// Whether the particles of this emitter should animate just the once
	bool m_GroupParticles;						// Set to true to add particles to one big pool, instead of the emitters own pool.
	
	// ----Arrays for quick access to attribute values
	bool m_OwnGradients;
	int m_GradientSize;			// size of gradient tables.
	ScalarGradient * c_R;
	ScalarGradient * c_G;
	ScalarGradient * c_B;
	ScalarGradient * c_BaseSpin;
	ScalarGradient * c_Spin;
	ScalarGradient * c_SpinVariation;
	ScalarGradient * c_Velocity;
	ScalarGradient * c_BaseWeight;
	ScalarGradient * c_Weight;
	ScalarGradient * c_WeightVariation;
	ScalarGradient * c_BaseSpeed;
	ScalarGradient * c_VelVariation;
	ScalarGradient * c_Alpha;
	ScalarGradient * c_SizeX;
	ScalarGradient * c_SizeY;
	ScalarGradient * c_ScaleX;
	ScalarGradient * c_ScaleY;
	ScalarGradient * c_SizeXVariation;
	ScalarGradient * c_SizeYVariation;
	ScalarGradient * c_LifeVariation;
	ScalarGradient * c_Life;
	ScalarGradient * c_Amount;
	ScalarGradient * c_AmountVariation;
	ScalarGradient * c_EmissionAngle;
	ScalarGradient * c_EmissionRange;
	ScalarGradient * c_GlobalVelocity;
	ScalarGradient * c_Direction;
	ScalarGradient * c_DirectionVariation;
	ScalarGradient * c_DirectionVariationOT;
	ScalarGradient * c_FrameRate;
	ScalarGradient * c_Stretch;
	ScalarGradient * c_Splatter;

	// Bypassers
	bool m_Bypass_Weight;
	bool m_Bypass_Speed;
	bool m_Bypass_Spin;
	bool m_Bypass_DirectionVariation;
	bool m_Bypass_Colour;
	bool m_Bypass_ScaleX;
	bool m_Bypass_ScaleY;
	bool m_Bypass_LifeVariaton;
	bool m_Bypass_FrameRate;
	bool m_Bypass_Stretch;
	bool m_Bypass_Splatter;

	//f32 m_CurrentLife;
	//f32 m_Current_Weight;
	//f32 m_Current_WeightVariation;
	//f32 m_Current_Speed;
	//f32 m_Current_SpeedVariation;
	//f32 m_Current_Spin;
	//f32 m_Current_SpinVariation;
	//f32 m_Current_DirectionVariation;
	//f32 m_Current_EmissionAngle;
	//f32 m_Current_EmissionRange;
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
