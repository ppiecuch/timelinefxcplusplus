// 
//	File	   	-> tlParticleManager.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlParticleManager.h"

#include "tlEntity.h"
#include "tlEffect.h"
#include "tlEmitter.h"
#include "tlParticle.h"
#include "tlLibrary.h"

#include "ezxml.h"

//------------------------------------------------------------------------------
// Create a new Particle Manager
// Creates a new particle manager and sets the maximum number of particles.

tlParticleManager * tlParticleManager::Create( const char * atlasName, int maxBlend, int maxAdd, u32 drawMask ) // static
{
	tlParticleManager * pm = new tlParticleManager( );

	pm->m_DrawMask = drawMask;

	pm->m_UnUsedCount = maxBlend + maxAdd;
	pm->m_InUseCount = 0;

	pm->m_FontAtlas[0] = pm->CreateFont( FileSetExt( atlasName, ".fnt" ), maxBlend );
	pm->m_FontAtlas[1] = pm->CreateFont( FileSetExt( atlasName, ".fnt" ), maxAdd );
	pm->m_FontAtlas[1]->SetMaterial( Material::Get( FileSetExt( atlasName, ".add.material" ) ) );

	// compute RETINA scale factor...
	Material * mat = pm->m_FontAtlas[0]->GetMaterial();
	pm->m_Scale = TaskEngine::m_Width / mat->m_ResX;
	PRINTF( "PARTICLE RETINA SCALE %f\n", pm->m_Scale );

	pm->LoadEffects( FileSetExt( atlasName, ".effect" ) );

	// Allocate a pool of particles
	pm->m_ParticleArray = new tlParticle[ pm->m_UnUsedCount ];
	pm->m_UnUsed = pm->m_ParticleArray;

	int i;
	for (i=0; i<pm->m_UnUsedCount-1; ++i)
	{
		pm->m_ParticleArray[ i ].m_Next = &pm->m_ParticleArray[ i + 1 ];
	}
	pm->m_ParticleArray[ i ].m_Next = NULL;

	return pm;
}

//------------------------------------------------------------------------------

tlParticleManager::tlParticleManager( )
{
	m_LookupFreq = 60.0f;

	m_DrawMask = NODE_DRAWMASK_PARTICLE;

	m_IdleTimeLimit = 60;	//1second

	m_SpawningAllowed = true;

	m_UnUsedCount = 0;
	m_InUseCount = 0;

	m_Lib = new tlLibrary();

	m_Effects = NULL;

	m_ParticleArray = NULL;
	m_UnUsed = NULL;
	for (int i=0; i<9; ++i)
	{
		m_InUse[i] = NULL;
	}
	// register events so particleManager is updated and draws
	g_EventManager->AddListener( EVENT_UPDATE, LISTEN_METHOD( this, tlParticleManager::Update ), 100 );
	g_EventManager->AddListener( EVENT_CAMERA_PRERENDER, LISTEN_METHOD( this, tlParticleManager::DrawParticles ), 100 );
}

//------------------------------------------------------------------------------

tlParticleManager::~tlParticleManager()
{
	delete [] m_ParticleArray;

	g_EventManager->RemoveListener( EVENT_UPDATE, LISTEN_METHOD( this, tlParticleManager::Update ) );
	g_EventManager->RemoveListener( EVENT_CAMERA_PRERENDER, LISTEN_METHOD( this, tlParticleManager::DrawParticles ) );
}

//------------------------------------------------------------------------------
// Update the Particle Manager
// Run this method in your main loop to update all particle effects.

void tlParticleManager::Update( void * eventData )
{
	TIMER_START( "ParticleUpdate", "" );

	tlEffect * e = m_Effects;
	tlEffect * prev = NULL;

	while ( e )
	{
		tlEffect * next = e->m_Next;

		e->Update();

		// if effect was tagged as destroyed, then remove it from the m_Effect list here, and destroy it
		if ( e->m_Destroyed == true )
		{
			// remove the effect
			if ( prev )
				prev->m_Next = next;
			else
				m_Effects = next;

			// we can now destroy this effect ( its been removed from the m_Effects list )
			e->Destroy( );

			e = next;
			continue;
		}
		prev = e;
		e = next;
	}

	TIMER_END();
}

//------------------------------------------------------------------------------
// grab a free particle from the 'unused' list.

tlParticle * tlParticleManager::GrabParticle( tlEffect * effect, bool groupIt, int layer )
{
	// any unused particles available ?
	tlParticle * p = m_UnUsed;

	if ( p )
	{
		// remove from the unused list
		m_UnUsed = p->m_Next;
		m_UnUsedCount--;
		m_InUseCount++;

		p->m_PM = this;
		p->m_Age = 0.0f;

		// add to the used list
		p->m_Layer = layer;
		p->m_GroupParticles = groupIt;

		if ( groupIt )
		{
			// add to double linked list, in the effects 'inUse' list
			p->m_Prev = NULL;
			p->m_Next = effect->m_InUse[layer];
			if ( effect->m_InUse[layer] ) effect->m_InUse[layer]->m_Prev = p;
			effect->m_InUse[layer] = p;
		}
		else
		{
			// add to double linked list
			p->m_Prev = NULL;
			p->m_Next = m_InUse[layer];
			if ( m_InUse[layer] ) m_InUse[layer]->m_Prev = p;
			m_InUse[layer] = p;
		}
	}
	return p;
}

//------------------------------------------------------------------------------

