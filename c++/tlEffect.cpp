// 
//	File	   	-> tlEffect.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlEffect.h"

#include "time.h"

#include "tlParticleManager.h"
#include "tlEmitter.h"
#include "tlParticle.h"
#include "tlLibrary.h"

//------------------------------------------------------------------------------

tlEffect::tlEffect() : m_Next(NULL)
{
	for (int i=0; i<9; ++i)
	{
		m_InUse[i] = NULL;
	}

	m_ParentEmitter = NULL;

	m_Age = 0.0f;
	m_Parent = NULL;
	m_SpawnDirection = 1.0f;

	m_Dying = false;
	m_AllowSpawning = true;
	m_EllipseArc = 360.0f;
	m_LockAspect = true;

	m_GradientIndex = 0;
	m_IdleTime = 0;

	// these are only allocated for effects in the tlLibrary ( which are never directly used ).
	m_OwnGradients = false;
	c_Life = NULL;
	c_Amount = NULL;
	c_SizeX = NULL;
	c_SizeY = NULL;
	c_Velocity = NULL;
	c_Weight = NULL;
	c_Spin = NULL;
	c_Alpha = NULL;
	c_EmissionAngle = NULL;
	c_EmissionRange = NULL;
	c_Width = NULL;
	c_Height = NULL;
	c_Angle = NULL;
	c_Stretch = NULL;
	c_GlobalZoom = NULL;

	m_OverrideSize = false;
	m_OverrideEmissionAngle = false;
	m_OverrideEmissionRange = false;
	m_OverrideAngle = false;
	m_OverrideLife = false;
	m_OverrideAmount = false;
	m_OverrideVelocity = false;
	m_OverrideSpin = false;
	m_OverrideSizeX = false;
	m_OverrideSizeY = false;
	m_OverrideWeight = false;
	m_OverrideAlpha = false;
	m_OverrideStretch = false;
	m_OverrideGlobalZoom = false;
}

//------------------------------------------------------------------------------
// destroy effect.

tlEffect::~tlEffect()
{
}

void tlEffect::Destroy()
{
	if ( m_OwnGradients )
	{
		delete c_Life;
		delete c_Amount;
		delete c_SizeX;
		delete c_SizeY;
		delete c_Velocity;
		delete c_Weight;
		delete c_Spin;
		delete c_Alpha;
		delete c_EmissionAngle;
		delete c_EmissionRange;
		delete c_Width;
		delete c_Height;
		delete c_Angle;
		delete c_Stretch;
		delete c_GlobalZoom;
	}

	// return all tlParticle objects we're using to the unused pool.
	tlParticleManager * pm = GetParticleManager();
	for (int i=0; i<9; ++i)
	{
		while ( m_InUse[i] )
		{
			tlParticle * p = m_InUse[i];

			pm->m_UnUsedCount++;
			pm->m_InUseCount--;

			// remove from the effects InUse list
			m_InUse[i] = m_InUse[i]->m_Next;

			// add to UnUsed list ( which is managed by the particleManager )
			p->m_Next = pm->m_UnUsed;
			pm->m_UnUsed = p;

			p->Reset();
		}
	}
	// call the base destroy method.
	tlEntity::Destroy();

	// finally, delete it
	delete this;
}

//------------------------------------------------------------------------------
// Show all Emitters
// Sets all emitters to visible so that they will be rendered. This also applies to any sub effects and their emitters.
	
void tlEffect::ShowAll()
{
	tlEmitter * e = (tlEmitter *) m_Children;

	while (e)
	{
		e->ShowAll();
		e = (tlEmitter *) e->m_NextSibling;
	}
}

//------------------------------------------------------------------------------
// Hide all Emitters
// Sets all emitters to hidden so that they will no longer be rendered. This also applies to any sub effects and their emitters.
	
void tlEffect::HideAll()
{
	tlEmitter * e = (tlEmitter*) m_Children;

	while (e)
	{
		e->HideAll();
		e = (tlEmitter*) e->m_NextSibling;
	}
}

//------------------------------------------------------------------------------
// Show one Emitter
// Sets the emitter passed to the method to visible so that it will be rendered, all the other emitters are made invisible.
	
void tlEffect::ShowOne(tlEmitter * emm)
{
	tlEmitter * e = (tlEmitter*) m_Children;
	while (e)
	{
		e->SetVisible(false);
		e = (tlEmitter*) e->m_NextSibling;
	}
	emm->SetVisible(true);
}
//------------------------------------------------------------------------------
// Get count of emitters within this effect
// returns: Number of emitters
// Use this to find out how many emitters the effect has
	
