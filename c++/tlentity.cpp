// 
//	File	   	-> tlEntity.cpp
//	Created		-> Dec 04 2013
//	Author 		-> Peter Ward
//	Description	-> Support for particles created via TimeLineFX particle editor
//
// http://subversion.assembla.com/svn/timelinefx-module/timelinefx.mod/timelinefx.bmx

#include "tlentity.h"
#include "tlparticlemanager.h"

//------------------------------------------------------------------------------

tlEntity::tlEntity() : m_HashName(0)
{
	m_Age = 0.0f;
	m_LifeTime = 0.0f;

	m_Scale.x = 1.0f;
	m_Scale.y = 1.0f;

	m_Size.x = 1.0f;
	m_Size.y = 1.0f;

	m_Color.r = 1.0f;
	m_Color.g = 1.0f;
	m_Color.b = 1.0f;
	m_Color.a = 1.0f;

	m_Pos.x = 0.0f;
	m_Pos.y = 0.0f;
	m_Pos.z = 0.0f;

	m_World.x = 0.0f;
	m_World.y = 0.0f;
	m_World.z = 0.0f;

	m_Zoom = 1.0f;

	m_Angle = 0.0f;
	m_RelativeAngle = 0.0f;

	m_Width = 0.0f;
	m_Height = 0.0f;
	m_Weight = 0.0f;
	m_Gravity = 0.0f;
	m_BaseWeight = 0.0f;

	m_FrameRate = 1.0f;

	m_Relative = true;
	m_Dead = false;
	m_Destroyed = false;
	m_RunChildren = false;
	m_UpdateSpeed = true;
	m_HandleCenter = true;

	m_PM = NULL;

	m_BlendMode = ALPHABLEND;

	m_Parent = NULL;
	m_RootParent = NULL;

	m_ChildCount = 0;
	m_Children = NULL;
	m_PrevSibling = NULL;
	m_NextSibling = NULL;

	MatrixIdentity( &m_Matrix );
}

//------------------------------------------------------------------------------

tlEntity::~tlEntity()
{
}
//------------------------------------------------------------------------------
// Destroy the entity
// This will destroy the entity and all it's children, ensuring all references are removed. Best to call this
// when you're finished with the entity to avoid memory leaks.
	
void tlEntity::Destroy()
{
	m_Parent = NULL;
	m_RootParent = NULL;

	tlEntity * e = m_Children;
	while ( e )
	{
		tlEntity * n = (tlEntity*) e->m_NextSibling;
		e->Destroy();
		e = n;
	}
	m_Children = NULL;
	m_Destroyed = true;
}

//------------------------------------------------------------------------------
// Update the entity
// Updates its coordinates based on the current speed (cs), applies any gravity, updates the current frame of the image and also updates the world coordinates. World
// coordinates (wx and wy) represent the location of the entity where it will be drawn on screen. This is calculated based on where the entity is in relation to
// it's parent entities. Update also updates the entity's children so only a single call to the parent entity is required to see all the children updated.
	
void tlEntity::Update( )
{
	f32 dTime = TimeGetElapsedSeconds();

	// Update speed in pixels per second
	if (m_UpdateSpeed)
	{
		f32 speed =  m_Speed * dTime * m_Zoom;

		m_Pos.x += MathCos(m_Direction) * speed;
		m_Pos.y -= MathSin(m_Direction) * speed;
	}
		
	// update the gravity
	if (m_Weight != 0.0f)
	{
		m_Gravity += m_Weight * dTime;
		m_Pos.y += (m_Gravity * dTime) * m_Zoom;
	}
	
	// set the matrix if it is relative to the parent
	if ( m_Relative )
	{
		_MatrixRotationZ( &m_Matrix, m_Angle );
	}
	
	// calculate where the entity is in the world
	if ( m_Parent && m_Relative )
	{
		SetZoom( m_Parent->m_Zoom );

		_MatrixMultiply( &m_Matrix, &m_Matrix, &m_Parent->m_Matrix );

		Vector2 temp;
		Vector2Rotate( &temp, (Vector2*)&m_Pos, &m_Matrix );

		m_World.x = m_Parent->m_World.x + (temp.x * m_Zoom);
		m_World.y = m_Parent->m_World.y + (temp.y * m_Zoom);

		m_RelativeAngle = m_Parent->m_RelativeAngle + m_Angle;
	}
	else
	{
		m_World.x = m_Pos.x;
		m_World.y = m_Pos.y;
	}
	
	if (m_Parent == NULL)
	{
		m_RelativeAngle = m_Angle;
	}
	
	// update the children		
	UpdateChildren();

}
//------------------------------------------------------------------------------
// A mini update called when the entity is created
// This is sometimes necessary to get the correct world coordinates for when new entities are spawned so that tweening is updated too. Otherwise
// you might experience the entity shooting across the screen as it tweens between locations.
	
