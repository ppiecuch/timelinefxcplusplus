// 
//	File	   	-> tlEntity.h
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#ifndef __PARTICLE_ENTITY__H__
#define __PARTICLE_ENTITY__H__

//------------------------------------------------------------------------------

#include "tltypes.h"

class tlParticleManager;
class tlParticle;
class tlEffect;
class tlEmitter;

//------------------------------------------------------------------------------
// Types of blending we support.

#define ALPHABLEND		0
#define LIGHTBLEND		1

//------------------------------------------------------------------------------
// base class for particles and emitters.

class tlEntity
{
public:
	tlEntity();
	virtual ~tlEntity();

	virtual void Update( );
	virtual void Destroy();

	void MiniUpdate();
	void UpdateChildren();
	void AssignRootParent( tlEntity * e );
	void Zoom( f32 zoom );

	void AddChild(tlEntity * e);
	void RemoveChild(tlEntity * e);
	void ClearChildren();
	void KillChildren();

	void Rotate(f32 degrees);
	void Move(f32 xAmount, f32 yAmount);

	f32 GetRed();
	void SetRed( f32 r );
	f32 GetGreen();
	void SetGreen( f32 g );
	f32 GetBlue();
	void SetBlue( f32 b );

	void SetEntityColor( f32 r, f32 g, f32 b );
	f32 GetEntityAlpha();
	void SetEntityAlpha( f32 alpha );

	void SetX( f32 x );
	void SetY( f32 y );
	void SetZoom( f32 zoom );

	f32 GetX();
	f32 GetY();
	void SetPosition( f32 x, f32 y );
	void SetWX( f32 x );
	void SetWY( f32 y );
	void SetAutoCenter( bool state );
	void SetAngle(f32 angle);
	void SetBlendMode( int mode );
	void SetHandleX(f32 x);
	void SetHandleY(f32 y);
	void SetName( const char * name );
	void SetParent(tlEntity * e);
	void SetRelative( bool state );
	void SetEntityScale( f32 sx, f32 sy );
	void SetSpeed( f32 speed );
	f32 GetSpeed();
	f32 GetFrameRate();
	void SetFrameRate(f32 rate);
	bool GetAnimating();
	void SetAnimating( bool state );
	//f32 GetCurrentFrame();
	//void SetCurrentFrame( f32 frame );
	//void SetSprite( u32 index );
	//void SetSprite( const char * image, u32 frame );
	//FontChar * GetSprite();

	void SetAnimateOnce( bool state );
	void SetUpdateSpeed( bool state );
	f32 GetAngle();
	f32 GetHandleX();
	f32 GetHandleY();
	bool GetHandleCenter();
	int GetBlendMode();
	bool GetRelative();
	void GetEntityScale( f32 * sx, f32 * sy );
	tlEntity * GetParent();
	tlEntity * GetChildren();
	f32 GetLifeTime();
	void SetLifeTime( f32 life );
	f32 GetAge();
	void SetAge( f32 m_Age );
	void Decay( f32 seconds );
	f32 GetWX();
	f32 GetWY();
	f32 GetEntityDirection();
	void SetEntityDirection( f32 dir );
	bool GetOKToRender( );
	void SetOKToRender( bool state );

	tlParticleManager * GetParticleManager() {return m_PM;}

public:

	tlParticleManager * m_PM;					// manager i belong to

	// Coordinates----------------------
	Vector3 m_Pos;								// 
	Vector3 m_World;
	f32 m_Zoom;									// zoom level
	bool m_Relative;							// whether the entity remains relative to it's parent. Relative is the default.

	Vector2 m_SpeedVec;

	Matrix2x2 m_Matrix;							// rot matrix.

	// Entity name----------------------
	u32 m_HashName;
	// ---------------------------------
	// Colours/Alpha--------------------
	// ---------------------------------
	Color m_Color;
	// ---------------------------------
	// Size attributes and weight-------
	// ---------------------------------
	Vector2 m_Scale;							// scale
	Vector2 m_Size;								// size

	f32 m_Width;								// width and height
	f32 m_Height;

	f32 m_Weight;								// current weight
	f32 m_Gravity;								// current speed of the drop
	f32 m_BaseWeight;							// base weight

	// ---------------------------------
	// Speed settings and variables-----
	f32 m_Speed;								// current speed
	f32 m_BaseSpeed;							// base speed of entity
	bool m_UpdateSpeed;							// Set to false to make the update method avoid updating the speed and movement
	// ---------------------------------
	// Direction and rotation-----------
	f32 m_Direction;							// current direction we're moving.
	bool m_DirectionLocked;						// Locks the direction to the edge of the effect, for edm_Age traversal

	f32 m_DirectionMoved;						// Direction last moved. (angle 0-360)

	f32 m_Angle;								// current rotation of the entity
	f32 m_RelativeAngle;						// To store the angle imposed by the parent
	// ---------------------------------

	f32 m_FrameRate;							// frameRate of the anim ( ex: 60.0f = 60 frames per second, 1 = 1 frame per second )

	//int m_AnimAction;							// how the entity should animate
	bool m_Animating;							// whether or not the entity should be animating
	bool m_AnimateOnce;							// whether the entity should animate just the once
	bool m_OkToRender;							// Set to false if you don// t want this to be rendered

	bool m_HandleCenter;						// True if the handle of the entity is at the center of the image
	Vector2 m_Handle;
	// ---------------------------------
	// life and m_Age variables-----------
	f32 m_Age;
	f32 m_LifeTime;								// measured in seconds ( pw changed from ms )

	f32 m_RepeatAgeAlpha;						// times to repear alpha over m_Age
	f32 m_RepeatAgeColor;						// times to repear color over m_Age
	int m_AlphaCycles;
	int m_ColorCycles;

	int m_Dead;
	bool m_Destroyed;

	// ---------------------------------
	// Ownerships-----------------------
	tlEntity * m_Parent;						// parent of the entity, for example bullet fired by the entity
	tlEntity * m_RootParent;					// The root parent of the entity

	// Children
	int m_ChildCount;							// count of children
	tlEntity * m_Children;						// 1st child
	tlEntity * m_PrevSibling;					// prev pointer to children iteration
	tlEntity * m_NextSibling;					// next pointer to children iteration

	// ---------------------------------
	int m_BlendMode;							// blend mode of the entity
	// ---------------------------------
	// Flags----------------------------
	bool m_RunChildren;							// When the entity is created, this is false to avoid runninng it// s children on creation to avoid recursion
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