int tlEffect::EmitterCount()
{
	return m_ChildCount;
}

//------------------------------------------------------------------------------
// Assign Particle manager
// Sets the Particle Manager that this effect is manm_Aged by. See #tlParticleManager
	
void tlEffect::AssignParticleManager( tlParticleManager * pm )
{
	m_PM = pm;
}
//------------------------------------------------------------------------------
// Set the class of the Effect
// Sets the effect to one of 4 types - point, area, ellipse and line. To set one of these use one of the 4 corresponding consts: tlPOINT_EFFECT, tlAREA_EFFECT, tlLINE_EFFECT, tlELLIPSE_EFFECT
	
void tlEffect::SetClass( int classType )
{
	m_Class = classType;
}
//------------------------------------------------------------------------------
// Sets m_LockAspect
// Set to true to make the size of particles scale uniformly
	
void tlEffect::SetLockAspect( bool state )
{
	m_LockAspect = state;
}
//------------------------------------------------------------------------------
// Set maximum width grid points
// In area and ellipse effects this value represents the number of grid points along the width, in the case of area and line effect, or around the 
// circumference, in the case of ellipses.
	
void tlEffect::SetMGX( int max )
{
	m_MGX = max;
}
//------------------------------------------------------------------------------
// Set maximum height grid points
// In area effects this value represents the number of grid points along the height, it has no relevence for other effect types.
	
void tlEffect::SetMGY( int max )
{
	m_MGY = max;
}
//------------------------------------------------------------------------------
// Sets whether the effect should emit at points
// if (set to true then the particles within the effect will emit from evenly spaced points with the area, line or ellipse. The number of points is determined
// by <i>mgx</i> and <i>mgy</i>. The value is not applicable to point effects.
	
void tlEffect::SetEmitAtPoints( bool state )
{
	m_EmitAtPoints = state;
}
//------------------------------------------------------------------------------
// Set the emission type
// In area, line and ellipse effects the emission type determines the direction that the particles travel once spawned. Use the following consts to determine
// the direction:
// <b>tlEMISSION_INWARDS: </b>Particles will emit towards the handle of the effect.<br/>
// <b>tlEMISSION_OUTWARDS: </b>Particles will emit away from the handle of the effect.<br/>
// <b>tlEMISSION_SPECIFIED: </b>Particles will emit in the direction specified by the <i>emission_angle</i> and <i>emission_range</i> attributes.<br/>
// <b>tlEMISSION_IN_AND_OUT: </b>Particles will alternative between emitting towards and away from the handle of the effect.
	
void tlEffect::SetEmissionType( int type )
{
	m_EmissionType = type;
}
//------------------------------------------------------------------------------
// Set the length of the effect
// Effects can be looped by setting the effect length. Just pass it the length in seconds that you want it to loop by or set to 0 if 
// you don't want the effect to loop.
	
void tlEffect::SetEffectLength( f32 seconds )
{
	m_EffectLength = seconds;
}
//------------------------------------------------------------------------------
// Set the parent emitter
// Effects can be sub effects within effects. To do this emitters can store a list of effects that they attach to particles they spawn. This sets the emitter
// that the effect is parented to.
	
void tlEffect::SetParentEmitter( tlEmitter * parent )
{
	m_ParentEmitter = parent;
}
//------------------------------------------------------------------------------
// Set to true for particles to traverse line type effects
// Only applying to line effects, setting this to true makes the particles travel along the length of the line always remaining relative to it.
	
void tlEffect::SetTraverseEdge( bool state )
{
	m_TraverseEdge = state;
}
//------------------------------------------------------------------------------
// Set the end behaviour of particles traversing a line
// if (an effect if set so that particles traverse the edge of the line, then this makes the particles behave in one of 3 ways when they reach 
// the end of the line.  By passing it either of the following const they can:
// <b>tlEND_LOOPAROUND</b>: The particles will loop back round to the beginning of the line.<br/>
// <b>tlEND_KILL</b>: The particles will be killed even if they haven't reached the end of their lifetimes yet.<br/>
// <b>tlLET_FREE</b>: The particles will be free to continue on their merry way.
	