void tlEntity::MiniUpdate( )
{
	_MatrixRotationZ( &m_Matrix, m_Angle );
		
	if (m_Parent && m_Relative)
	{
		SetZoom( m_Parent->m_Zoom );

		_MatrixMultiply( &m_Matrix, &m_Matrix, &m_Parent->m_Matrix );

		Vector2 temp;
		Vector2Rotate( &temp, (Vector2*)&m_Pos, &m_Matrix );

		m_World.x = m_Parent->m_World.x + (temp.x * m_Zoom);
		m_World.y = m_Parent->m_World.y + (temp.y * m_Zoom);
	}
	else
	{
		if (m_Parent)
		{
			SetZoom(m_Parent->m_Zoom);
		}

		m_World.x = m_Pos.x;
		m_World.y = m_Pos.y;
	}
}		
//------------------------------------------------------------------------------
// Update all children of this entity.
	
void tlEntity::UpdateChildren()
{
	tlEntity * e = (tlEntity*) m_Children;
	while ( e )
	{
		tlEntity * n = (tlEntity*) e->m_NextSibling;
		e->Update();
		e = n;
	}
}
//------------------------------------------------------------------------------
// Assign the root parent of the entity
// This assigns the root parent of the entity which will be the highest level in the entity hierarchy. This method is generally only used
// internally, when an entity is added as a child to another entity.
	
void tlEntity::AssignRootParent( tlEntity * e )
{
	if (m_Parent)
		m_Parent->AssignRootParent(e);
	else
		e->m_RootParent = this;
}
//------------------------------------------------------------------------------
// Change the level of zoom for the particle.
	
void tlEntity::Zoom( f32 zoom )
{
	m_Zoom += zoom;
}
//------------------------------------------------------------------------------
// Add a new child entity to the entity
// This will also automatically set the childs parent.
			
void tlEntity::AddChild(tlEntity * e)
{
	// add to double linked list
	e->m_PrevSibling = NULL;
	e->m_NextSibling = m_Children;
	if ( m_Children ) m_Children->m_PrevSibling = e;
	m_Children = e;

	e->m_Parent = this;
	e->AssignRootParent(e);
	m_ChildCount++;
}

//------------------------------------------------------------------------------
// Remove a child entity from this entity's list of children
	
void tlEntity::RemoveChild( tlEntity * e )
{
	// unlink this child.
	if ( e->m_PrevSibling )
		e->m_PrevSibling->m_NextSibling = e->m_NextSibling;
	else
		m_Children = e->m_NextSibling;

	if (e->m_NextSibling )
		e->m_NextSibling->m_PrevSibling = e->m_PrevSibling;

	e->m_Parent = NULL;
	e->m_PrevSibling = NULL;
	e->m_NextSibling = NULL;
	m_ChildCount--;
}
//------------------------------------------------------------------------------
// Clear all child entities from this list of children
// This completely destroys them so the garbage collector can free the memory
	
void tlEntity::ClearChildren()
{
	while ( m_Children )
	{
		m_Children->Destroy();
	}
	m_ChildCount = 0; // should already be at zero.
}
//------------------------------------------------------------------------------
// Recursively kills all child entities and any children within those too and so on.
// This sets all the children's dead field to true so that you can tidy them later on however you need. if (you just want to 
// get rid of them completely use #clearchildren.
	