void tlParticleManager::ReleaseParticle( tlParticle * p )
{
	m_UnUsedCount++;
	m_InUseCount--;

	if ( p->m_GroupParticles == false )
	{
		// unlink this child from the 'used' list
		if ( p->m_Prev )
			p->m_Prev->m_Next = p->m_Next;
		else
			m_InUse[p->m_Layer] = p->m_Next;

		if ( p->m_Next )
			p->m_Next->m_Prev = p->m_Prev;

		p->m_Prev = NULL;
	}
	else
	{
		if ( p->m_Prev )
			p->m_Prev->m_Next = p->m_Next;
		else
			p->m_Emitter->m_ParentEffect->m_InUse[ p->m_Layer ] = p->m_Next;

		if ( p->m_Next )
			p->m_Next->m_Prev = p->m_Prev;

		p->m_Prev = NULL;
	}

	// add particle bad to the 'unused' list ( this is only maintained as a single linked list )
	p->m_Next = m_UnUsed;
	m_UnUsed = p;
}	
//------------------------------------------------------------------------------
// Draw all particles currently in use
// Draws all pariticles in use and uses the tween value you pass to use render tween in order to smooth out the movement of effects assuming you
// use some kind of tweening code in your app.
	
void tlParticleManager::DrawParticles( void * eventData )
{
	NodeCamera * camera = (NodeCamera*) eventData;

	// is this camera supposed to draw these particles ? Check their drawmasks
	if ( camera->m_DrawMask & m_DrawMask )
	{
		DrawList * drawlist = camera->GetDrawList();

		// first, draw all particles not grouped with effects.
		for (int i=0; i<9; ++i)
		{
			tlParticle * p = m_InUse[i];
			while ( p )
			{
				DrawParticle( p );
				p = p->m_Next;
			}
		}
		// next, draw all effects ( and the particles grouped in them )
		DrawEffects( );
	}
}	

//------------------------------------------------------------------------------
// Set the angle of the particle manager
// Setting the angle of the particle manager will rotate all of the particles around the origin
	
void tlParticleManager::SetAngle( f32 angle )
{
	m_Angle = angle;
}
	
//------------------------------------------------------------------------------
// Set the amount of time before idle effects are deleted from the particle manager
// Any effects that have no active particles being drawn on the screen will be automatically removed from the particle manager after a given time set by this function.

void tlParticleManager::SetIdleTimeLimit( int ticks )
{
	m_IdleTimeLimit = ticks;
}
	
//------------------------------------------------------------------------------
// Get the current number of particles in use

int tlParticleManager::GetParticlesInUse( )
{
	return m_InUseCount;
}
//------------------------------------------------------------------------------
// Get the current number of un used particles

int tlParticleManager::GetParticlesUnUsed( )
{
	return m_UnUsedCount;
}
//------------------------------------------------------------------------------
// Adds a new effect to the particle manager
// Use this method to add new effects to the particle manager which will be updated automatically
	
void tlParticleManager::AddEffect(tlEffect * e)
{
	e->m_Next = m_Effects;
	m_Effects = e;
}
//------------------------------------------------------------------------------
// Removes an effect from the particle manager
// Use this method to remove effects from the particle manager. It's best to destroy the effect as well to avoid memory leaks
	
void tlParticleManager::RemoveEffect( tlEffect * e )
{
	tlEffect * prev = NULL;
	tlEffect * search = m_Effects;
	while ( search )
	{
		if ( search == e )
		{
			if ( prev )
				prev->m_Next = e->m_Next;
			else
				m_Effects = e->m_Next;

			e->m_Next = NULL;
			return;
		}
	}
}
	
//------------------------------------------------------------------------------
// Clear all particles in use
// Call this method to empty the list of in use particles and move them to the unUsed list.
	
void tlParticleManager::ClearInUse()
{
	for (int i=0; i<9; ++i)
	{
		while (m_InUse[i])
		{
			tlParticle * p = m_InUse[i];

			m_UnUsedCount++;
			m_InUseCount--;

			// remove from the InUse list
			m_InUse[i] = m_InUse[i]->m_Next;

			// add to UnUsed list
			p->m_Next = m_UnUsed;
			m_UnUsed = p;

			p->Reset();
		}
	}
}
//------------------------------------------------------------------------------
// Destroy the particle manager
// This will destroy the particle, clearing all effects and particles. Use only when you are finished with the particle manager and want it removed
// to avoid any memory leaks.
	
void tlParticleManager::Destroy()
{
	ClearAllEffects();
	ClearInUse();
	m_UnUsed = NULL;
}
	
//------------------------------------------------------------------------------
// Remove all effects and clear all particles in use
// if (you want to remove all effects and particles from the manager then use this command. Every effect will instantly stop being
// rendered.
	
void tlParticleManager::ClearAllEffects()
{
	tlEffect * e = m_Effects;

	while (e)
	{
		tlEffect * n = e->m_Next;
		e->Destroy();
		e = n;
	}
	// clear the list
	m_Effects = NULL;
}
	
//------------------------------------------------------------------------------
// Release single particles
// if (there are any singleparticles (see #SetSingleParticle) this will release all of them and allow them to m_Age and die.
	
void tlParticleManager::ReleaseParticles()
{
	for (int i=0; i<9; ++i)
	{
		tlParticle * p = m_InUse[i];
		while (p)
		{
			p->m_ReleaseSingleParticle = true;
			p = p->m_Next;
		}
	}
}

//------------------------------------------------------------------------------
// internal methods-------

void tlParticleManager::DrawEffects()
{
	tlEffect * e = m_Effects;
	while ( e )
	{
		// if tagged as destroyed, don't draw it
		if ( e->m_Destroyed == false )
		{
			DrawEffect( e );
		}
		e = e->m_Next;
	}
}
	
//------------------------------------------------------------------------------

void tlParticleManager::DrawEffect( tlEffect * effect )
{
	// Draw particles in each layer...
	for (int i=0; i<9; ++i)
	{
		tlParticle * p = effect->m_InUse[ i ];

		while ( p )
		{
			DrawParticle( p );

			tlEffect * e = (tlEffect*) p->m_Children;
			while ( e )
			{
				DrawEffect( e );
				e = (tlEffect*) e->m_NextSibling;
			}
			p = p->m_Next;
		}
	}
}
	
//------------------------------------------------------------------------------