void tlEffect::SetEndBehaviour( int value )
{
	m_EndBehaviour = value;
}
//------------------------------------------------------------------------------
// Set to true to make the distance travelled determined by the life of the particle.
// When <i>traverseedge</i> is set to true and <i>endbehaviour</i> is set to true then the distance travelled along the line will be determined by the 
// m_Age of the particle.
	
void tlEffect::SetDistanceSetByLife( bool state )
{
	m_DistanceSetByLife = state;
}
//------------------------------------------------------------------------------
// Sets the x handle of the effect
// This effects where the effect will be placed on screen. A value of 0 represents the left of the effect.
	
void tlEffect::SetHandleX( f32 value )
{
	m_Handle.x = value;
}
//------------------------------------------------------------------------------
// Sets the y handle of the effect
// This effects where the effect will be placed on screen. A value of 0 represents the top of the effect.
	
void tlEffect::SetHandleY( f32 value )
{
	m_Handle.y = value;
}
//------------------------------------------------------------------------------
// Sets to true to center the handle of the effect
// if (set to true then then position of the handle is automatically set to to the center of the effect.
	
void tlEffect::SetHandleCenter( bool state )
{
	m_HandleCenter = state;
}
//------------------------------------------------------------------------------
// Set the order particles spawn
// A vlaue of true means that in area, line and ellipse effects, particles will spawn from right to left or anti-clockwise.
	
void tlEffect::SetReverseSpawn( bool state )
{
	m_ReverseSpawn = state;
}
//------------------------------------------------------------------------------
// This sets the direction particles are spawned.
// theres no need to call this, as its called internally by the emitter depending on the reverse spawn flag. see #setreversespawn.
	
void tlEffect::SetSpawnDirection()
{
	if (m_ReverseSpawn)
		m_SpawnDirection = -1.0f;
	else
		m_SpawnDirection = 1.0f;
}
//------------------------------------------------------------------------------
// Set the effects particle manager
// Every effect needs a particle manager. For more info see #tlParticleManager
	
void tlEffect::SetParticleManager( tlParticleManager * pm )
{
	m_PM = pm;
}
//------------------------------------------------------------------------------
// Set the area size of the effect
// For area and ellipse effects, use this function to override the graph and set the width and height of the area to whatever you want.
	
void tlEffect::SetAreaSize( f32 width, f32 height )
{
	m_OverrideSize = true;

	m_CurrentWidth = width;
	m_CurrentHeight = height;
}
//------------------------------------------------------------------------------
// Set the line length of the effect
// For line effects, use this function to override the graph and set the length of the line to whatever you want.
	
void tlEffect::SetLineLength( f32 length )
{
	m_OverrideSize = true;
	m_CurrentWidth = length;
}
//------------------------------------------------------------------------------
// Set the Emission Angle of the effect
// This overides whatever angle is set on the graph and sets the emission angle of the effect. This won't effect emitters that have <i>UseEffectEmission</i> set
// to FALSE.
	
void tlEffect::SetEmissionAngle( f32 angle )
{
	m_OverrideEmissionAngle = true;
	m_CurrentEmissionAngle = angle;
}
//------------------------------------------------------------------------------
// Set the Angle of the effect
// This overides the whatever angle is set on the graph and sets the angle of the effect.
	
void tlEffect::SetEffectAngle( f32 angle )
{
	m_OverrideAngle = true;
	m_Angle = angle;
}
//------------------------------------------------------------------------------
// Set the Global attribute Life of the effect
// This overides the graph the effect uses to set the Global Attribute Life
	
void tlEffect::SetLife( f32 life )
{
	m_OverrideLife = true;
	m_CurrentLife = life;
}
//------------------------------------------------------------------------------
// Set the Global attribute Amount of the effect
// This overides the graph the effect uses to set the Global Attribute Amount
	
void tlEffect::SetAmount( f32 amount )
{
	m_OverrideAmount = true;
	m_CurrentAmount = amount;
}
//------------------------------------------------------------------------------
// Set the Global attribute velocity of the effect
// This overides the graph the effect uses to set the Global Attribute velocity
	
void tlEffect::SetVelocity( f32 velocity )
{
	m_OverrideVelocity = true;
	m_CurrentVelocity = velocity;
}
//------------------------------------------------------------------------------
// Set the Global attribute Spin of the effect
// This overides the graph the effect uses to set the Global Attribute Spin
	
