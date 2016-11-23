// 
//	File	   	-> tlParticle.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlParticle.h"
#include "tlParticleManager.h"
#include "tlEntity.h"
#include "tlEmitter.h"

#include "Time.h"

//------------------------------------------------------------------------------

tlParticle::tlParticle()
{
	Reset();
}

//------------------------------------------------------------------------------

tlParticle::~tlParticle()
{
}

//------------------------------------------------------------------------------

void tlParticle::Destroy()
{
	// reset particle to default values.
	Reset();

	// put particle back into the unused pool
	m_PM->ReleaseParticle(this);

	// call the base destroy method.
	tlEntity::Destroy();
}

//------------------------------------------------------------------------------
// Get the m_CurrentFrame of the entity sprite animation

f32 tlParticle::GetCurrentFrame()
{
	return m_CurrentFrame;
}
//------------------------------------------------------------------------------
// Set the m_CurrentFrame of the entity sprite animation

void tlParticle::SetCurrentFrame( f32 frame )
{
	m_CurrentFrame = frame;

	Font * font = GetParticleManager()->m_FontAtlas[0];
	u32 glyph = m_Emitter->m_BaseFrame + (u32)m_CurrentFrame;
	m_Avatar = &font->m_FontFile->charArray[ glyph ];
}

//------------------------------------------------------------------------------
// Updates the particle.
// This is called by the emitter the particle is parented to.
	
void tlParticle::Update( )
{
	f32 dTime = TimeGetElapsedSeconds();
	m_Age += dTime;

	if ( (m_Emitter->m_Dying == true) || (m_Emitter->m_OneShot == true) || (m_Dead == true) )
	{
		m_ReleaseSingleParticle = true;
	}

	if ( (m_Emitter->m_SingleParticle) && (m_ReleaseSingleParticle==false) )
	{
		if (m_Age > m_LifeTime)
		{
			m_Age = 0.0f;
		}
	}
		
	tlEntity::Update();
				
	if ( (m_Age > m_LifeTime) || (m_Dead == 2) )		// if dead=2 then that means its reached the end of the line (in kill mode) for line traversal effects
	{
		m_Dead = true;

		if (m_ChildCount == 0)
		{
			m_PM->ReleaseParticle(this);
			m_Parent->RemoveChild(this);
			Reset();
		}
		else
		{
			m_Emitter->ControlParticle(this);
			KillChildren();
		}
		return;
	}

	m_Emitter->ControlParticle(this);
}
//------------------------------------------------------------------------------
// Resets the particle so it's ready to be recycled by the particle manager
	
void tlParticle::Reset()
{
	m_Age = 0.0f;
	m_World.x = 0.0f;
	m_World.y = 0.0f;
	m_Avatar = NULL;
	m_Dead = false;
	m_SpinVariation = 0.0f;
	m_DirectionVariation = 0.0f;

	m_Angle = 0.0f;
	m_RelativeAngle = 0.0f;

	m_Zoom = 1.0f;

	m_Direction = 0.0f;
	m_DirectionLocked = false;

	m_RandomSpeed = 0.0f;
	m_RandomDirection = 0.0f;
	m_Parent = NULL;
	m_RootParent = NULL;
	m_AlphaCycles = 0;
	m_ColorCycles = 0;
	m_RepeatAgeAlpha = 0;
	m_RepeatAgeColor = 0;
	m_ReleaseSingleParticle = false;
	m_Gravity = 0.0f;
	m_Weight = 0.0f;
	m_Emitter = NULL;
	m_Avatar = NULL;
	m_TimeTracker = 0.0f;

	ClearChildren();
}
//------------------------------------------------------------------------------