void tlParticleManager::DrawParticle( tlParticle * p )
{
	tlEmitter * e = p->m_Emitter;

	if (p->m_Age || e->m_SingleParticle)
	{
		Font * font = m_FontAtlas[ e->m_BlendMode ];

		u32 glyph = (u32)p->m_CurrentFrame + e->m_BaseFrame;

		//glyph = 0;
		//p->m_Avatar = &font->m_FontFile->charArray[ glyph ];

		Vector2 alignment;
		if (e->m_HandleCenter)
		{
			alignment.x = -0.5f * p->m_Avatar->xSize;
			alignment.y = -0.5f * p->m_Avatar->ySize;
		}
		else
		{
			alignment.x = -p->m_Handle.x;
			alignment.y = -p->m_Handle.y;
		}

		Vector2 scale;
		scale.x = p->m_Scale.x * p->m_Zoom;
		scale.y = p->m_Scale.y * p->m_Zoom;

		// draw the particle.
		// ::Particle1 draws a single quad with rotation performed in the vertexShader

		if ( e->m_AngleRelative )
			font->Particle1( &p->m_World, p->m_Angle + p->m_RelativeAngle, &alignment, &scale, &p->m_Color, glyph );
		else
			font->Particle1( &p->m_World, p->m_Angle, &alignment, &scale, &p->m_Color, glyph );
	}
}
//------------------------------------------------------------------------------









































//------------------------------------------------------------------------------
// Create a new Emitter
// returns: a New tlEmitter with a default set of attribute values

tlEmitter * tlParticleManager::CreateParticle( tlEffect * parent )
{
	tlEmitter * e = new tlEmitter();

	e->m_PM = this;

	e->SetName( "New Particle" );

	e->c_Amount->Add(0.0f, 1.0f);
	e->c_Life->Add(0.0f, 1.0f/1000.0f);
	e->c_SizeX->Add(0.0f, 200.0f);
	e->c_SizeY->Add(0.0f, 200.0f);
	e->c_BaseSpeed->Add( 0.0f, 0.0f );
	e->c_BaseSpin->Add( 0.0f, 0.0f );
	e->c_BaseWeight->Add( 0.0f, 0.0f );
	
	e->c_VelVariation->Add( 0.0f, 0.0f );
	e->c_LifeVariation->Add( 0.0f, 0.0f );
	e->c_AmountVariation->Add( 0.0f, 0.0f );
	e->c_SizeXVariation->Add( 0.0f, 0.0f );
	e->c_SizeYVariation->Add( 0.0f, 0.0f );
	e->c_SpinVariation->Add( 0.0f, 0.0f );
	e->c_DirectionVariation->Add( 0.0f, 0.0f );
	e->c_WeightVariation->Add( 0.0f, 0.0f );
	
	e->c_R->Add( 0.0f, 1.0f );
	e->c_G->Add( 0.0f, 1.0f );
	e->c_B->Add( 0.0f, 1.0f );
	e->c_Alpha->Add( 0.0f, 1.0f );

	e->c_ScaleX->Add( 0.0f, 1.0f );
	e->c_ScaleY->Add( 0.0f, 1.0f );
	e->c_Spin->Add( 0.0f, 0.0f );
	e->c_Velocity->Add( 0.0f, 0.0f );
	e->c_Weight->Add( 0.0f, 0.0f );
	e->c_Direction->Add( 0.0f, 0.0f );
	e->c_DirectionVariationOT->Add( 0.0f, 0.0f );
	e->c_FrameRate->Add( 0.0f, 30.0f );
	e->c_GlobalVelocity->Add( 0.0f, 1.0f );
	
	e->SetUseEffectEmission(1);
	
	e->SetBlendMode(LIGHTBLEND);
	e->SetHandleCenter(true);
	
	e->m_ParentEffect = parent;
	
	return e;
}

//------------------------------------------------------------------------------
// Create a new emitter
// returns: Blank tlEmitter

tlEmitter * tlParticleManager::CreateEmitter()
{
	tlEmitter * e = new tlEmitter();
	e->m_PM = this;
	return e;
}

//------------------------------------------------------------------------------
// Create a new effect
// returns: A new effect with a default set of attributes.
// Pass the parent emitter if it is to be a sub effect.

tlEffect * tlParticleManager::CreateEffect( tlEmitter * parent )
{
	tlEffect * e = new tlEffect();

	e->m_PM = this;
	e->SetName("New Effect");

	if (parent)
	{
		e->SetParentEmitter(parent);
	}
	return e;
}


//------------------------------------------------------------------------------
// Get an effect from the tlLibrary.

tlEffect * tlParticleManager::GetEffectFromLibrary( const char * name )
{
	return m_Lib->GetEffect(name);
}

//------------------------------------------------------------------------------
// Makes a copy of the effect passed to it
// returns: A new clone of the effect entire, including all emitters and sub effects.

tlEffect * tlParticleManager::CopyEffect( tlEffect * e )
{
	tlEffect * eff = new tlEffect();

	eff->m_PM = this;
	eff->m_HashName = e->m_HashName;

	eff->SetEllipseArc( e->m_EllipseArc );
	eff->SetLockAspect( e->m_LockAspect );
	eff->SetClass( e->m_Class );
	eff->SetMGX( e->m_MGX );
	eff->SetMGY( e->m_MGY );
	eff->SetEmitAtPoints( e->m_EmitAtPoints );
	eff->SetEmissionType( e->m_EmissionType );
	eff->SetEffectLength( e->m_EffectLength );
	eff->SetTraverseEdge( e->m_TraverseEdge );
	eff->SetEndBehaviour( e->m_EndBehaviour );
	eff->SetReverseSpawn( e->m_ReverseSpawn );
	eff->SetSpawnDirection( );
	eff->SetDistanceSetByLife( e->m_DistanceSetByLife );
	eff->SetHandleCenter( e->m_HandleCenter );
	eff->SetHandleX( e->m_Handle.x );
	eff->SetHandleY( e->m_Handle.y );
	eff->AssignParticleManager(this);
	eff->SetOKToRender(false);

	tlEmitter * em = (tlEmitter*) e->m_Children;
	while ( em )
	{
		tlEmitter * ec = CopyEmitter( em );

		ec->m_ParentEffect = eff;
		ec->m_Parent = eff;
		eff->AddChild( ec );

		em = (tlEmitter*) em->m_NextSibling;
	}
	
	LinkEffectArrays( e, eff );

	return eff;
}

