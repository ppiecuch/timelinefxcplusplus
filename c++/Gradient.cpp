// 
//	File	   	-> Gradient.cpp
//	Created		-> Oct 16, 2013
//	Author 		-> Peter Ward
//	Description	-> gradient support ( for color gradients, scalar gradients, etc )
//

#include "Gradient.h"

//------------------------------------------------------------------------------

ScalarGradient::ScalarGradient() : m_Lookup(NULL), m_LastIndex(0)
{
	// create the array, capable of holding 8 keys, growing by 8 keys ( 8 is probably more than enough for most cases )
	m_Array = DArrayCreate( sizeof(ScalarKey), 8, 8 );

	// add first key ( user may and probably will replace it ) at time index 0.0f;
	Add( 0.0f, 1.0f );
}

ScalarGradient::~ScalarGradient()
{
	DArrayDestroy( m_Array );
	if ( m_Lookup )
		free( m_Lookup );
}

//------------------------------------------------------------------------------
// Add a new entry to this gradient.
//
// NOTE:
// Adding must be done in increasing TIME order!

void ScalarGradient::Add( f32 time, f32 value )
{
	// 1st check if time index already exists. If it does, just replace the value.
	int count = _DArrayLength( m_Array );
	ScalarKey * key = (ScalarKey *) _DArrayGet( m_Array, 0 );
    for (int i=0; i<count; ++i, ++key)
	{
        if ( key->time == time )
		{
			key->value = value;
			return;
        }
    }
	key = (ScalarKey *) DArrayInc( m_Array );
	key->time = time;
	key->value = value;
}

//------------------------------------------------------------------------------
// Get the value along the gradient at time index 'time' ( time is a normalized 0-1 value )

f32 ScalarGradient::Get( f32 time )
{
	// is there a lookup table available for super fast gradients ?
	if ( m_Lookup )
	{
		int index = time * m_LastIndex;
		return m_Lookup[ index ];
	}

	ScalarKey * startKey = (ScalarKey *) _DArrayGet( m_Array, 0 );
    int count = _DArrayLength( m_Array );

	if ( count > 1 )
	{
		ScalarKey * key = startKey+1;
		for (int i=1; i<count; ++i, ++key)
		{
			if ( key->time >= time )
			{
				f32 factor = (time - startKey->time) / (key->time - startKey->time);
				return startKey->value + (key->value - startKey->value) * factor;
			}
			startKey = key;
		}
	}
	return startKey->value;
}

f32 ScalarGradient::GetMaxValue( )
{
    int count = _DArrayLength( m_Array );
	ScalarKey * key = (ScalarKey *) _DArrayGet( m_Array, 0 );
	f32 max = key->value;

	for (int i=0; i<count; ++i, ++key)
	{
		if ( key->value > max )
		{
			max = key->value;
		}
	}
	return max;
}

f32 ScalarGradient::GetMaxTime( )
{
    int count = _DArrayLength( m_Array );
	ScalarKey * key = (ScalarKey *) _DArrayGet( m_Array, 0 );
	f32 max = key->time;

	for (int i=0; i<count; ++i, ++key)
	{
		if ( key->time > max )
		{
			max = key->time;
		}
	}
	return max;
}


//------------------------------------------------------------------------------

void ScalarGradient::BuildLookup( f32 freq, int size )
{
	f32 delta = 1.0f/freq;
	f32 time = 0.0f;

	if ( m_Lookup )
	{
		free( m_Lookup );
		m_Lookup = NULL;
	}

	if ( size == 0 )
	{
		size = (int) ((GetMaxTime() * freq) + 2.0f);
	}
	else
	{
		delta = 1.0f / (size-1);
	}

	// lookups are only useful if our gradient has more than 1 entry. But to keep the code fast (prevent extra checks), we always use a lookup table ( sometimes it is just only 1 entry )
	if ( (_DArrayLength( m_Array ) <  2) || (size < 1) )
		size = 1;

	f32 * lookup = (f32*) malloc( sizeof(f32) * size );
	m_LastIndex = size-1;

	for (int i=0; i<size; ++i)
	{
		lookup[i] = Get( time );
		time += delta;
	}
	m_Lookup = lookup;
}


void ScalarGradient::Dump( )
{
    int count = _DArrayLength( m_Array );

	PRINTF("ScalarGradient DUMP (%d)\n", count );

	for (int i=0; i<count; ++i)
	{
		ScalarKey * key = (ScalarKey *) _DArrayGet( m_Array, i );
		PRINTF("Key %f = %f\n", key->time, key->value);
	}

	for (int i=0; i<=m_LastIndex; ++i)
	{
		PRINTF("%d %f\n", i, m_Lookup[i]);
	}
	PRINTF("\n");
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