void tlEntity::KillChildren()
{
	tlEntity * e = (tlEntity*) m_Children;
	while ( e )
	{
		e->KillChildren();

		e->m_Dead = true;
		e = (tlEntity*) e->m_NextSibling;
	}
}
//------------------------------------------------------------------------------
// Rotate the entity by the number of degrees you pass to it
	
void tlEntity::Rotate( f32 degrees )
{
	m_Angle += degrees;
}
//------------------------------------------------------------------------------
// Move the entity by the amount x and y that you pass to it
	
void tlEntity::Move( f32 xAmount, f32 yAmount )
{
	m_Pos.x += xAmount;
	m_Pos.y += yAmount;
}
//------------------------------------------------------------------------------
// Get the red value in this tlEntity object.
f32 tlEntity::GetRed()
{
	return m_Color.r;
}
//------------------------------------------------------------------------------
// Set the red value for this tlEntity object.
void tlEntity::SetRed( f32 r )
{
	m_Color.r = r;
}
//------------------------------------------------------------------------------
// Get the green value in this tlEntity object.

f32 tlEntity::GetGreen()
{
	return m_Color.g;
}
//------------------------------------------------------------------------------
// Set the green value for this tlEntity object.

void tlEntity::SetGreen( f32 g )
{
	m_Color.g = g;
}
//------------------------------------------------------------------------------
// Get the blue value in this tlEntity object.
f32 tlEntity::GetBlue()
{
	return m_Color.b;
}
//------------------------------------------------------------------------------
// Set the blue value for this tlEntity object.

void tlEntity::SetBlue( f32 b )
{
	m_Color.b = b;
}
//------------------------------------------------------------------------------
// Set the colour for the tlEntity object.

void tlEntity::SetEntityColor( f32 r, f32 g, f32 b )
{
	m_Color.r = r;
	m_Color.g = g;
	m_Color.b = b;
}
//------------------------------------------------------------------------------
// Get the alpha value in this tlEntity object.
f32 tlEntity::GetEntityAlpha()
{
	return m_Color.a;
}
//------------------------------------------------------------------------------
// Set the alpha value for this tlEntity object.

void tlEntity::SetEntityAlpha( f32 alpha )
{
	m_Color.a = alpha;
}
//------------------------------------------------------------------------------
// Set the current x coordinate of the entity
// This will be relative to the parent if relative is set to true
	
void tlEntity::SetX( f32 x )
{
	m_Pos.x = x;
}
//------------------------------------------------------------------------------
// Set the current y coordinate of the entity
// This will be relative to the parent if relative is set to true
	
void tlEntity::SetY( f32 y )
{
	m_Pos.y = y;
}
//------------------------------------------------------------------------------
// Set the current zoom level of the entity
	
void tlEntity::SetZoom( f32 zoom )
{
	m_Zoom = zoom;
}
//------------------------------------------------------------------------------
// Get the current x coordinate of the entity
// This will be relative to the parent if relative is set to true
	
f32 tlEntity::GetX()
{
	return m_Pos.x;
}
//------------------------------------------------------------------------------
// Get the current y coordinate of the entity
// This will be relative to the parent if relative is set to true
	
f32 tlEntity::GetY()
{
	return m_Pos.y;
}
//------------------------------------------------------------------------------
// The the x and y position of the entity
// This will be relative to the parent if relative is set to true
	
void tlEntity::SetPosition( f32 x, f32 y )
{
	m_Pos.x = x;
	m_Pos.y = y;
	m_Pos.z = 0.0f;
}
//------------------------------------------------------------------------------
// Set the current world x coordinate of the entity
	
void tlEntity::SetWX( f32 x )
{
	m_World.x = x;
}
//------------------------------------------------------------------------------
// Set the current world y coordinate of the entity
	
void tlEntity::SetWY( f32 y )
{
	m_World.y = y;
}
//------------------------------------------------------------------------------
// Set to true to position the handle of the entity at the center
	
void tlEntity::SetAutoCenter( bool state )
{
	m_HandleCenter = state;
}
//------------------------------------------------------------------------------
// Set the current angle of rotation of the entity
	