//------------------------------------------------------------------------------
// Load an effects library
// returns: New tlEffectsLibrary
// Pass the url of the library and pass TRUE or FALSE for compile if you want to compile all the effects or not.
// Effects can be retrieved from the library using #GetEffect

void tlParticleManager::LoadEffects( const char * filename )
{
	if ( filename == NULL )
		return;

	u32 size;
	char * buffer = (char*) BundlerLoad( filename, NULL, &size );

	if ( buffer == NULL )
		return;

	ezxml_t rootXML = ezxml_parse_str(buffer, size);
	//ezxml_t effectsXML = ezxml_child(rootXML, "EFFECTS");
	ezxml_t effectsXML = rootXML;

	if ( rootXML == NULL )
	{
		ASSERT(false, "Not a particle effects file");
		return;
	}

	// first. Load all the shapes only
	ezxml_t shapeXML = ezxml_child(rootXML, "SHAPES");
	if (shapeXML)
	{
		ezxml_t imageXML = shapeXML->child;

		while ( imageXML )
		{
			const char * url = ezxml_attr( imageXML, "URL" );
			u32 frames = StringToInt(ezxml_attr( imageXML, "FRAMES" ));
			u32 index = StringToInt(ezxml_attr( imageXML, "INDEX" ));
			m_Lib->AddShape( index, frames, url );
			imageXML = imageXML->ordered;
		}
	}

	// Search my children recursively.
	ezxml_t childXML = effectsXML->child;
	while ( childXML )
	{
		const char * nodeName = childXML->name;

		if ( StringCompare( nodeName, "EFFECT" ) )
		{
			//PRINTF(" tlEffect: %s\n", ezxml_attr( childXML, "NAME" ) );

			tlEffect * effect = LoadEffectXmlTree( childXML, m_Lib, NULL );
			effect->Compile_All( );
			m_Lib->AddEffect( effect );
		}
		else if ( StringCompare( nodeName, "FOLDER" ) )
		{
			ezxml_t folderChildXML = childXML->child;
			while (folderChildXML)
			{
				nodeName = folderChildXML->name;

				if ( StringCompare( nodeName, "EFFECT" ) )
				{
					//PRINTF(" tlEffect: %s\n", ezxml_attr( folderChildXML, "NAME" ) );

					tlEffect * effect = LoadEffectXmlTree( folderChildXML, m_Lib, NULL );
					effect->Compile_All( );
					m_Lib->AddEffect( effect );
				}
				folderChildXML = folderChildXML->ordered;
			}
		}
		childXML = childXML->ordered;
	}

	BundlerRelease( buffer );
	ezxml_free( rootXML );
}

//------------------------------------------------------------------------------

tlEmitter * tlParticleManager::CopyEmitter( tlEmitter * em )
{
	tlEmitter * ec = CreateEmitter();

	ec->m_HashName = em->m_HashName;
	ec->m_BaseFrame = em->m_BaseFrame;
	ec->m_FrameCount = em->m_FrameCount;
	ec->m_BaseWidth = em->m_BaseWidth;
	ec->m_BaseHeight = em->m_BaseHeight;
	ec->m_GradientSize = em->m_GradientSize;

	ec->SetUseEffectEmission( em->m_UseEffectEmission );
	ec->SetFrame( em->m_Frame ); // PRW baseframe too...
	ec->SetAngleType( em->m_AngleType );
	ec->SetAngleOffset( em->m_AngleOffset );
	ec->SetAngle( em->m_Angle );
	ec->SetBlendMode( em->m_BlendMode );
	ec->SetParticlesRelative( em->m_ParticlesRelative );
	ec->SetUniform( em->m_Uniform );
	ec->SetLockAngle( em->m_LockedAngle );
	ec->SetAngleRelative( em->m_AngleRelative );
	ec->SetHandleX( em->m_Handle.x );
	ec->SetHandleY( em->m_Handle.y );
	ec->SetSingleParticle( em->m_SingleParticle );
	ec->SetVisible( em->m_Visible );
	ec->SetRandomColor( em->m_RandomColor );
	ec->SetZLayer( em->m_ZLayer );
	ec->SetAnimate( em->m_Animate );
	ec->SetRandomStartFrame( em->m_RandomStartFrame );
	ec->SetAnimationDirection( em->m_AnimationDirection );
	ec->SetFrame( em->m_Frame );
	ec->SetColorRepeat( em->m_ColorRepeat );
	ec->SetAlphaRepeat( em->m_AlphaRepeat );
	ec->SetOneShot( em->m_OneShot );
	ec->SetHandleCenter( em->m_HandleCenter );
	ec->SetOnce( em->m_Once );
	ec->SetGroupParticles( em->m_GroupParticles );
	ec->SetOKToRender( false );

	// Bypassers
	ec->m_Bypass_Weight = em->m_Bypass_Weight;
	ec->m_Bypass_Speed = em->m_Bypass_Speed;
	ec->m_Bypass_Spin = em->m_Bypass_Spin;
	ec->m_Bypass_DirectionVariation = em->m_Bypass_DirectionVariation;
	ec->m_Bypass_Colour = em->m_Bypass_Colour;
	ec->m_Bypass_ScaleX = em->m_Bypass_ScaleX;
	ec->m_Bypass_ScaleY = em->m_Bypass_ScaleY;
	ec->m_Bypass_FrameRate = em->m_Bypass_FrameRate;
	ec->m_Bypass_Stretch = em->m_Bypass_Stretch;
	ec->m_Bypass_Splatter = em->m_Bypass_Splatter;

	tlEffect * e = em->m_Effects;
	while ( e )
	{
		ec->AddEffect( CopyEffect(e) );
		e = e->m_Next;
	}
	LinkEmitterArrays(em, ec);
	return ec;
}

