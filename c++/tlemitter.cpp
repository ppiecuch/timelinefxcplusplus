// 
//	File	   	-> tlEmitter.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlemitter.h"
#include "tlparticlemanager.h"
#include "tleffect.h"
#include "tlparticle.h"
#include "tllibrary.h"

//------------------------------------------------------------------------------

tlEmitter::tlEmitter()
{
	m_Parent = NULL;
	m_Age = 0.0f;

	m_AngleOffset = 0.0f;
	m_LockedAngle = false;
	m_Counter = 0.0f;

	m_Effects = NULL;

	m_Dying = false;
	m_Once = false;
	m_GroupParticles = false;
	m_Visible = true;

	m_DirAlternater = false;
	m_OneShot = false;
	m_ParticlesRelative = false;

	m_StartedSpawning = false;

//	m_Current_Weight = 0.0f;
//	m_Current_WeightVariation = 0.0f;
//	m_Current_Speed = 0.0f;
//	m_Current_SpeedVariation = 0.0f;
//	m_Current_Spin = 0.0f;
//	m_Current_SpinVariation = 0.0f;
//	m_Current_DirectionVariation = 0.0f;
//	m_Current_EmissionAngle = 0.0f;
//	m_Current_EmissionRange = 0.0f;

	m_OwnGradients = false;

	c_R = NULL;
	c_G = NULL;
	c_B = NULL;
	c_BaseSpin = NULL;
	c_Spin = NULL;
	c_SpinVariation = NULL;
	c_Velocity = NULL;
	c_BaseWeight = NULL;
	c_Weight = NULL;
	c_WeightVariation = NULL;
	c_BaseSpeed = NULL;
	c_VelVariation = NULL;
	c_Alpha = NULL;
	c_SizeX = NULL;
	c_SizeY = NULL;
	c_ScaleX = NULL;
	c_ScaleY = NULL;
	c_SizeXVariation = NULL;
	c_SizeYVariation = NULL;
	c_LifeVariation = NULL;
	c_Life = NULL;
	c_Amount = NULL;
	c_AmountVariation = NULL;
	c_EmissionAngle = NULL;
	c_EmissionRange = NULL;
	c_GlobalVelocity = NULL;
	c_Direction = NULL;
	c_DirectionVariation = NULL;
	c_DirectionVariationOT = NULL;
	c_FrameRate = NULL;
	c_Stretch = NULL;
	c_Splatter = NULL;
}
//------------------------------------------------------------------------------

tlEmitter::~tlEmitter()
{
}

//------------------------------------------------------------------------------

void tlEmitter::Destroy()
{
	if ( m_OwnGradients )
	{
		delete c_R;
		delete c_G;
		delete c_B;
		delete c_BaseSpin;
		delete c_Spin;
		delete c_SpinVariation;
		delete c_Velocity;
		delete c_BaseWeight;
		delete c_Weight;
		delete c_WeightVariation;
		delete c_BaseSpeed;
		delete c_VelVariation;
		delete c_Alpha;
		delete c_SizeX;
		delete c_SizeY;
		delete c_ScaleX;
		delete c_ScaleY;
		delete c_SizeXVariation;
		delete c_SizeYVariation;
		delete c_LifeVariation;
		delete c_Life;
		delete c_Amount;
		delete c_AmountVariation;
		delete c_EmissionAngle;
		delete c_EmissionRange;
		delete c_GlobalVelocity;
		delete c_Direction;
		delete c_DirectionVariation;
		delete c_DirectionVariationOT;
		delete c_FrameRate;
		delete c_Stretch;
		delete c_Splatter;
	}

	m_ParentEffect = NULL;

	// delete all effects used by this emitter.
	tlEffect * e = m_Effects;
	while ( e )
	{
		tlEffect * n = e->m_Next;
		e->Destroy();
		e = n;
	}

	// call the base destroy method.
	tlEntity::Destroy();

	// finally, delete it
	delete this;
}
//------------------------------------------------------------------------------
//Show all Emitters
// Sets all emitters to visible so that they will be rendered. This also applies to any sub effects and their emitters.
	
void tlEmitter::ShowAll()
{
	m_Visible = true;

	tlEffect * e = m_Effects;
	while (e)
	{
		e->ShowAll();
		e = e->m_Next;
	}
}
//------------------------------------------------------------------------------
// Hide all Emitters
// Sets all emitters to hidden so that they will no longer be rendered. This also applies to any sub effects and their emitters.
	
void tlEmitter::HideAll()
{
	m_Visible = false;

	tlEffect * e = m_Effects;
	while (e)
	{
		e->HideAll();
		e = e->m_Next;
	}
}
//------------------------------------------------------------------------------
// Add an effect to the emitters list of effects.
// Effects that are in the effects list are basically sub effects that are added to any particles that this emitter spawns which in turn should
// contain their own emitters that spawn more particles and so on.
	
void tlEmitter::AddEffect( tlEffect * e )
{
	e->m_Next = m_Effects;
	m_Effects = e;
}
//------------------------------------------------------------------------------
// Set Parent Effect
// Assigns the effect that is the parent to this emitter
	
void tlEmitter::SetParentEffect( tlEffect * parent )
{
	m_ParentEffect = parent;
}
//------------------------------------------------------------------------------
// Set the image frame
// if (the image has more then one frame then setting this can determine which frame the particle uses to draw itself.
	
void tlEmitter::SetFrame( int frame )
{
	m_Frame = frame;
}
//------------------------------------------------------------------------------
// Set the angle offset or Variation
// Depending on the value of %angletype (tlANGLE_ALIGN, tlANGLE_RANDOM or tlANGLE_SPECIFY), this will either set the angle offset of the particle in the 
// case of tlANGLE_ALIGN and tlANGLE_SPECIFY, or act as the range of degrees for tlANGLE_RANDOM.
	
void tlEmitter::SetAngleOffset( f32 offset )
{
	m_AngleOffset = offset;
}
//------------------------------------------------------------------------------
// Set Uniform
// Dictates whether the particles size scales uniformally. Set to either TRUE or FALSE.
	
void tlEmitter::SetUniform( bool state )
{
	m_Uniform = state;
}
//------------------------------------------------------------------------------
// Set the angle type
// Angle type tells the particle how it show orientate itself when spawning. Either tlANGLE_ALIGN, tlANGLE_RANDOM or tlANGLE_SPECIFY.
// @tlANGLE_ALIGN: Force the particle to align itself with the direction that it's travelling in.<br/>
// @tlANGLE_RANDOM: Choose a random angle.<br/>
// @tlANGLE_SPECIFY: Specify the angle that the particle spawns with.
// Use %angleoffset to control the either both the specific angle, random range of angles and an offset if aligning.
	
void tlEmitter::SetAngleType( int type )
{
	m_AngleType = type;
}
//------------------------------------------------------------------------------
// Set Use effect emission
// Set to TRUE by default, this tells the emitter to take the emission range and emission angle attributes from the parent effect, otherwise if set to FALSE it
// will take the values from the emitters own emission attributes.
	
void tlEmitter::SetUseEffectEmission( bool state )
{
	m_UseEffectEmission = state;
}
//------------------------------------------------------------------------------
// Set to FALSE to stop drawing the particles this emitter spawns
	