void tlEffect::SetSpin( f32 spin )
{
	m_OverrideSpin = true;
	m_CurrentSpin = spin;
}
//------------------------------------------------------------------------------
// Set the Global attribute Weight of the effect
// This overides the graph the effect uses to set the Global Attribute Weight
	
void tlEffect::SetWeight( f32 weight )
{
	m_OverrideWeight = true;
	m_CurrentWeight = weight;
}
//------------------------------------------------------------------------------
// Set the Global attribute Sizex of the effect
// This overides the graph the effect uses to set the Global Attribute Sizex and sizey
	
void tlEffect::SetEffectParticleSize( f32 sizex, f32 sizey )
{
	m_OverrideSizeX = true;
	m_OverrideSizeY = true;
	m_CurrentSizeX = sizex;
	m_CurrentSizeY = sizey;
}
//------------------------------------------------------------------------------
// Set the Global attribute Sizex of the effect
// This overides the graph the effect uses to set the Global Attribute Sizex
	
void tlEffect::SetSizeX( f32 sizex )
{
	m_OverrideSizeX = true;
	m_CurrentSizeX = sizex;
}
//------------------------------------------------------------------------------
// Set the Global attribute Sizey of the effect
// This overides the graph the effect uses to set the Global Attribute Sizey
	
void tlEffect::SetSizeY( f32 sizey )
{
	m_OverrideSizeY = true;
	m_CurrentSizeY = sizey;
}
//------------------------------------------------------------------------------
// Set the Global attribute Alpha of the effect
// This overides the graph the effect uses to set the Global Attribute Alpha
	
void tlEffect::SetEffectAlpha( f32 alpha )
{
	m_OverrideAlpha = true;
	m_CurrentAlpha = alpha;
}
//------------------------------------------------------------------------------
// Set the Global attribute EmissionRange of the effect
// This overides the graph the effect uses to set the Global Attribute EmissionRange
	
void tlEffect::SetEffectEmissionRange( f32 emissionRange )
{
	m_OverrideEmissionRange = true;
	m_CurrentEmissionRange = emissionRange;
}
//------------------------------------------------------------------------------
// Set range in degrees of the arc
// When an effect uses an ellipse as its effect type, you can adjust how far round the ellipse particles will spawn
// by setting the ellipse arc. 360 degrees will spawn around the full amount.
	
void tlEffect::SetEllipseArc( f32 degrees )
{
	m_EllipseArc = degrees;
	m_EllipseOffset = 90.0f - (degrees * 0.5f);
}
//------------------------------------------------------------------------------
// Set the current zoom level of the effect
// This overides the graph the effect uses to set the Global Attribute Global Zoom

void tlEffect::SetZoom( f32 zoom )
{
	m_OverrideGlobalZoom = true;
	m_Zoom = zoom * m_PM->m_Scale;
}
//------------------------------------------------------------------------------
// Set the Global attribute Stretch of the effect
// This overides the graph the effect uses to set the Global Attribute Stretch
	
void tlEffect::SetStretch( f32 stretch )
{
	m_OverrideStretch = true;
	m_CurrentStretch = stretch;
}
//------------------------------------------------------------------------------
// Sets the current state of whether spawned particles are added to the particle managers pool, or the emitters own pool. 
// true means that they're grouped together under each emitter. 
// This will change all emitters with the effect, and is recommended you use this rather then individually
// for each emitter.
	
void tlEffect::SetGroupParticles( bool state )
{
	tlEmitter * e = (tlEmitter*) m_Children;
	while (e)
	{
		e->SetGroupParticles( state );

		tlEffect * ef = e->m_Effects;
		while (ef)
		{
			ef->SetGroupParticles( state );
			ef = ef->m_Next;
		}
		e = (tlEmitter*) e->m_NextSibling;
	}
}
//------------------------------------------------------------------------------
// Get class
// returns: The current class of the effect - tlAREA_EFFECT, tlLINE_EFFECT, tlELLIPSE_EFFECT or tlPOINT_EFFECT
	
int tlEffect::GetClass()
{
	return m_Class;
}
//------------------------------------------------------------------------------
// returns the <i>m_LockAspect</i> 
// returns: Either TRUE or FALSE
	
bool tlEffect::GetLockAspect()
{
	return m_LockAspect;
}
//------------------------------------------------------------------------------
// Get the current maximum grid points along the width
	
int tlEffect::GetMGX()
{
	return m_MGX;
}
//------------------------------------------------------------------------------
// Get the current maximum grid points along the height
	