//------------------------------------------------------------------------------

void tlParticleManager::LinkEffectArrays( tlEffect * efrom, tlEffect * eto )
{
	eto->m_OwnGradients = false;

	eto->c_Life = efrom->c_Life;
	eto->c_Amount = efrom->c_Amount;
	eto->c_SizeX = efrom->c_SizeX;
	eto->c_SizeY = efrom->c_SizeY;
	eto->c_Velocity = efrom->c_Velocity;
	eto->c_Weight = efrom->c_Weight;
	eto->c_Spin = efrom->c_Spin;
	eto->c_Alpha = efrom->c_Alpha;
	eto->c_EmissionAngle = efrom->c_EmissionAngle;
	eto->c_EmissionRange = efrom->c_EmissionRange;
	eto->c_Width = efrom->c_Width;
	eto->c_Height = efrom->c_Height;
	eto->c_Angle = efrom->c_Angle;
	eto->c_Stretch = efrom->c_Stretch;
	eto->c_GlobalZoom = efrom->c_GlobalZoom;
}

//------------------------------------------------------------------------------

void tlParticleManager::LinkEmitterArrays( tlEmitter * efrom, tlEmitter * eto )
{
	eto->m_OwnGradients = false;

	eto->c_Life = efrom->c_Life;
	eto->c_LifeVariation = efrom->c_LifeVariation;
	eto->c_Amount = efrom->c_Amount;
	eto->c_SizeX = efrom->c_SizeX;
	eto->c_SizeY = efrom->c_SizeY;
	eto->c_BaseSpeed = efrom->c_BaseSpeed;
	eto->c_BaseWeight = efrom->c_BaseWeight;

	eto->c_R = efrom->c_R;
	eto->c_G = efrom->c_G;
	eto->c_B = efrom->c_B;
	eto->c_BaseSpin = efrom->c_BaseSpin;
	eto->c_EmissionAngle = efrom->c_EmissionAngle;
	eto->c_EmissionRange = efrom->c_EmissionRange;
	eto->c_Splatter = efrom->c_Splatter;
	eto->c_VelVariation = efrom->c_VelVariation;
	eto->c_WeightVariation = efrom->c_WeightVariation;
	eto->c_AmountVariation = efrom->c_AmountVariation;
	eto->c_SizeXVariation = efrom->c_SizeXVariation;
	eto->c_SizeYVariation = efrom->c_SizeYVariation;
	eto->c_SpinVariation = efrom->c_SpinVariation;
	eto->c_DirectionVariation = efrom->c_DirectionVariation;
	eto->c_Alpha = efrom->c_Alpha;
	eto->c_ScaleX = efrom->c_ScaleX;
	eto->c_ScaleY = efrom->c_ScaleY;
	eto->c_Spin = efrom->c_Spin;
	eto->c_Velocity = efrom->c_Velocity;
	eto->c_Weight = efrom->c_Weight;
	eto->c_Direction = efrom->c_Direction;
	eto->c_DirectionVariationOT = efrom->c_DirectionVariationOT;
	eto->c_FrameRate = efrom->c_FrameRate;
	eto->c_Stretch = efrom->c_Stretch;
	eto->c_GlobalVelocity = efrom->c_GlobalVelocity;
}

//------------------------------------------------------------------------------