void tlEmitter::SetVisible( bool state )
{
	m_Visible = state;
}
//------------------------------------------------------------------------------
// Set Single Particle
// You can have particles that do not age and will only be spawned once for point emitters, or just for one frame with area, line and ellipse emitters.
// Single particles will remain until they are destroyed and will one behave according the values stored in the first temmiterchange nodes - in otherwords they
// will not change at all over time.
	
void tlEmitter::SetSingleParticle( bool state )
{
	m_SingleParticle = state;
}
//------------------------------------------------------------------------------
// Sets whether the particle chooses random colour from the colour attributes
	
void tlEmitter::SetRandomColor( bool state )
{
	m_RandomColor = state;
}
//------------------------------------------------------------------------------
// Set the z layer
// Emitters can be set to draw on different layers depending on what kind of effect you need. By default everything is drawn on layer 0, higher layers
// makes those particles spawned by that emitter drawn on top of emitters below them in layers. The layer value can range from 0-8 giving a total of 9 layers.
	
void tlEmitter::SetZLayer( int value )
{
	m_ZLayer = value;
}
//------------------------------------------------------------------------------
// Set whether the particle should animate
// Only applies if the particle's image has more then one frame of animation.
	
void tlEmitter::SetAnimate( bool state )
{
	m_Animate = state;
}
//------------------------------------------------------------------------------
// Set the particles to spawn with a random frame
// Only applies if the particle has more then one frame of animation
	
void tlEmitter::SetRandomStartFrame( bool state )
{
	m_RandomStartFrame = state;
}
//------------------------------------------------------------------------------
// Set the direction the animation plays in
// Set to 1 for forwards playback and set to -1 for reverse playback of the image aniamtion
	
void tlEmitter::SetAnimationDirection( f32 dir )
{
	m_AnimationDirection = dir;
}
//------------------------------------------------------------------------------
// Set to the number of times the colour should cycle within the particle lifetime
// Timeline Particles editor allows values from 1 to 10. 1 is the default.
	
void tlEmitter::SetColorRepeat( bool state )
{
	m_ColorRepeat = state;
}
//------------------------------------------------------------------------------
// Set to the number of times the alpha of the particle should cycle within the particle lifetime.
// Timeline Particles editor allows values from 1 to 10. 1 is the default.
	
void tlEmitter::SetAlphaRepeat( bool state )
{
	m_AlphaRepeat = state;
}
//------------------------------------------------------------------------------
// Make a particle a one shot particle or not.
// Emitters that have this set to true will only spawn one particle and that particle will just play out once and die. The is only relevant if
// %singleparticle is also set to true.
	
void tlEmitter::SetOneShot( bool state )
{
	m_OneShot = state;
}
//------------------------------------------------------------------------------
// Set the handle of the particle to its center
// Set to TRUE for the hande to be placed automatically at the center of the particle, or FALSE for the handle to be dictated by %m_Handle.x and %m_Handle.y.
	
void tlEmitter::SetHandleCenter( bool state )
{
	m_HandleCenter = state;
}
//------------------------------------------------------------------------------
// Set wheter the particles and emitter remain relative to the effect.
// Emitters that are relative spawn particles that move and rotate with the effect they're contained in.
	
void tlEmitter::SetParticlesRelative( bool state )
{
	m_ParticlesRelative = state;
}
//------------------------------------------------------------------------------
// Set to TRUE to make th particles spawned have their angle of rotation locked to direction
	
void tlEmitter::SetLockAngle( bool state )
{
	m_LockedAngle = state;
}
//------------------------------------------------------------------------------
// Set to TRUE to make th particles spawned have their angle of rotation relative to the parent effect
	
void tlEmitter::SetAngleRelative( bool state )
{
	m_AngleRelative = state;
}
//------------------------------------------------------------------------------
// Set to TRUE to make the particles spawned playback the animation just once 
	
void tlEmitter::SetOnce( bool state )
{
	m_Once = state;
}
//------------------------------------------------------------------------------
// Sets the current state of whether spawned particles are added to the particle managers pool, or the emitters own pool. true means that
// they're grouped together under each emitter
	
void tlEmitter::SetGroupParticles( bool group )
{
	m_GroupParticles = group;
}
//------------------------------------------------------------------------------
// Get the current parent effect
// returns: tlEffect
	
tlEffect * tlEmitter::GetParentEffect()
{
	return m_ParentEffect;
}
//------------------------------------------------------------------------------
// Get the animation frame of the tAnimImage used by the emitter
	
int tlEmitter::GetFrame()
{
	return m_Frame;
}
//------------------------------------------------------------------------------
// Get the current angle offset used by %angletype (see #setangletype)
	
f32 tlEmitter::GetAngleOffset()
{
	return m_AngleOffset;
}
//------------------------------------------------------------------------------
// Get whether the particles spawned by this emitter scale uniformally
// returns: TRUE or FALSE
	
bool tlEmitter::GetUniform()
{
	return m_Uniform;
}
//------------------------------------------------------------------------------
// Get the current angletype for particles spawned by this emitter
// returns: either tlANGLE_ALIGN, tlANGLE_RANDOM or tlANGLE_SPECIFY
	
int tlEmitter::GetAngleType()
{
	return m_AngleType;
}
//------------------------------------------------------------------------------
// Get whether the emitter uses the effect emission instead of its own
// returns: either TRUE or FALSE
	
bool tlEmitter::GetUseEffectEmission()
{
	return m_UseEffectEmission;
}
//------------------------------------------------------------------------------
// Get the visibility status of the emitter
// returns: either TRUE or FALSE
	
bool tlEmitter::GetVisible()
{
	return m_Visible;
}
//------------------------------------------------------------------------------
// Find out if the emitter spawns a single particle
// returns: Either TRUE or FALSE

bool tlEmitter::GetSingleParticle()
{
	return m_SingleParticle;
}
//------------------------------------------------------------------------------
// Get whether the emitter chooses a random colour for the particles it spawns
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetRandomColor()
{
	return m_RandomColor;
}
//------------------------------------------------------------------------------
// Get the current z layer of the emitter
//	returns: Value from 0 - 8
	
int tlEmitter::GetZLayer()
{
	return m_ZLayer;
}
//------------------------------------------------------------------------------
// Get whether this emitter spawns particles that animate
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetAnimate()
{
	return m_Animate;
}
//------------------------------------------------------------------------------
// Get whether the emitter chooses a random start frame for the particles it spawns
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetRandomStartFrame()
{
	return m_RandomStartFrame;
}
//------------------------------------------------------------------------------
// Get the current animation direction
// returns: Either -1 for reverse playback or 1 for normal playback for particles spawned by this emitter.
	
f32 tlEmitter::GetAnimationDirection()
{
	return m_AnimationDirection;
}
//------------------------------------------------------------------------------
// Get the number of times the colour cycles over the lifetime of the particles spawned by this emitter.
	
int tlEmitter::GetColorRepeat()
{
	return m_ColorRepeat;
}
//------------------------------------------------------------------------------
// Get the number of times the alpha cycles over the lifetime of the particles spawned by this emitter.
	
int tlEmitter::GetAlphaRepeat()
{
	return m_AlphaRepeat;
}
//------------------------------------------------------------------------------
// Get whether this emitter spawns a one shot particle (see #setoneshot)
// returns: either TRUE or FALSE
	