int tlEffect::GetMGY()
{
	return m_MGY;
}
//------------------------------------------------------------------------------
// Get wheter the effect is currently set to emit at points
// returns: Either TRUE or FALSE
	
int tlEffect::GetEmitAtPoints()
{
	return m_EmitAtPoints;
}
//------------------------------------------------------------------------------
// Get the current emission type
// returns: The current emission type: tlEMISSION_INWARDS, tlEMISSION_OUTWARDS, tlEMISSION_SPECIFIED, tlEMISSION_IN_AND_OUT
	
int tlEffect::GetEmissionType()
{
	return m_EmissionType;
}
//------------------------------------------------------------------------------
// Get the effect length
// returns: Length in seconds
	
f32 tlEffect::GetEffectLength()
{
	return m_EffectLength;
}
//------------------------------------------------------------------------------
// Get the parent emitter of the effect
	
tlEmitter * tlEffect::GetParentEmitter()
{
	return m_ParentEmitter;
}
//------------------------------------------------------------------------------
// Get the parent entity of the effect
	
tlEntity * tlEffect::GetParent()
{
	return m_Parent;
}
//------------------------------------------------------------------------------
// Get whether particles should traverse the line (if it's a line effect)
// returns: Either TRUE or FALSE
	
bool tlEffect::GetTraverseEdge()
{
	return m_TraverseEdge;
}
//------------------------------------------------------------------------------
// Gets the end behaviour for when particles reach the end of the line
// returns: Either tlEND_KILL, tlEND_LOOPAROUND, tlEND_LETFREE
	
int tlEffect::GetEndBehaviour()
{
	return m_EndBehaviour;
}
//------------------------------------------------------------------------------
// Gets whether the distance along the traversed line is determined by the particle m_Age
// returns: Either TRUE or FALSE
	
bool tlEffect::GetDistanceSetByLife()
{
	return m_DistanceSetByLife;
}
//------------------------------------------------------------------------------
// Get whether the effect's handle is automatically set to center
// returns: Either TRUE or FALSE
	
bool tlEffect::GetHandleCenter()
{
	return m_HandleCenter;
}
//------------------------------------------------------------------------------
// Gets whether the particles should spawn in the opposite direction
// returns: Either TRUE or FALSE
	
bool tlEffect::GetReverseSpawn()
{
	return m_ReverseSpawn;
}
//------------------------------------------------------------------------------
// Gets the current number of particles spawned by this effects' emitters including any sub effects
	
int tlEffect::GetParticleCount()
{
	int particlecount = 0;

	tlEmitter * e = (tlEmitter*) m_Children;
	while (e)
	{
		tlParticle * p = (tlParticle*) e->m_Children;
		while (p)
		{
			tlEffect * eff = (tlEffect*) p->m_Children;
			while (eff)
			{
				particlecount += eff->GetParticleCount();
				eff = (tlEffect*) eff->m_NextSibling;
			}
			p = (tlParticle*) p->m_NextSibling;
		}

		particlecount += e->m_ChildCount;
		e = (tlEmitter*) e->m_NextSibling;
	}
	return particlecount;
}
//------------------------------------------------------------------------------
// get the range in degrees of the arc
// see #SetEllipseArc
	
f32 tlEffect::GetEllipseArc()
{
	return m_EllipseArc;
}
//------------------------------------------------------------------------------	

bool tlEffect::HasParticles()
{
	tlEmitter * e = (tlEmitter*) m_Children;
	while ( e )
	{
		if (e->m_ChildCount)
			return true;
		e = (tlEmitter*) e->m_NextSibling;
	}
	return false;
}
//------------------------------------------------------------------------------
// Add a new effect to the directory including any sub effects and emitters. Effects are stored using a map and can be retrieved using #GetEffect.
	
void tlEffect::AddEffect( tlEffect * e )
{
	tlEmitter * em = (tlEmitter*) e->m_Children;
	while (em)
	{
		AddEmitter(em);
		em = (tlEmitter*) em->m_NextSibling;
	}
}
//------------------------------------------------------------------------------
// Add a new emitter to the directory. Emitters are stored using a map and can be retrieved using #GetEmitter. Generally you don't want to call this at all, 
// just use #AddEffect and all its emitters will be added also.
	
