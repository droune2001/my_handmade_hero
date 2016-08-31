#ifndef _HANDMADE_INTRINSICS_H_
#define _HANDMADE_INTRINSICS_H_

// TODO(nfauvet): remove math.h
#include "math.h"

inline internal
int32 RoundReal32ToInt32( real32 Real32 )
{
	int32 Result = (int32)( Real32 + 0.5f );
	return Result;
}

inline internal
uint32 RoundReal32ToUInt32( real32 Real32 )
{
	int32 Result = (uint32)( Real32 + 0.5f );
	return Result;
}

inline internal
int32 FloorReal32ToInt32( real32 Real32 )
{
	if ( Real32 < 0.0f )
	{
		return (int32)( Real32 - 1.0f );
	}
	else
	{
		return (int32)( Real32 );
	}
}

inline internal
int32 TruncateReal32ToInt32( real32 Real32 )
{
	int32 Result = (int32)Real32;
	return Result;
}

inline real32
Sin( real32 Angle )
{
	real32 Result = sinf( Angle );
	return Result;
}

inline real32
Cos( real32 Angle )
{
	real32 Result = cosf( Angle );
	return Result;
}

inline real32
ATan2( real32 Y, real32 X )
{
	real32 Result = atan2f( Y, X );
	return Result;
}

#endif // _HANDMADE_INTRINSICS_H_