bool tlEmitter::GetOneShot()
{
	return m_OneShot;
}
//------------------------------------------------------------------------------
// Get whether the handle of the particles spawned by this emitter are set to the center.
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetHandleCenter()
{
	return m_HandleCenter;
}
//------------------------------------------------------------------------------
// Get whether the particles spawned by this emitter remain relative to the containg effect
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetParticlesRelative()
{
	return m_ParticlesRelative;
}
//------------------------------------------------------------------------------
// Get whether particles spawned are having their angles locked to direction
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetLockAngle()
{
	return m_LockedAngle;
}
//------------------------------------------------------------------------------
// Get whether particles spawned will have there angle relative to the parent
// returns: Either TRUE or FALSE
	
bool tlEmitter::GetAngleRelative()
{
	return m_AngleRelative;
}
//------------------------------------------------------------------------------
// returns the current state of whether spawned particles playback the animation just once
	
bool tlEmitter::GetOnce()
{
	return m_Once;
}
//------------------------------------------------------------------------------
// returns the current state of whether spawned particles are added to the particle managers pool, or the emitters own pool. true means that
// 	they're added to the particle managers pool.
	
bool tlEmitter::GetGroupParticles()
{
	return m_GroupParticles;
}

//------------------------------------------------------------------------------
// Update the emitter
// This is an internal method called by the parent effect when updating each frame. This method will update its position and spawn new particles
// depending on whatever settings the emitter has by calling #updatespawns
	
void tlEmitter::Update()
{
	_MatrixRotationZ( &m_Matrix, m_Angle );
		
	if (m_Parent && m_Relative)
	{
		SetZoom( m_Parent->m_Zoom );

		_MatrixMultiply( &m_Matrix, &m_Matrix, &m_Parent->m_Matrix );
		//matrix = matrix.transform(parent.matrix)

		Vector2 rotvec;
		Vector2Rotate( &rotvec, (Vector2*)&m_Pos, &m_Parent->m_Matrix );
		//rotvec:tlVector2 = parent.matrix.transformvector(New tlVector2.Create(x, y))

		m_World.x = m_Parent->m_World.x + (rotvec.x * m_Zoom);
		m_World.y = m_Parent->m_World.y + (rotvec.y * m_Zoom);

		m_RelativeAngle = m_Parent->m_RelativeAngle + m_Angle;
	}
	else 
	{
		m_World.x = m_Pos.x;
		m_World.y = m_Pos.y;
	}
		
	m_Dying = m_ParentEffect->m_Dying;

	// update all children ( these are particles on this emitter )
	UpdateChildren( );
				
	if ((m_Dead == false) && (m_Dying == false))
	{
		if (m_Visible && GetParticleManager()->m_SpawningAllowed)
		{
			UpdateSpawns();
		}
	}
	else
	{
		if (m_ChildCount == 0)
		{
			m_Parent->RemoveChild(this);
			Destroy();
		}
		else
		{
			KillChildren();
		}
	}
}
				
//------------------------------------------------------------------------------
// Spawns a new lot of particles if necessary and assign all properties and attributes to the particle.
// This method is called by #update each frame.
	