void tlEffect::AddEmitter( tlEmitter * e )
{
	tlEffect * ef = e->m_Effects;
	while (ef)
	{
		AddEffect(ef);
		ef = ef->m_Next;
	}
}
//------------------------------------------------------------------------------
// Updates the effect
// Call this once every frame to update the effect. Updating effects is handled by the Particle Manager unless you want to manm_Age things on your own.
	
void tlEffect::Update( )
{
	f32 dTime = TimeGetElapsedSeconds();

	// update the age of this effect.
	m_Age += dTime;
	
	if (m_EffectLength > 0.0f)
	{
		if (m_Age > m_EffectLength)
		{
			m_Age = 0.0f;
		}
	}
	
	// compute the gradient index for all gradient table lookups.
	m_GradientIndex = (int) (m_Age * GetParticleManager()->m_LookupFreq);
	
	if (m_OverrideSize == false)
	{
		switch ( m_Class )
		{
		case tlPOINT_EFFECT:
			m_CurrentWidth = 0.0f;
			m_CurrentHeight = 0.0f;
			break;
		case tlAREA_EFFECT:
		case tlELLIPSE_EFFECT:
			m_CurrentWidth = c_Width->Get(m_GradientIndex);
			m_CurrentHeight = c_Height->Get(m_GradientIndex);
			break;
		case tlLINE_EFFECT:
			m_CurrentWidth = c_Width->Get(m_GradientIndex);
			m_CurrentHeight = 0.0f;
			break;
		}
	}
	
	// can be optimised
	if (m_HandleCenter && (m_Class != tlPOINT_EFFECT))
	{
		m_Handle.x = 0.5f * m_CurrentWidth;
		m_Handle.y = 0.5f * m_CurrentHeight;
	}
	else if (m_HandleCenter)
	{
		m_Handle.x = 0.0f;
		m_Handle.y = 0.0f;
	}

	if (HasParticles())
	{
		m_IdleTime = 0; // in ticks
	}
	else
	{
		m_IdleTime++;
	}

	if (m_ParentEmitter)
	{
		if (m_OverrideLife == false)
			m_CurrentLife = c_Life->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentLife;

		if (m_OverrideAmount == false)
			m_CurrentAmount = c_Amount->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentAmount;

		if (m_LockAspect)
		{
			if (m_OverrideSizeX == false)
				m_CurrentSizeX = c_SizeX->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentSizeX;
			if (m_OverrideSizeY == false)
				m_CurrentSizeY = m_CurrentSizeX * m_ParentEmitter->m_ParentEffect->m_CurrentSizeY;
		}
		else
		{
			if (m_OverrideSizeX == false)
				m_CurrentSizeX = c_SizeX->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentSizeX;
			if (m_OverrideSizeY == false)
				m_CurrentSizeY = c_SizeY->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentSizeY;
		}
		if (m_OverrideVelocity == false)
			m_CurrentVelocity = c_Velocity->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentVelocity;

		if (m_OverrideWeight == false)
			m_CurrentWeight = c_Weight->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentWeight;

		if (m_OverrideSpin == false)
			m_CurrentSpin = c_Spin->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentSpin;

		if (m_OverrideAlpha == false)
			m_CurrentAlpha = c_Alpha->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentAlpha;

		if (m_OverrideEmissionAngle == false)
			m_CurrentEmissionAngle = c_EmissionAngle->Get(m_GradientIndex);

		if (m_OverrideEmissionRange == false)
			m_CurrentEmissionRange = c_EmissionRange->Get(m_GradientIndex);

		if (m_OverrideAngle == false)
			m_Angle = c_Angle->Get(m_GradientIndex);

		if (m_OverrideStretch == false)
			m_CurrentStretch = c_Stretch->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentStretch;

		if (m_OverrideGlobalZoom == false)
			m_CurrentGlobalZoom = c_GlobalZoom->Get(m_GradientIndex) * m_ParentEmitter->m_ParentEffect->m_CurrentGlobalZoom;
	}
	else
	{
		if (m_OverrideLife == false)
			m_CurrentLife = c_Life->Get(m_GradientIndex);

		if (m_OverrideAmount == false)
			m_CurrentAmount = c_Amount->Get(m_GradientIndex);

		if (m_LockAspect)
		{
			if (m_OverrideSizeX == false)
				m_CurrentSizeX = c_SizeX->Get(m_GradientIndex);
			if (m_OverrideSizeY == false)
				m_CurrentSizeY = m_CurrentSizeX;
		}
		else
		{
			if (m_OverrideSizeX == false)
				m_CurrentSizeX = c_SizeX->Get(m_GradientIndex);

			if (m_OverrideSizeY == false)
				m_CurrentSizeY = c_SizeY->Get(m_GradientIndex);
		}
		if (m_OverrideVelocity == false)
			m_CurrentVelocity = c_Velocity->Get(m_GradientIndex);

		if (m_OverrideWeight == false)
			m_CurrentWeight = c_Weight->Get(m_GradientIndex);

		if (m_OverrideSpin == false)
			m_CurrentSpin = c_Spin->Get(m_GradientIndex);

		if (m_OverrideAlpha == false)
			m_CurrentAlpha = c_Alpha->Get(m_GradientIndex);

		if (m_OverrideEmissionAngle == false)
			m_CurrentEmissionAngle = c_EmissionAngle->Get(m_GradientIndex);

		if (m_OverrideEmissionRange == false)
			m_CurrentEmissionRange = c_EmissionRange->Get(m_GradientIndex);

		if (m_OverrideAngle == false)
			m_Angle = c_Angle->Get(m_GradientIndex);

		if (m_OverrideStretch == false)
			m_CurrentStretch = c_Stretch->Get(m_GradientIndex);

		if (m_OverrideGlobalZoom == false)
			m_CurrentGlobalZoom = c_GlobalZoom->Get(m_GradientIndex);
	}
	
	if (m_OverrideGlobalZoom == false)
	{
		m_Zoom = m_CurrentGlobalZoom;
	}
	
	if (m_CurrentWeight == 0.0f)
	{
		m_Bypass_Weight = true;
	}
	
	if (m_ParentEmitter)
		m_Dying = m_ParentEmitter->m_Dying;
	
	tlEntity::Update( );
	
	if (m_IdleTime > GetParticleManager()->m_IdleTimeLimit)
	{
		m_Dead = true;
	}
	
	if (m_Dead)
	{
		if (m_ChildCount == 0)
		{
			if (m_Parent)
			{
				m_Parent->RemoveChild(this);
			}
			// tag it, so it will get destroyed
			m_Destroyed = true;
		}
		else
		{
			KillChildren();
		}
	}
}

