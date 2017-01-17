// 
//	File	   	-> Gradient.h
//	Created		-> Oct 16, 2013
//	Author 		-> Peter Ward
//	Description	-> Gradient support
//

#ifndef __GRADIENT__H__
#define __GRADIENT__H__

#include "tltypes.h"

//------------------------------------------------------------------------------

typedef struct _ScalarKey
{
	f32 time;
	f32 value;

} ScalarKey;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class ScalarGradient
{
public:
	ScalarGradient();
	~ScalarGradient();

	void Add( f32 time, f32 value );
	int GetLastIndex() {return m_LastIndex;}		// last valid index into the tables
	int GetTableSize() {return m_LastIndex+1;}		// return the size of the lookup table

	void BuildLookup( f32 freq, int size=0 );

	f32 Get( f32 time );

	void Dump();

	f32 GetMaxValue();			// get max value
	f32 GetMaxTime();			// get max time

	f32 Get( int index )
	{
		if ( index <= m_LastIndex )
			return m_Lookup[ index ];
		else
			return m_Lookup[ m_LastIndex ];
	}

private:
	DArray * m_Array;	// Array of 'KeyFrame' colors

	int m_LastIndex;
	f32 * m_Lookup;
};

//------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------