tlEffect * tlParticleManager::LoadEffectXmlTree( ezxml_t xml, tlLibrary * lib, tlEmitter * parent )
{
	tlEffect * e = new tlEffect();

	// allocate memory for the gradients.
	e->m_PM = this;

	e->m_OwnGradients = true;
	e->c_Amount = new ScalarGradient();
	e->c_Life = new ScalarGradient();
	e->c_SizeX = new ScalarGradient();
	e->c_SizeY = new ScalarGradient();
	e->c_Velocity = new ScalarGradient();
	e->c_Weight = new ScalarGradient();
	e->c_Spin = new ScalarGradient();
	e->c_Alpha = new ScalarGradient();
	e->c_EmissionAngle = new ScalarGradient();
	e->c_EmissionRange = new ScalarGradient();
	e->c_Width = new ScalarGradient();
	e->c_Height = new ScalarGradient();
	e->c_Angle = new ScalarGradient();
	e->c_Stretch = new ScalarGradient();
	e->c_GlobalZoom = new ScalarGradient();

	e->c_Amount->Add( 0.0f, 1.0f );
	e->c_Life->Add( 0.0f, 1.0f ); // this is a life multiplier. NOT life in ms
	e->c_SizeX->Add( 0.0f, 1.0f );
	e->c_SizeY->Add( 0.0f, 1.0f );
	e->c_Velocity->Add( 0.0f, 1.0f );
	e->c_Weight->Add( 0.0f, 1.0f );
	e->c_Spin->Add( 0.0f, 1.0f );
	e->c_Alpha->Add( 0.0f, 1.0f );
	e->c_EmissionAngle->Add( 0.0f, 0.0f );
	e->c_EmissionRange->Add( 0.0f, 0.0f );
	e->c_Width->Add( 0.0f, 1.0f );
	e->c_Height->Add( 0.0f, 1.0f );
	e->c_Angle->Add( 0.0f, 0.0f );
	e->c_Stretch->Add( 0.0f, 1.0f );
	e->c_GlobalZoom->Add( 0.0f, 1.0f );

	e->m_Class = StringToInt( ezxml_attr( xml, "TYPE" ) );
	e->m_EmitAtPoints = StringToInt( ezxml_attr( xml, "EMITATPOINTS" ) );
	e->m_MGX = StringToInt( ezxml_attr( xml, "MAXGX" ) );
	e->m_MGY = StringToInt( ezxml_attr( xml, "MAXGY" ) );
	e->m_EmissionType = StringToInt( ezxml_attr( xml, "EMISSION_TYPE" ) );
	e->m_EllipseArc = StringToF32( ezxml_attr( xml, "ELLIPSE_ARC" ) );
	e->m_EffectLength = StringToF32( ezxml_attr( xml, "EFFECT_LENGTH" ) );
	e->m_LockAspect = StringToInt( ezxml_attr( xml, "UNIFORM" ) );
	e->m_HandleCenter = StringToInt( ezxml_attr( xml, "HANDLE_CENTER" ) );
	e->m_Handle.x = StringToF32( ezxml_attr( xml, "HANDLE_X" ) );
	e->m_Handle.y = StringToF32( ezxml_attr( xml, "HANDLE_Y" ) );
	e->m_TraverseEdge = StringToInt( ezxml_attr( xml, "TRAVERSE_EDGE" ) );
	e->m_EndBehaviour = StringToInt( ezxml_attr( xml, "END_BEHAVIOUR" ) );
	e->m_DistanceSetByLife = StringToInt( ezxml_attr( xml, "DISTANCE_SET_BY_LIFE" ) );
	e->m_ReverseSpawn = StringToInt( ezxml_attr( xml, "REVERSE_SPAWN_DIRECTION" ) );
	e->SetName( ezxml_attr( xml, "NAME" ) );
	e->SetParentEmitter( parent );

	//PRINTF(" new EFFECT %s\n", ezxml_attr( xml, "NAME" ) );

	ezxml_t childXML = xml->child;
	while ( childXML )
	{
		const char * nodeName = childXML->name;

		if ( StringCompare( nodeName, "AMOUNT" ) )
		{
			e->c_Amount->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "LIFE" ) )
		{
			e->c_Life->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) ); // this is a life multiplier. NOT life in ms
		}
		else if ( StringCompare( nodeName, "SIZEX" ) )
		{
			e->c_SizeX->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "SIZEY" ) )
		{
			e->c_SizeY->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "VELOCITY" ) )
		{
			e->c_Velocity->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "WEIGHT" ) )
		{
			e->c_Weight->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "SPIN" ) )
		{
			e->c_Spin->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "ALPHA" ) )
		{
			e->c_Alpha->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "EMISSIONANGLE" ) )
		{
			e->c_EmissionAngle->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "EMISSIONRANGE" ) )
		{
			e->c_EmissionRange->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "AREA_WIDTH" ) )
		{
			e->c_Width->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "AREA_HEIGHT" ) )
		{
			e->c_Height->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "ANGLE" ) )
		{
			e->c_Angle->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "STRETCH" ) )
		{
			e->c_Stretch->Add( StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f, StringToF32(ezxml_attr(childXML, "VALUE" )) );
		}
		else if ( StringCompare( nodeName, "GLOBAL_ZOOM" ) )
		{
			f32 frame = StringToF32(ezxml_attr(childXML, "FRAME" ))/1000.0f;
			f32 zoom = StringToF32(ezxml_attr(childXML, "VALUE" ));
			e->c_GlobalZoom->Add( frame, zoom * m_Scale );
		}
		else if ( StringCompare( nodeName, "PARTICLE" ) )
		{
			tlEmitter * em = LoadEmitterXmlTree( childXML, lib, e);
			e->AddChild( em );
		}

		childXML = childXML->ordered;
	}

	//if (!e->stretch.Count()
		//e->addstretch 0, 1
	
	return e;
}

//------------------------------------------------------------------------------