void tlEntity::SetAngle( f32 angle )
{
	m_Angle = angle;
}
//------------------------------------------------------------------------------
// Set the current blendmode of the entity ie., ALPHABLEND/LIGHTBLEND
	
void tlEntity::SetBlendMode( int mode )
{
	m_BlendMode = mode;
}
//------------------------------------------------------------------------------
// Set the entities x handle
// This will not apply if autocenter is set to true
	
void tlEntity::SetHandleX( f32 x )
{
	m_Handle.x = x;
}
//------------------------------------------------------------------------------
// Set the entities y handle
// This will not apply if autocenter is set to true
	
void tlEntity::SetHandleY( f32 y )
{
	m_Handle.y = y;
}
//------------------------------------------------------------------------------
// Set the name of the entity
	
void tlEntity::SetName( const char * name )
{
	m_HashName = HashFromString(name);
}
//------------------------------------------------------------------------------
// Set the parent of the entity
// Entities can have parents and children. Entities are drawn relative to their parents if the relative flag is set to true. 
// Using this command and #addchild you can create a hierarchy of entities. There's no need to call #addchild as well as #setparent
// because both commands will automatically set both accordingly
	
void tlEntity::SetParent(tlEntity * e)
{
	e->AddChild(this);
}
//------------------------------------------------------------------------------
// Sets whether this entity remains relative to it's parent
// Entities that are relative to their parent entity will position, rotate and scale with their parent.
	
void tlEntity::SetRelative( bool state )
{
	m_Relative = state;
}
//------------------------------------------------------------------------------
// Sets the scale of the entity.
// This will set both the x and y scale of the entity based on the values you pass it.
	
void tlEntity::SetEntityScale( f32 sx, f32 sy )
{
	m_Scale.x = sx;
	m_Scale.y = sy;
}
//------------------------------------------------------------------------------
// Set the current speed of the entity
// Sets the speed which is measured in pixels per second
	
void tlEntity::SetSpeed( f32 speed )
{
	m_Speed = speed;
}
//------------------------------------------------------------------------------
// Get the current speed of the entity
// Gets the speed which is measured in pixels per second
	
f32 tlEntity::GetSpeed()
{
	return m_Speed;
}
//------------------------------------------------------------------------------
// Get the framerate value in this tlEntity object.
// see #SetFrameRate for more info.

f32 tlEntity::GetFrameRate()
{
	return m_FrameRate;
}
//------------------------------------------------------------------------------
// Set the framerate value for this tlEntity object.
// the frame rate dicates how fast the entity animates if it has more then 1 frame
// of animation. The framerate is measured in frames per second.
void tlEntity::SetFrameRate(f32 rate)
{
	m_FrameRate = rate;
}
//------------------------------------------------------------------------------
// returns true if the entity is animating 

bool tlEntity::GetAnimating()
{
	return m_Animating;
}
//------------------------------------------------------------------------------
// Set to true to make the entity animate

void tlEntity::SetAnimating( bool state )
{
	m_Animating = state;
}

//------------------------------------------------------------------------------
// Set to true to make the entity animate just once
// Once the entity as reached the end of the animation cycle it will stop animating.
	
void tlEntity::SetAnimateOnce( bool state )
{
	m_AnimateOnce = state;
}
//------------------------------------------------------------------------------
// Set the UpdateSpeed value for this tlEntity object.
// Set to true or false, default is true. Setting to false will make the update method
// ignore any speed calculations. This is useful in situations where you want to extend tlEntity
// and calculate speed in your own way.

void tlEntity::SetUpdateSpeed( bool state )
{
	m_UpdateSpeed = state;
}
//------------------------------------------------------------------------------
// Get the current angle of rotation
	
f32 tlEntity::GetAngle()
{
	return m_Angle;
}
//------------------------------------------------------------------------------
// Get the current entity handle x
	
f32 tlEntity::GetHandleX()
{
	return m_Handle.x;
}
//------------------------------------------------------------------------------
// Get the current entity image handle y
	