//------------------------------------------------------------------------------
// Softly kill an effect
// Call this to kill an effect by stopping it from spawning any more particles. This will make the effect slowly die about as any remaining 
// particles cease to exist. Any single particles are converted to one shot particles.
	
void tlEffect::SoftKill()
{
	m_Dying = true;
}
//------------------------------------------------------------------------------
// Hard kill an effect
// immediatley kills an effect by destroying all particles created by it.
	
void tlEffect::HardKill()
{
	GetParticleManager()->RemoveEffect(this);
	Destroy();
}

//------------------------------------------------------------------------------

f32 tlEffect::GetLongestLife()
{
	f32 longestLife = 0.0f;

	tlEmitter * e = (tlEmitter *) m_Children;
	while (e)
	{
		f32 temp = e->GetLongestLife();

		if ( temp > longestLife )
			longestLife = temp;

		e = (tlEmitter *)e->m_NextSibling;
	}
	return longestLife;
}

//------------------------------------------------------------------------------
// Compilers
// Pre-Compile all attributes.
// In order to use look-up arrays to access attribute values over the course of the effects life you need to compile all of the attribute values
// into an array. This method will compile all of them together in one go including all of it children emitters and any sub effects and so on.
	
void tlEffect::Compile_All( )
{
	f32 freq = GetParticleManager()->m_LookupFreq;

	//PRINTF("BUILDING effect lookup size\n" );

	c_Life->BuildLookup( freq );
	c_Amount->BuildLookup( freq );
	c_SizeX->BuildLookup( freq );
	c_SizeY->BuildLookup( freq );
	c_Velocity->BuildLookup( freq );
	c_Weight->BuildLookup( freq );
	c_Spin->BuildLookup( freq );
	c_Alpha->BuildLookup( freq );
	c_EmissionAngle->BuildLookup( freq );
	c_EmissionRange->BuildLookup( freq );
	c_Width->BuildLookup( freq );
	c_Height->BuildLookup( freq );
	c_Angle->BuildLookup( freq );
	c_Stretch->BuildLookup( freq );
	c_GlobalZoom->BuildLookup( freq );

	tlEmitter * e = (tlEmitter *) m_Children;
	while ( e )
	{
		e->Compile_All( );
		e = (tlEmitter *) e->m_NextSibling;
	}
}