void tlEmitter::UpdateSpawns( tlParticle * esingle )
{
	// spawn some particles
	f32 tx;
	f32 ty;
	int intCounter;
			
	f32 cellsizew;
	f32 cellsizeh;
	f32 th;
	f32 er;

	int gradientIndex = m_ParentEffect->m_GradientIndex;

	f32 dTime = TimeGetElapsedSeconds();
	
	tlParticleManager * pm = GetParticleManager();

	f32 qty = (c_Amount->Get(gradientIndex) + RANDFLOATMAX(c_AmountVariation->Get(gradientIndex))) * m_ParentEffect->m_CurrentAmount * dTime;

	//PRINTF("%d, %x, %f %f \n", gradientIndex, this, m_ParentEffect->m_Age, qty );

	if (m_SingleParticle == false)
	{
		m_Counter += qty;
	}


	if (m_Counter >= 1.0f || (m_SingleParticle && !m_StartedSpawning) )
	{
		if ( (m_StartedSpawning==false) && m_SingleParticle )
		{
			switch ( m_ParentEffect->m_Class )
			{
			case tlPOINT_EFFECT:
				m_Counter = 1.0f;
				break;
			case tlAREA_EFFECT:
				m_Counter = m_ParentEffect->m_MGX * m_ParentEffect->m_MGY * m_ParentEffect->m_CurrentAmount;
				break;
			case tlLINE_EFFECT:
			case tlELLIPSE_EFFECT:
				m_Counter = m_ParentEffect->m_MGX * m_ParentEffect->m_CurrentAmount;
				break;
			}
		}
		else if (m_SingleParticle && m_StartedSpawning)
		{
			m_Counter = 0.0f;
		}

		intCounter = m_Counter;


		// Preload Attributes----
		f32 currentLife = c_Life->Get(gradientIndex) * m_ParentEffect->m_CurrentLife;


		f32 current_Weight;
		f32 current_WeightVariation;
		if (m_Bypass_Weight == false)
		{
			current_Weight = c_BaseWeight->Get(gradientIndex);
			current_WeightVariation = c_WeightVariation->Get(gradientIndex);
		}

		f32 current_Speed;
		f32 current_SpeedVariation;
		if (m_Bypass_Speed == false)
		{
			current_Speed = c_BaseSpeed->Get( gradientIndex );
			current_SpeedVariation = c_VelVariation->Get( gradientIndex );
		}

		f32 current_Spin;
		f32 current_SpinVariation;
		if (m_Bypass_Spin == false)
		{
			current_Spin = c_BaseSpin->Get(gradientIndex);
			current_SpinVariation = c_SpinVariation->Get( gradientIndex );
		}

		f32 current_DirectionVariation = c_DirectionVariation->Get( gradientIndex );

		f32 current_EmissionAngle;
		if (m_UseEffectEmission)
		{
			er = m_ParentEffect->m_CurrentEmissionRange;
			current_EmissionAngle = m_ParentEffect->m_CurrentEmissionAngle;
		}
		else
		{
			er = c_EmissionRange->Get( gradientIndex );
			current_EmissionAngle = c_EmissionAngle->Get( gradientIndex );
		}

		f32 current_LifeVariation = c_LifeVariation->Get( gradientIndex );
		f32 current_SizeX = c_SizeX->Get( gradientIndex );
		f32 current_SizeY = c_SizeY->Get( gradientIndex );
		f32 current_SizeXVariation = c_SizeXVariation->Get( gradientIndex );
		f32 current_SizeYVariation = c_SizeYVariation->Get( gradientIndex );

		// ----------------------

		for (int i=0; i<intCounter; ++i)
		{
			m_StartedSpawning = true;

			tlParticle * p;

			if (esingle == NULL)
			{
				p = pm->GrabParticle(m_ParentEffect, m_GroupParticles, m_ZLayer);
			}
			else
			{
				p = esingle;
			}


			if ( p )
			{
				// -----Link to it's emitter and assign the control source (which is this emitter)----
				p->m_Emitter = this;
				p->m_Parent = this;
				AddChild( p );

				if (m_ParentEffect->m_TraverseEdge && m_ParentEffect->m_Class == tlLINE_EFFECT)
				{
					m_ParticlesRelative = true;
				}
				p->m_Relative = m_ParticlesRelative;

				switch ( m_ParentEffect->m_Class )
				{
				case tlPOINT_EFFECT:
					if (p->m_Relative)
					{
						p->m_Pos.x = -m_ParentEffect->m_Handle.x;
						p->m_Pos.y = -m_ParentEffect->m_Handle.y;
					}
					else
					{
						if (m_ParentEffect->m_HandleCenter || (m_ParentEffect->m_Handle.x + m_ParentEffect->m_Handle.y == 0.0f) )
						{
							p->m_Pos.x = m_World.x;
							p->m_Pos.y = m_World.y;
							p->m_World.x = p->m_Pos.x - (m_ParentEffect->m_Handle.x * m_Zoom);
							p->m_World.y = p->m_Pos.y - (m_ParentEffect->m_Handle.y * m_Zoom);
						}
						else
						{
							p->m_Pos.x = -m_ParentEffect->m_Handle.x;
							p->m_Pos.y = -m_ParentEffect->m_Handle.y;

							Vector2 rotvec;
							Vector2Rotate( &rotvec, (Vector2*) &p->m_Pos, &m_Matrix );
							// rotvec = parent.matrix.transformvector(New tlVector2.Create(p->x, p->y))

							p->m_Pos.x = m_World.x + rotvec.x;
							p->m_Pos.y = m_World.y + rotvec.y;
							p->m_World.x = p->m_Pos.x * m_Zoom;
							p->m_World.y = p->m_Pos.y * m_Zoom;
						}
					}
					break;
				case tlAREA_EFFECT:
					if (m_ParentEffect->m_EmitAtPoints)
					{
						if (m_ParentEffect->m_SpawnDirection == -1)
						{
							m_GX += m_ParentEffect->m_SpawnDirection;
							if (m_GX < 0.0f)
							{
								m_GX = m_ParentEffect->m_MGX - 1;
								m_GY += m_ParentEffect->m_SpawnDirection;
								if (m_GY < 0.0f)
								{
									m_GY = m_ParentEffect->m_MGY - 1;
								}
							}
						}
						if (m_ParentEffect->m_MGX > 1.0f)
						{
							p->m_Pos.x = (m_GX / (m_ParentEffect->m_MGX - 1) * m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
						}
						else
						{
							p->m_Pos.x = -m_ParentEffect->m_Handle.x;
						}

						if (m_ParentEffect->m_MGY > 1.0f)
						{
							p->m_Pos.y = (m_GY / (m_ParentEffect->m_MGY - 1) * m_ParentEffect->m_CurrentHeight) - m_ParentEffect->m_Handle.y;
						}
						else
						{
							p->m_Pos.y = -m_ParentEffect->m_Handle.y;
						}

						if (m_ParentEffect->m_SpawnDirection > 0.0f)
						{
							m_GX += m_ParentEffect->m_SpawnDirection;
							if (m_GX >= m_ParentEffect->m_MGX)
							{
								m_GX = 0.0f;
								m_GY += m_ParentEffect->m_SpawnDirection;
								if (m_GY >= m_ParentEffect->m_MGY)
								{
									m_GY = 0.0f;
								}
							}
						}
					}
					else
					{
						p->m_Pos.x = RANDFLOATMAX(m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
						p->m_Pos.y = RANDFLOATMAX(m_ParentEffect->m_CurrentHeight) - m_ParentEffect->m_Handle.y;
					}
					if (p->m_Relative == false)
					{
						Vector2 rotvec;
						Vector2Rotate( &rotvec, (Vector2*)&p->m_Pos, &m_Parent->m_Matrix );
						//rotvec = parent.matrix.transformvector(New tlVector2.Create(p->x, p->y))

						p->m_Pos.x = m_Parent->m_World.x + (rotvec.x * m_Zoom);
						p->m_Pos.y = m_Parent->m_World.y + (rotvec.y * m_Zoom);
					}
					break;
				case tlELLIPSE_EFFECT:
					if (m_ParentEffect->m_EmitAtPoints)
					{
						cellsizew = m_ParentEffect->m_CurrentWidth * 0.5f;
						cellsizeh = m_ParentEffect->m_CurrentHeight * 0.5f;
						
						if (m_ParentEffect->m_MGX == 0)
						{
							m_ParentEffect->m_MGX = 1;
						}
						tx = cellsizew;
						ty = cellsizeh;

						m_GX += m_ParentEffect->m_SpawnDirection;
						if (m_GX >= m_ParentEffect->m_MGX)
						{
							m_GX = 0.0f;
						}
						else if (m_GX < 0.0f)
						{
							m_GX = m_ParentEffect->m_MGX - 1;
						}
					
						th = m_GX * (m_ParentEffect->m_EllipseArc / m_ParentEffect->m_MGX) + m_ParentEffect->m_EllipseOffset;
						
						p->m_Pos.x = MathCos(th) * tx - m_ParentEffect->m_Handle.x + tx;
						p->m_Pos.y = MathSin(th) * ty - m_ParentEffect->m_Handle.y + ty;
					}
					else
					{
						tx = m_ParentEffect->m_CurrentWidth * 0.5f;
						ty = m_ParentEffect->m_CurrentHeight * 0.5f;
					
						th = RANDFLOATMAX(m_ParentEffect->m_EllipseArc) + m_ParentEffect->m_EllipseOffset;
						
						p->m_Pos.x = MathCos(th) * tx - m_ParentEffect->m_Handle.x + tx;
						p->m_Pos.y = MathSin(th) * ty - m_ParentEffect->m_Handle.y + ty;
					}
					if (p->m_Relative == false)
					{
						Vector2 rotvec;
						Vector2Rotate( &rotvec, (Vector2*) &p->m_Pos, &m_Parent->m_Matrix );

						p->m_Pos.x = m_Parent->m_World.x + (rotvec.x * m_Zoom);
						p->m_Pos.y = m_Parent->m_World.y + (rotvec.y * m_Zoom);
					}
					break;
				case tlLINE_EFFECT:
					if (m_ParentEffect->m_TraverseEdge == false)
					{
						if (m_ParentEffect->m_EmitAtPoints)
						{
							if (m_ParentEffect->m_SpawnDirection < 0.0f)
							{
								m_GX += m_ParentEffect->m_SpawnDirection;
								if (m_GX < 0.0f)
								{
									m_GX = m_ParentEffect->m_MGX - 1;
								}
							}
							if (m_ParentEffect->m_MGX > 1.0f)
							{
								p->m_Pos.x = (m_GX / (m_ParentEffect->m_MGX - 1) * m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
							}
							else
							{
								p->m_Pos.x = -m_ParentEffect->m_Handle.x;
							}

							p->m_Pos.y = -m_ParentEffect->m_Handle.y;
							if (m_ParentEffect->m_SpawnDirection > 0.0f)
							{
								m_GX += m_ParentEffect->m_SpawnDirection;
								if (m_GX >= m_ParentEffect->m_MGX)
								{
									m_GX = 0.0f;
								}
							}
						}
						else
						{
							p->m_Pos.x = RANDFLOATMAX(m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
							p->m_Pos.y = -m_ParentEffect->m_Handle.y;
						}
					}
					else
					{
						if (m_ParentEffect->m_DistanceSetByLife)
						{
							p->m_Pos.x = -m_ParentEffect->m_Handle.x;
							p->m_Pos.y = -m_ParentEffect->m_Handle.y;
						}
						else
						{
							if (m_ParentEffect->m_EmitAtPoints)
							{
								if (m_ParentEffect->m_SpawnDirection < 0.0f)
								{
									m_GX += m_ParentEffect->m_SpawnDirection;

									if (m_GX < 0.0f)
									{
										m_GX = m_ParentEffect->m_MGX - 1;
									}
								}
								if (m_ParentEffect->m_MGX > 1.0f)
								{
									p->m_Pos.x = (m_GX / (m_ParentEffect->m_MGX - 1) * m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
								}
								else
								{
									p->m_Pos.x = -m_ParentEffect->m_Handle.x;
								}

								p->m_Pos.y = -m_ParentEffect->m_Handle.y;
								if (m_ParentEffect->m_SpawnDirection > 0.0f)
								{
									m_GX += m_ParentEffect->m_SpawnDirection;
									if (m_GX >= m_ParentEffect->m_MGX)
									{
										m_GX = 0.0f;
									}
								}
							}
							else
							{
								p->m_Pos.x = RANDFLOATMAX(m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
								p->m_Pos.y = -m_ParentEffect->m_Handle.y;
							}
						}
					}
					// rotate
					if (p->m_Relative == false)
					{
						Vector2 rotvec;
						Vector2Rotate( &rotvec, (Vector2*)&p->m_Pos, &m_Parent->m_Matrix );
						//rotvec = parent.matrix.transformvector(New tlVector2.Create(p->x, p->y))

						p->m_Pos.x = m_Parent->m_World.x + (rotvec.x * m_Zoom);
						p->m_Pos.y = m_Parent->m_World.y + (rotvec.y * m_Zoom);
					}
					break;
				}

				// -------------------------------
				// -----blend mode-----------------
				p->m_BlendMode = m_BlendMode;

				// -----Animation and framerate----
				p->m_Animating = m_Animate;
				p->m_AnimateOnce = m_Once;
				p->m_FrameRate = c_FrameRate->Get(0);

				if (m_RandomStartFrame)
				{
					p->SetCurrentFrame( RANDFLOATMAX(m_FrameCount-1) );
				}
				else
				{
					p->SetCurrentFrame( m_Frame );
				}
				// -------------------------------
				// Set the zoom level
				p->SetZoom( m_Zoom );

				// -----Set up the image----------
				p->m_Handle.x = m_Handle.x;
				p->m_Handle.y = m_Handle.y;
				p->m_HandleCenter = m_HandleCenter;

				// -------------------------------
				// -----Set lifetime properties---
				p->m_LifeTime = currentLife + RANDFLOATRANGE(-current_LifeVariation, current_LifeVariation) * m_ParentEffect->m_CurrentLife;

				// -------------------------------
				// -----Speed---------------------
				p->m_SpeedVec.x = 0.0f;
				p->m_SpeedVec.y = 0.0f;

				if (m_Bypass_Speed == false)
				{
					f32 velVariation = RANDFLOATRANGE(-current_SpeedVariation, current_SpeedVariation);
					p->m_BaseSpeed = (current_Speed + velVariation) * m_ParentEffect->m_CurrentVelocity;
					p->m_Speed = c_Velocity->Get(0) * p->m_BaseSpeed * c_GlobalVelocity->Get(0);
				}
				else
				{
					p->m_Speed = 0.0f;
				}
				// --------------------------------
				// -----Size----------------------
				p->m_GSizeX = m_ParentEffect->m_CurrentSizeX;
				p->m_GSizeY = m_ParentEffect->m_CurrentSizeY;

				if ( m_Uniform )
				{
					p->m_Width = current_SizeX + RANDFLOATMAX(current_SizeXVariation);
					p->m_Scale.x = (p->m_Width /  m_BaseWidth) * c_ScaleX->Get(0) * p->m_GSizeX;
					p->m_Scale.y = p->m_Scale.x;
					if ( (m_Bypass_Stretch == false) && p->m_Speed)
					{
						p->m_Scale.y = (c_ScaleX->Get(0) * p->m_GSizeX * (p->m_Width + (MathABS(p->m_Speed) * c_Stretch->Get(0) * m_ParentEffect->m_CurrentStretch))) / m_BaseWidth;
						if (p->m_Scale.y < p->m_Scale.x)
						{
							p->m_Scale.y = p->m_Scale.x;
						}
					}
				}
				else
				{
					// width
					p->m_Width = current_SizeX + RANDFLOATMAX(current_SizeXVariation);
					p->m_Scale.x = (p->m_Width / m_BaseWidth) * c_ScaleX->Get(0) * p->m_GSizeX;

					// height
					p->m_Height = current_SizeY + RANDFLOATMAX(current_SizeYVariation);
					p->m_Scale.y = (p->m_Height / m_BaseHeight) * c_ScaleY->Get(0) * p->m_GSizeY;

					if ( (m_Bypass_Stretch == false) && (p->m_Speed != 0.0f) )
					{
						p->m_Scale.y = (c_ScaleY->Get(0) * p->m_GSizeY * (p->m_Height + (MathABS(p->m_Speed) * c_Stretch->Get(0) * m_ParentEffect->m_CurrentStretch))) / m_BaseHeight;
						if (p->m_Scale.y < p->m_Scale.x)
						{
							p->m_Scale.y = p->m_Scale.x;
						}
					}
				}
				// -------------------------------
				// -----Splatter-------------------
				if (m_Bypass_Splatter == false)
				{
					Vector2 splat;

					f32 splattertemp = c_Splatter->Get(gradientIndex);

					splat.x = RANDFLOATRANGE(-splattertemp, splattertemp);
					splat.y = RANDFLOATRANGE(-splattertemp, splattertemp);

					while ( (Vector2Magnitude(&splat) >= splattertemp) && (splattertemp > 0.0f) )
					{
						splat.x = RANDFLOATRANGE(-splattertemp, splattertemp);
						splat.y = RANDFLOATRANGE(-splattertemp, splattertemp);
					}

					if (p->m_Relative)
					{
						p->m_Pos.x += splat.x;
						p->m_Pos.y += splat.y;
					}
					else
					{
						p->m_Pos.x += splat.x * m_Zoom;
						p->m_Pos.y += splat.y * m_Zoom;
					}
				}
				// --------------------------------
				// rotation and direction of travel settings-----

				p->MiniUpdate();

				if ( (m_ParentEffect->m_TraverseEdge == true) && (m_ParentEffect->m_Class == tlLINE_EFFECT) )
				{
					p->m_DirectionLocked = true;
					p->m_Direction = 90.0f;
				}
				else
				{
					if (m_ParentEffect->m_Class != tlPOINT_EFFECT)
					{
						if (m_Bypass_Speed==false || m_AngleType == tlANGLE_ALIGN)
						{
							switch (m_ParentEffect->m_EmissionType)
							{
							case tlEMISSION_INWARDS:
								p->m_EmissionAngle = current_EmissionAngle + RANDFLOATRANGE(-er, er);
								if (p->m_Relative)
									p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_Pos, &VECTOR2_ZERO );
								else
									p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_World, (Vector2*)&p->m_Parent->m_World );
									break;

							case tlEMISSION_OUTWARDS:
								p->m_EmissionAngle = current_EmissionAngle + RANDFLOATRANGE(-er, er);
								if (p->m_Relative)
									p->m_EmissionAngle += Vector2Direction( &VECTOR2_ZERO, (Vector2*)&p->m_Pos );
								else
									p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_Parent->m_World, (Vector2*)&p->m_World );
								break;

							case tlEMISSION_IN_AND_OUT:
								p->m_EmissionAngle = current_EmissionAngle + RANDFLOATRANGE(-er, er);

								if (m_DirAlternater)
								{
									if (p->m_Relative)
										p->m_EmissionAngle += Vector2Direction( &VECTOR2_ZERO, (Vector2*)&p->m_Pos );
									else
										p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_Parent->m_World, (Vector2*)&p->m_World );
								}
								else
								{
									if (p->m_Relative)
										p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_Pos, &VECTOR2_ZERO );
									else
										p->m_EmissionAngle += Vector2Direction( (Vector2*)&p->m_World, (Vector2*)&p->m_Parent->m_World );
								}
								m_DirAlternater = !m_DirAlternater;
								break;
							case tlEMISSION_SPECIFIED:
								p->m_EmissionAngle = current_EmissionAngle + RANDFLOATRANGE(-er, er);
								break;
							}
						}
					}
					else
					{
						p->m_EmissionAngle = current_EmissionAngle + RANDFLOATRANGE(-er, er);
					}
					if (m_Bypass_DirectionVariation == false)
					{
						p->m_DirectionVariation = current_DirectionVariation;
						f32 dv = p->m_DirectionVariation * c_DirectionVariationOT->Get(0);
						p->m_Direction = p->m_EmissionAngle + c_Direction->Get(0) + RANDFLOATRANGE(-dv, dv);
					}
					else
					{
						p->m_Direction = p->m_EmissionAngle + c_Direction->Get(0);
					}
				}
				// -------------------------------
				// p->lockedangle = lockedangle----
				if (m_Bypass_Spin == false)
				{
					p->m_SpinVariation = RANDFLOATRANGE(-current_SpinVariation, current_SpinVariation) + current_Spin;
				}
				// -----Weight---------------------
				if (m_Bypass_Weight == false)
				{
					p->m_Weight = c_Weight->Get(0);
					p->m_WeightVariation = RANDFLOATRANGE(-current_WeightVariation, current_WeightVariation);
					p->m_BaseWeight = (current_Weight + p->m_WeightVariation) * m_ParentEffect->m_CurrentWeight;
				}
				// --------------------------------
				if (m_LockedAngle)
				{
					if (!m_Bypass_Weight && !m_Bypass_Speed && !m_ParentEffect->m_Bypass_Weight)
					{
						p->m_SpeedVec.x = MathCos(p->m_Direction);
						p->m_SpeedVec.y = MathSin(p->m_Direction);
						p->m_Angle = p->m_Direction;
						//p->m_Angle = Vector2Direction( &VECTOR2_ZERO, &p->m_SpeedVec ); //prwprw
					}
					else
					{
						if (m_ParentEffect->m_TraverseEdge)
						{
							p->m_Angle = m_ParentEffect->m_Angle + m_AngleOffset;
						}
						else
						{
							p->m_Angle = p->m_Direction + m_Angle + m_AngleOffset;
						}
					}
				}
				else
				{
					switch (m_AngleType)
					{
					case tlANGLE_ALIGN:
						if (m_ParentEffect->m_TraverseEdge)
							p->m_Angle = m_ParentEffect->m_Angle + m_AngleOffset;
						else
							p->m_Angle = p->m_Direction + m_AngleOffset;
						break;

					case tlANGLE_RANDOM:
						p->m_Angle = RANDFLOATMAX(m_AngleOffset);
						break;

					case tlANGLE_SPECIFY:
						p->m_Angle = m_AngleOffset;
						break;
					}
				}
				// -------------------------------
				// -----Colour Settings-----------
				if ( m_RandomColor )
				{
					int rand = RandIntMax(c_R->GetLastIndex());
					p->m_Color.r = c_R->Get(rand);
					p->m_Color.g = c_G->Get(rand);
					p->m_Color.b = c_B->Get(rand);
				}
				else
				{
					p->m_Color.r = c_R->Get(0);
					p->m_Color.g = c_G->Get(0);
					p->m_Color.b = c_B->Get(0);
				}
				p->m_Color.a = p->m_Emitter->c_Alpha->Get(0) * m_ParentEffect->m_CurrentAlpha;


				// ---------------------------------
				// -----add any sub children--------

				tlEffect * eff = m_Effects;
				while (eff)
				{
					tlEffect * neweff = pm->CopyEffect( eff );

					neweff->m_Parent = p;
					neweff->m_ParentEmitter = this;
					p->AddChild( neweff );

					eff = eff->m_Next;
				}


				// get the relativeangle
				if (p->m_Relative == false)
				{
					_MatrixRotationZ( &p->m_Matrix, p->m_Angle );
					//p->matrix.set(Cos(angle), Sin(p->m_Angle), -Sin(p->m_Angle), Cos(p->m_Angle))

					_MatrixMultiply( &p->m_Matrix, &p->m_Matrix, &m_Parent->m_Matrix );
					//p->matrix = p->matrix.transform(parent.matrix)
				}
				p->m_RelativeAngle = m_Parent->m_RelativeAngle + p->m_Angle;
			}
		}
		m_Counter -= intCounter;
	}
}
//------------------------------------------------------------------------------
// Control a particle
// Any particle spawned by an emitter is controlled by it. When a particle is updated it calls this method to find out how it should behave.
	
void tlEmitter::ControlParticle( tlParticle * p )
{
	f32 dTime = TimeGetElapsedSeconds( );

	// handle particle animation
	if ( m_Animating )
	{
		//dTime = 1.0f / 60.0f;
		p->m_CurrentFrame += p->m_FrameRate * dTime;

		if ( p->m_AnimateOnce )
		{
			if ( p->m_CurrentFrame >= m_FrameCount )
			{
				p->m_CurrentFrame = m_FrameCount-1;
			}
			else if (p->m_CurrentFrame < 0.0f)
			{
				p->m_CurrentFrame = 0.0f;
			}
		}
		else
		{
			if ( p->m_CurrentFrame >= m_FrameCount )
				p->m_CurrentFrame = 0.0f;
			else if ( p->m_CurrentFrame < 0.0f )
				p->m_CurrentFrame = m_FrameCount-1;
		}

		p->SetCurrentFrame( p->m_CurrentFrame );
	}

	// determine lookup index for all lookup tables.  This is the same for all tables, so lets compute it once, and use to everywhere.
	int lookupIndex = (int) (p->m_Age * m_GradientSize / p->m_LifeTime);

	// -----Alpha Change-----
	if (m_AlphaRepeat > 1)
	{
		p->m_RepeatAgeAlpha += dTime * m_AlphaRepeat;

		int index = (int) (p->m_RepeatAgeAlpha * GetParticleManager()->m_LookupFreq);

		p->m_Color.a = c_Alpha->Get(index) * m_ParentEffect->m_CurrentAlpha;
		if ( (p->m_RepeatAgeAlpha > p->m_LifeTime) && (p->m_AlphaCycles < m_AlphaRepeat) )
		{
			p->m_RepeatAgeAlpha -= p->m_LifeTime;
			p->m_AlphaCycles++;
		}
	}
	else
	{
		p->m_Color.a = c_Alpha->Get(lookupIndex) * m_ParentEffect->m_CurrentAlpha;
	}
	// ----------------------
	// -----Angle Changes----
	if ( (m_LockedAngle == true) && (m_AngleType == tlANGLE_ALIGN) )
	{
		if (p->m_DirectionLocked)
		{
			p->m_Angle = m_ParentEffect->m_Angle + m_Angle + m_AngleOffset;
		}
		else
		{
			//if ( (m_Bypass_Weight == false) && (m_ParentEffect->m_Bypass_Weight == false) || p->m_Direction)
			if ( (m_Bypass_Weight == false) && (m_ParentEffect->m_Bypass_Weight == false) )
			{
				// prwprw
				if (p->m_Relative)
				{
					p->m_Angle = p->m_Direction;//Vector2Direction(p->m_oldx, p->m_oldy, p->m_x, p->m_y);
				}
				else
				{
					p->m_Angle = p->m_Direction;//Vector2Direction(p->m_oldwx, p->m_oldwy, p->m_wx, p->m_wy);
				}
			}
			else
			{
				p->m_Angle = p->m_Direction + m_Angle + m_AngleOffset;
			}
		}
	}
	else
	{
		if (m_Bypass_Spin == false)
		{
			p->m_Angle += c_Spin->Get(lookupIndex) * p->m_SpinVariation * m_ParentEffect->m_CurrentSpin * dTime;
		}
	}
	// ----------------------
	// ----Direction Changes and Motion Randomness--
	if (p->m_DirectionLocked)
	{
		p->m_Direction = 90.0f;

		switch ( m_ParentEffect->m_Class )
		{
		case tlLINE_EFFECT:
			if (m_ParentEffect->m_DistanceSetByLife)
			{
				f32 life = p->m_Age / p->m_LifeTime;
				p->m_Pos.x = (life * m_ParentEffect->m_CurrentWidth) - m_ParentEffect->m_Handle.x;
			}
			else
			{
				switch (m_ParentEffect->m_EndBehaviour)
				{
				case tlEND_KILL:
					if ( (p->m_Pos.x > m_ParentEffect->m_CurrentWidth - m_ParentEffect->m_Handle.x) || (p->m_Pos.x < -m_ParentEffect->m_Handle.x ) )
					{
						p->m_Dead = 2;
					}
					break;
				case tlEND_LOOPAROUND:
					if (p->m_Pos.x > m_ParentEffect->m_CurrentWidth - m_ParentEffect->m_Handle.x)
					{
						p->m_Pos.x = -m_ParentEffect->m_Handle.x;
						p->MiniUpdate();
					}
					else
					{
						if (p->m_Pos.x < -m_ParentEffect->m_Handle.x)
						{
							p->m_Pos.x = m_ParentEffect->m_CurrentWidth - m_ParentEffect->m_Handle.x;
							p->MiniUpdate();
						}
					}
					break;
				}
			}
		}
	}
	else 
	{
		if (m_Bypass_DirectionVariation == false)
		{
			f32 dv = p->m_DirectionVariation * c_DirectionVariationOT->Get(lookupIndex);

			p->m_TimeTracker += dTime;

			if (p->m_TimeTracker > tlMOTION_VARIATION_INTERVAL)
			{
				p->m_RandomDirection += tlMAX_DIRECTION_VARIATION * RANDFLOATRANGE(-dv, dv);
				p->m_RandomSpeed += tlMAX_VELOCITY_VARIATION * RANDFLOATRANGE(-dv, dv);
				p->m_TimeTracker = 0.0f;
			}
		}
		p->m_Direction = p->m_EmissionAngle + c_Direction->Get(lookupIndex) + p->m_RandomDirection;
	}
	// -----------------------
	// ------Size Changes-----
	if ( m_Uniform )
	{
		if (m_Bypass_ScaleX == false)
		{
			p->m_Scale.x = (c_ScaleX->Get(lookupIndex) * p->m_GSizeX * p->m_Width) / m_BaseWidth;
			p->m_Scale.y = p->m_Scale.x;
		}
	}
	else
	{
		if (m_Bypass_ScaleX == false)
		{
			p->m_Scale.x = (c_ScaleX->Get(lookupIndex) * p->m_GSizeX * p->m_Width) / m_BaseWidth;
		}
		if (m_Bypass_ScaleY == false)
		{
			p->m_Scale.y = (c_ScaleY->Get(lookupIndex) * p->m_GSizeY * p->m_Height) / m_BaseHeight;
		}
	}
	// -----------------------
	// -----Colour Changes----
	if (m_Bypass_Colour == false)
	{
		if (m_RandomColor == false)
		{
			if (m_ColorRepeat > 1)
			{
				p->m_RepeatAgeColor += dTime * m_ColorRepeat;

				int index = (int) (p->m_RepeatAgeColor * GetParticleManager()->m_LookupFreq);
				p->m_Color.r = c_R->Get(index);
				p->m_Color.g = c_G->Get(index);
				p->m_Color.b = c_B->Get(index);

				if ( (p->m_RepeatAgeColor > p->m_LifeTime) && (p->m_ColorCycles < m_ColorRepeat) )
				{
					p->m_RepeatAgeColor -= p->m_LifeTime;
					p->m_ColorCycles++;
				}
			}
			else
			{
				p->m_Color.r = c_R->Get(lookupIndex);
				p->m_Color.g = c_G->Get(lookupIndex);
				p->m_Color.b = c_B->Get(lookupIndex);
			}
		}
	}
	// -----------------------
	// -------Animation-------
	if (m_Bypass_FrameRate == false)
	{
		p->m_FrameRate = c_FrameRate->Get(lookupIndex) * m_AnimationDirection;
	}
	// -----------------------
	// -----Speed Changes-----
	if (m_Bypass_Speed == false)
	{
		p->m_Speed = c_Velocity->Get(lookupIndex) * p->m_BaseSpeed * c_GlobalVelocity->Get(m_ParentEffect->m_GradientIndex);
		p->m_Speed += p->m_RandomSpeed;
	}
	else
	{
		p->m_Speed = p->m_RandomSpeed;
	}
	// -------Stretch---------
	if (m_Bypass_Stretch == false)
	{
		if ( (m_Bypass_Weight==false) && (m_ParentEffect->m_Bypass_Weight==false) )
		{
			if (p->m_Speed)
			{
				p->m_SpeedVec.x = (p->m_SpeedVec.x * dTime);
				p->m_SpeedVec.y = (p->m_SpeedVec.y * dTime) - p->m_Gravity;
			}
			else
			{
				p->m_SpeedVec.x = 0.0f;
				p->m_SpeedVec.y = -p->m_Gravity;
			}
			if (m_Uniform)
			{
				p->m_Scale.y = (c_ScaleX->Get(lookupIndex) * p->m_GSizeX * (p->m_Width + (Vector2Magnitude(&p->m_SpeedVec) * c_Stretch->Get(lookupIndex) * m_ParentEffect->m_CurrentStretch))) / m_BaseWidth;
			}
			else
			{
				p->m_Scale.y = (c_ScaleY->Get(lookupIndex) * p->m_GSizeY * (p->m_Height + (Vector2Magnitude(&p->m_SpeedVec) * c_Stretch->Get(lookupIndex) * m_ParentEffect->m_CurrentStretch))) / m_BaseHeight;
			}
		}
		else
		{
			if (m_Uniform)
			{
				p->m_Scale.y = (c_ScaleX->Get(lookupIndex) * p->m_GSizeX * (p->m_Width + (MathABS(p->m_Speed) * c_Stretch->Get(lookupIndex) * m_ParentEffect->m_CurrentStretch))) / m_BaseWidth;
			}
			else
			{
				p->m_Scale.y = (c_ScaleY->Get(lookupIndex) * p->m_GSizeY * (p->m_Height + (MathABS(p->m_Speed) * c_Stretch->Get(lookupIndex) * m_ParentEffect->m_CurrentStretch))) / m_BaseHeight;
			}
		}

		if (p->m_Scale.y < p->m_Scale.x)
		{
			p->m_Scale.y = p->m_Scale.x;
		}
	}
	// ------------------------
	// -----Weight Changes-----
	if (m_Bypass_Weight == false)
	{
		p->m_Weight = c_Weight->Get(lookupIndex) * p->m_BaseWeight;
	}
}

//------------------------------------------------------------------------------
// Cycle forward through the image frames
	
void tlEmitter::NextFrame()
{
	if (++m_Frame >= m_FrameCount)
	{
		m_Frame = 0;
	}
}
//------------------------------------------------------------------------------
// Cycle backwards throught the image frames.
	
void tlEmitter::PreviousFrame()
{
	if (--m_Frame < 0)
		m_Frame = m_FrameCount - 1;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Compilers
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void tlEmitter::Compile_All(  )
{
	f32 longestLife = GetLongestLife();
	f32 freq = GetParticleManager()->m_LookupFreq;
	int size = (int) (longestLife * freq);

	m_GradientSize = size;

	//PRINTF("BUILDING emitter lookup size\n" );

	c_R->BuildLookup( freq, size );	// time is normalized 0-1 here, so adjust lookupsize based on longestLife of this particle ( because 1 = end of life )
	c_G->BuildLookup( freq, size );
	c_B->BuildLookup( freq, size );
	c_Alpha->BuildLookup( freq, size );

	c_BaseSpin->BuildLookup( freq );
	c_Spin->BuildLookup( freq, size );
	c_SpinVariation->BuildLookup( freq );

	c_Velocity->BuildLookup( freq, size );
	c_VelVariation->BuildLookup( freq );
	c_GlobalVelocity->BuildLookup( freq );

	c_BaseWeight->BuildLookup( freq );
	c_Weight->BuildLookup( freq, size );
	c_WeightVariation->BuildLookup( freq );
	c_BaseSpeed->BuildLookup( freq );

	c_SizeX->BuildLookup( freq );
	c_SizeY->BuildLookup( freq );

	c_SizeXVariation->BuildLookup( freq );
	c_SizeYVariation->BuildLookup( freq );

	c_ScaleX->BuildLookup( freq, size );
	c_ScaleY->BuildLookup( freq, size );

	c_Life->BuildLookup( freq );
	c_LifeVariation->BuildLookup( freq );

	c_Amount->BuildLookup( freq );
	c_AmountVariation->BuildLookup( freq );

	c_EmissionAngle->BuildLookup( freq );
	c_EmissionRange->BuildLookup( freq );

	c_Direction->BuildLookup( freq );
	c_DirectionVariation->BuildLookup( freq );
	c_DirectionVariationOT->BuildLookup( freq, size );

	c_FrameRate->BuildLookup( freq, size );

	c_Stretch->BuildLookup( freq, size );
	c_Splatter->BuildLookup( freq );

	tlEffect * e = m_Effects;
	while (e)
	{
		e->Compile_All( );
		e = e->m_Next;
	}

	Analyse_Emitter();
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tlEmitter::Analyse_Emitter()
{
	ResetBypassers();

	return;

	if (c_LifeVariation->GetLastIndex() == 0)
	{
		if (c_LifeVariation->Get(0) == 0.0f)
		{
			m_Bypass_LifeVariaton = true;
		}
	}
	if (c_Stretch->Get(0) == 0.0f)
	{
		m_Bypass_Stretch = true;
	}

	if (c_FrameRate->GetLastIndex() == 0)
	{
		if (c_FrameRate->Get(0) == 0.0f)
		{
			m_Bypass_FrameRate = true;
		}
	}
	if ( (c_Splatter->GetLastIndex() == 0) && (c_Splatter->Get(0) == 0.0f) )
	{
		m_Bypass_Splatter = true;
	}

	if ( (c_BaseWeight->GetLastIndex() == 0) && (c_WeightVariation->GetLastIndex() == 0) )
	{
		if ( (c_BaseWeight->Get(0) == 0.0f) && ( c_WeightVariation->Get(0) == 0.0f) )
		{
			m_Bypass_Weight = true;
		}
	}

	if ( (c_Weight->GetLastIndex()==0) && (c_Weight->Get(0) == 0.0f) )
	{
		m_Bypass_Weight = true;
	}
	if ( (c_BaseSpeed->GetLastIndex() == 0) && (c_VelVariation->GetLastIndex() == 0) )
	{
		if ( (c_BaseSpeed->Get(0) == 0.0f) && (c_VelVariation->Get(0) == 0.0f) )
		{
			m_Bypass_Speed = true;
		}
	}
	if ( (c_BaseSpin->GetLastIndex() == 0) && (c_SpinVariation->GetLastIndex() == 0) )
	{
		if ( (c_BaseSpin->Get(0) == 0.0f) && (c_SpinVariation->Get(0) == 0.0f) )
		{
			m_Bypass_Spin = true;
		}
	}
	if (c_DirectionVariation->GetLastIndex() == 0)
	{
		if (c_DirectionVariation->Get(0) == 0.0f)
		{
			m_Bypass_DirectionVariation = true;
		}
	}
	if (c_R->GetLastIndex() == 0)
	{
		m_Bypass_Colour = true;
	}
	if (c_ScaleX->GetLastIndex() == 0)
	{
		m_Bypass_ScaleX = true;
	}
	if (c_ScaleY->GetLastIndex() == 0)
	{
		m_Bypass_ScaleY = true;
	}
}
//------------------------------------------------------------------------------

void tlEmitter::ResetBypassers()
{
	m_Bypass_Weight = false;
	m_Bypass_Speed = false;
	m_Bypass_Spin = false;
	m_Bypass_DirectionVariation = false;
	m_Bypass_Colour = false;
	m_Bypass_ScaleX = false;
	m_Bypass_ScaleY = false;
	m_Bypass_LifeVariaton = false;
	m_Bypass_FrameRate = false;
	m_Bypass_Stretch = false;
	m_Bypass_Splatter = false;
}
//------------------------------------------------------------------------------
// Get the longest life from this emitter.

f32 tlEmitter::GetLongestLife()
{
	f32 longestLife = ( c_LifeVariation->GetMaxValue() + c_Life->GetMaxValue() ) * m_ParentEffect->c_Life->GetMaxValue();
	return longestLife;
}