tlEmitter * tlParticleManager::LoadEmitterXmlTree( ezxml_t xml, tlLibrary * lib, tlEffect * e )
{
	tlEmitter * p = new tlEmitter();

	p->m_PM = this;
	p->m_ParentEffect = e;

	// allocate graidents.
	p->m_OwnGradients = true;

	p->c_R = new ScalarGradient();
	p->c_G = new ScalarGradient();
	p->c_B = new ScalarGradient();
	p->c_BaseSpin = new ScalarGradient();
	p->c_Spin = new ScalarGradient();
	p->c_SpinVariation = new ScalarGradient();
	p->c_Velocity = new ScalarGradient();
	p->c_BaseWeight = new ScalarGradient();
	p->c_Weight = new ScalarGradient();
	p->c_WeightVariation = new ScalarGradient();
	p->c_BaseSpeed = new ScalarGradient();
	p->c_VelVariation = new ScalarGradient();
	p->c_Alpha = new ScalarGradient();
	p->c_SizeX = new ScalarGradient();
	p->c_SizeY = new ScalarGradient();
	p->c_ScaleX = new ScalarGradient();
	p->c_ScaleY = new ScalarGradient();
	p->c_SizeXVariation = new ScalarGradient();
	p->c_SizeYVariation = new ScalarGradient();
	p->c_LifeVariation = new ScalarGradient();
	p->c_Life = new ScalarGradient();
	p->c_Amount = new ScalarGradient();
	p->c_AmountVariation = new ScalarGradient();
	p->c_EmissionAngle = new ScalarGradient();
	p->c_EmissionRange = new ScalarGradient();
	p->c_GlobalVelocity = new ScalarGradient();
	p->c_Direction = new ScalarGradient();
	p->c_DirectionVariation = new ScalarGradient();
	p->c_DirectionVariationOT = new ScalarGradient();
	p->c_FrameRate = new ScalarGradient();
	p->c_Stretch = new ScalarGradient();
	p->c_Splatter = new ScalarGradient();

	p->SetName( ezxml_attr(xml, "NAME") );

	//PRINTF(" new EMITTER %s\n", ezxml_attr(xml, "NAME" ) );

	p->m_HandleCenter = StringToInt( ezxml_attr(xml, "HANDLE_CENTERED") );
	p->m_Handle.x = StringToF32( ezxml_attr(xml, "HANDLE_X") );
	p->m_Handle.y = StringToF32( ezxml_attr(xml, "HANDLE_Y") );

	p->m_BlendMode = StringToInt( ezxml_attr(xml, "BLENDMODE") ) - 3;  // we get values 3-4 from the particle.effect file. Convert to 0-1
	p->m_ParticlesRelative = StringToInt( ezxml_attr(xml, "RELATIVE") );
	//p->m_ParticlesRelative = false; //prwprw

	p->m_RandomColor = StringToInt( ezxml_attr(xml, "RANDOM_COLOR") );
	p->m_ZLayer = StringToInt( ezxml_attr(xml, "LAYER") );
	p->m_SingleParticle = StringToInt( ezxml_attr(xml, "SINGLE_PARTICLE") );
	p->m_Uniform = StringToInt( ezxml_attr(xml, "UNIFORM") );
	p->m_AngleType = StringToInt( ezxml_attr(xml, "ANGLE_TYPE") );
	p->m_AngleOffset = StringToF32( ezxml_attr(xml, "ANGLE_OFFSET") );
	p->m_LockedAngle = StringToInt( ezxml_attr(xml, "LOCK_ANGLE") );
	p->m_AngleRelative = StringToInt( ezxml_attr(xml, "ANGLE_RELATIVE") );
	p->m_UseEffectEmission = StringToInt( ezxml_attr(xml, "USE_EFFECT_EMISSION") );
	p->m_ColorRepeat = StringToInt( ezxml_attr(xml, "COLOR_REPEAT") );
	p->m_AlphaRepeat = StringToInt( ezxml_attr(xml, "ALPHA_REPEAT") );
	p->m_OneShot = StringToInt( ezxml_attr(xml, "ONE_SHOT") );
	p->m_GroupParticles = StringToInt( ezxml_attr(xml, "GROUP_PARTICLES") );

	// animation
	p->m_Animate = StringToInt( ezxml_attr(xml, "ANIMATE") );
	p->m_Once = StringToInt( ezxml_attr(xml, "ANIMATE_ONCE") );
	p->m_RandomStartFrame = StringToInt( ezxml_attr(xml, "RANDOM_START_FRAME") );
	p->m_AnimationDirection = StringToF32( ezxml_attr(xml, "ANIMATION_DIRECTION") );
	p->m_Frame = StringToInt( ezxml_attr(xml, "FRAME") );

	if (p->m_AnimationDirection == 0.0f)
		p->m_AnimationDirection = 1.0f;

	ezxml_t childXML = xml->child;

	while ( childXML )
	{
		const char * nodeName = childXML->name;

		if ( StringCompare( nodeName, "SHAPE_INDEX" ) )
		{
			char baseName[64];

			tlShape * shape = lib->GetShape( StringToInt(childXML->txt) );
			sprintf( baseName, "%s_0", shape->url );
			p->m_BaseFrame = m_FontAtlas[0]->FindGlyphByName( baseName );
			p->m_FrameCount = shape->frames;
			p->m_BaseWidth = m_FontAtlas[0]->GetWidth( p->m_BaseFrame );
			p->m_BaseHeight = m_FontAtlas[0]->GetHeight( p->m_BaseFrame );
		}
		else if ( StringCompare( nodeName, "LIFE" ) )
		{
			p->c_Life->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE"))/1000.0f );
		}
		else if ( StringCompare( nodeName, "AMOUNT" ) )
		{
			p->c_Amount->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "BASE_SPEED" ) )
		{
			p->c_BaseSpeed->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "BASE_WEIGHT" ) )
		{
			p->c_BaseWeight->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "BASE_SIZE_X" ) )
		{
			p->c_SizeX->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "BASE_SIZE_Y" ) )
		{
			p->c_SizeY->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "BASE_SPIN" ) )
		{
			p->c_BaseSpin->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "SPLATTER" ) )
		{
			p->c_Splatter->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "LIFE_VARIATION" ) )
		{
			p->c_LifeVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE"))/1000.0f );
		}
		else if ( StringCompare( nodeName, "AMOUNT_VARIATION" ) )
		{
			p->c_AmountVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "VELOCITY_VARIATION" ) )
		{
			p->c_VelVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "WEIGHT_VARIATION" ) )
		{
			p->c_WeightVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "SIZE_X_VARIATION" ) )
		{
			p->c_SizeXVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "SIZE_Y_VARIATION" ) )
		{
			p->c_SizeYVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "SPIN_VARIATION" ) )
		{
			p->c_SpinVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "DIRECTION_VARIATION" ) )
		{
			p->c_DirectionVariation->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "ALPHA_OVERTIME" ) )
		{
			p->c_Alpha->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "VELOCITY_OVERTIME" ) )
		{
			p->c_Velocity->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "WEIGHT_OVERTIME" ) )
		{
			p->c_Weight->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "SCALE_X_OVERTIME" ) )
		{
			p->c_ScaleX->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
			//p->c_ScaleX->Add( StringToF32(ezxml_attr(childXML, "FRAME")), 1.0f );
		}
		else if ( StringCompare( nodeName, "SCALE_Y_OVERTIME" ) )
		{
			p->c_ScaleY->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
			//p->c_ScaleY->Add( StringToF32(ezxml_attr(childXML, "FRAME")), 1.0f );
		}
		else if ( StringCompare( nodeName, "SPIN_OVERTIME" ) )
		{
			p->c_Spin->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "DIRECTION" ) )
		{
			p->c_Direction->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "DIRECTION_VARIATIONOT" ) )
		{
			p->c_DirectionVariationOT->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "FRAMERATE_OVERTIME" ) )
		{
			p->c_FrameRate->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "STRETCH_OVERTIME" ) )
		{
			p->c_Stretch->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "RED_OVERTIME" ) )
		{
			p->c_R->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE"))/255.0f );
		}
		else if ( StringCompare( nodeName, "GREEN_OVERTIME" ) )
		{
			p->c_G->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE"))/255.0f );
		}
		else if ( StringCompare( nodeName, "BLUE_OVERTIME" ) )
		{
			p->c_B->Add( StringToF32(ezxml_attr(childXML, "FRAME")), StringToF32( ezxml_attr(childXML, "VALUE"))/255.0f );
		}
		else if ( StringCompare( nodeName, "GLOBAL_VELOCITY" ) )
		{
			p->c_GlobalVelocity->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "EMISSION_ANGLE" ) )
		{
			p->c_EmissionAngle->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "EMISSION_RANGE" ) )
		{
			p->c_EmissionRange->Add( StringToF32(ezxml_attr(childXML, "FRAME"))/1000.0f, StringToF32( ezxml_attr(childXML, "VALUE")) );
		}
		else if ( StringCompare( nodeName, "EFFECT" ) )
		{
			tlEffect * e = LoadEffectXmlTree( childXML, lib, p );
			p->AddEffect( e );
		}

		childXML = childXML->ordered;
	}
	return p;
}
//------------------------------------------------------------------------------
#if 0
Function GetBezierValue:Float(lastec:tlAttributeNode, a:tlAttributeNode, t:Float, ymin:Float, ymax:Float)
	if (lastec
		if (a.isCurve
			if (lastec.isCurve
				Local p0:tlPoint = New tlPoint.Create(lastec.frame, lastec.value)
				Local p1:tlPoint = New tlPoint.Create(lastec.c1x, lastec.c1y)
				Local p2:tlPoint = New tlPoint.Create(a.c0x, a.c0y)
				Local p3:tlPoint = New tlPoint.Create(a.frame, a.value)
				Local value:tlPoint = GetCubicBezier(p0, p1, p2, p3, t, ymin, ymax)
				return value.y
			else {
				Local p0:tlPoint = New tlPoint.Create(lastec.frame, lastec.value)
				Local p1:tlPoint = New tlPoint.Create(a.c0x, a.c0y)
				Local p2:tlPoint = New tlPoint.Create(a.frame, a.value)
				Local value:tlPoint = GetQuadBezier(p0, p1, p2, t, ymin, ymax)
				return value.y
			}
		else {if (lastec.isCurve
			Local p0:tlPoint = New tlPoint.Create(lastec.frame, lastec.value)
			Local p1:tlPoint = New tlPoint.Create(lastec.c1x, lastec.c1y)
			Local p2:tlPoint = New tlPoint.Create(a.frame, a.value)
			Local value:tlPoint = GetQuadBezier(p0, p1, p2, t, ymin, ymax)
			return value.y
		else {
			return 0
		}
	else {
		return 0
	}
#endif
//------------------------------------------------------------------------------

Font * tlParticleManager::CreateFont( const char * pFilename, u32 maxChars )
{
	Font * pFont = NULL;
	Material * material = NULL;

	PRINTF(" Font::Create %s ", pFilename );

	FontFile * pFontFile = (FontFile *) ResourceGet( pFilename, RESOURCE_FONT );

	if ( pFontFile == NULL )
	{
		pFontFile = (FontFile *) BundlerLoad( pFilename, NULL );

		if ( pFontFile )
		{
			// add this resource to resouceManager so it can be shared
			ResourceAdd( pFilename, RESOURCE_FONT, (void *) pFontFile );

			material = Material::Load( pFilename );
			ASSERT( material, "ERROR [FontLoad]: Unable to load MATERIAL\n");

			// apply DEVICE/RESOURCE scale
			Font::ResizeForDevice( pFontFile, material );
		}
	}


	if ( pFontFile )
	{
		pFont = new Font( pFontFile->charCount );
		pFont->m_FontFile = pFontFile;
		pFont->m_MaxChars = maxChars;

		PRINTF("(Loaded %d glyphs)\n", pFont->m_FontFile->charCount );

		// Create a mesh object to write this font with
		pFont->m_Mesh = new Mesh( PRIMTYPE_TRILIST );
		IndexBuffer * iBuf = IndexBuffer::Create( USAGE_DYNAMIC, TYPE_16BIT, MemAlloc(sizeof(u16)*maxChars*6), sizeof(u16)*maxChars*6 );

		// order of these attributes must exactly match 'FontParticleOneRotationVertex'
		VertexBuffer * vBuf = new VertexBuffer( USAGE_DYNAMIC );
		vBuf->AddAttribute( VA_POSITION, 3, ATTRIB_TYPE_FLOAT, false );
		vBuf->AddAttribute( VA_TEXCOORD0, 2, ATTRIB_TYPE_FLOAT, false );
		vBuf->AddAttribute( VA_COLOR0, 4, ATTRIB_TYPE_UNSIGNED_BYTE, true );
		vBuf->AddAttribute( VA_CORNER, 2, ATTRIB_TYPE_FLOAT, false ); // corner coordinates
		vBuf->AddAttribute( VA_ANGLE, 1, ATTRIB_TYPE_FLOAT, false ); // rotation angle
		vBuf->AllocateBuffer( NULL, true, maxChars*4 );

		pFont->m_Mesh->AddIndexBuffer( iBuf );
		pFont->m_Mesh->AddVertexBuffer( vBuf );

		// Init the indexBuffer ( since, this will never change )
		//
		// we use an index buffer so we can draw each character with only 4 vertices. 
		// Without an indexBuffer, it would require 6 vertices ( to draw 2 triangles )
		u16 * indexBuffer = (u16 *) iBuf->Lock( );
		for ( int i=0; i<maxChars; ++i )
		{
			indexBuffer[ i * 6 + 0 ] = i * 4 + 0;
			indexBuffer[ i * 6 + 1 ] = i * 4 + 1;
			indexBuffer[ i * 6 + 2 ] = i * 4 + 3;
			indexBuffer[ i * 6 + 3 ] = i * 4 + 0;
			indexBuffer[ i * 6 + 4 ] = i * 4 + 3;
			indexBuffer[ i * 6 + 5 ] = i * 4 + 2;
		}
		iBuf->Unlock();

		if ( material == NULL )
			material = Material::Load( pFilename );
		pFont->SetMaterial( material );
	}
	else
	{
		ERROR("ERROR [FontLoad]: Unable to load FONT: %s\n", pFilename);
	}
	return pFont;
}