f32 tlEntity::GetHandleY()
{
	return m_Handle.y;
}
//------------------------------------------------------------------------------
// True if the handle of the entity is at the center of the image

bool tlEntity::GetHandleCenter()
{
	return m_HandleCenter;
}

//------------------------------------------------------------------------------
// Get the current blendmode
	
int tlEntity::GetBlendMode()
{
	return m_BlendMode;
}
//------------------------------------------------------------------------------
// Get whether this entity is relative to it's parent
	
bool tlEntity::GetRelative()
{
	return m_Relative;
}
//------------------------------------------------------------------------------
// Gets the x and y scale of the entity.
	
void tlEntity::GetEntityScale( f32 * sx, f32 * sy )
{
	 *sx = m_Scale.x;
	 *sy = m_Scale.y;
}

//------------------------------------------------------------------------------
// Get the current parent of the entity
// Entities can have parents and children. Entities are drawn relative to their parents if the relative flag is true. 
// Using this command and #addchild you can create a hierarchy of entities.
	
tlEntity * tlEntity::GetParent()
{
	return m_Parent;
}
//------------------------------------------------------------------------------
// Get the children that this entity has
// This will return a list of children that the entity currently has
	
tlEntity * tlEntity::GetChildren()
{
	return m_Children;
}
//------------------------------------------------------------------------------
// Get the lifetime value in this tlEntity object.
// See #SetLifeTime

f32 tlEntity::GetLifeTime()
{
	return m_LifeTime;
}
//------------------------------------------------------------------------------
// Set the lifetime value for this tlEntity object.
// LifeTime represents the length of time that the entity should "Live" for. This allows entities to decay and expire in a given time.
// LifeTime is measured in seconds. See #SetAge and #Decay for adjusting the m_Age of the entity.

void tlEntity::SetLifeTime( f32 life )
{
	m_LifeTime = life;
}
//------------------------------------------------------------------------------
// Get the m_Age value in this tlEntity object.
// See #SetAge and #Decay.

f32 tlEntity::GetAge()
{
	return m_Age;
}
//------------------------------------------------------------------------------
// Set the m_Age value for this tlEntity object.
// Setting the m_Age of of the entity allows you to keep track of how old the entity so that something can happen after a given amount of time.
// See #Decay to increase the m_Age by a given amount.
void tlEntity::SetAge( f32 m_Age )
{
	m_Age = m_Age;
}
//------------------------------------------------------------------------------
// Increases the m_Age value by a given amount

void tlEntity::Decay( f32 seconds )
{
	m_Age += seconds;
}
//------------------------------------------------------------------------------
// Get the wx value in this tlEntity object.
// WX represents the current x world coordinate of the entity. This may differ to x, which will contain the x coordinate relative 
// to the parent entity

f32 tlEntity::GetWX()
{
	return m_World.x;
}
//------------------------------------------------------------------------------
// Get the wy value in this tlEntity object.
// WY represents the current Y world coordinate of the entity. This may differ to y, which will contain the Y coordinate relative 
// to the parent entity

f32 tlEntity::GetWY()
{
	return m_World.y;
}
//------------------------------------------------------------------------------
// Get the direction value in this tlEntity object.
// Get the current direction the entity is travelling in
f32 tlEntity::GetEntityDirection()
{
	return m_Direction;
}
//------------------------------------------------------------------------------
// Set the direction value for this tlEntity object.
// Set the current direction the entity is travelling in
void tlEntity::SetEntityDirection( f32 dir )
{
	m_Direction = dir;
}
//------------------------------------------------------------------------------
// Get the oktorender value in this tlEntity object
// see #SetOKToRender

bool tlEntity::GetOKToRender( )
{
	return m_OkToRender;
}
//------------------------------------------------------------------------------
// Set the oktorender value for this tlEntity object.
// Somethimes you might not always want entities to be rendered. When the render method is called, it will always render the children aswell,
// but if some of those children are effects which are rendered by a particle manager, then you don't want them rendered twice, so you can set this
// to false to avoid them being rendered.

void tlEntity::SetOKToRender( bool state )
{
	m_OkToRender = state;
}
//------------------------------------------------------------------------------
