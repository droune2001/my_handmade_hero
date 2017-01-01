#ifndef _HANDMADE_INTRINSICS_H_
#define _HANDMADE_INTRINSICS_H_

// TODO(nfauvet): remove math.h
#include "math.h"

inline int32
SignOf(int32 Value)
{
	int32 Result = ( Value >= 0 ) ? 1 : -1;
	//int32 Result = Value >> 31;
	return Result;
}

inline real32
SquareRoot( real32 Real32 )
{
	real32 Result = sqrtf( Real32 );
	return Result;
}

inline real32 
AbsoluteValue( real32 Real32 )
{
	real32 Result = fabsf( Real32 );
	return Result;
}

inline uint32
RotateLeft( uint32 Value, int32 Amount )
{
	uint32 Result = _rotl( Value, Amount );
	return Result;
}

inline uint32
RotateRight( uint32 Value, int32 Amount )
{
	uint32 Result = _rotr( Value, Amount );
	return Result;
}

inline int32 
RoundReal32ToInt32( real32 Real32 )
{
	//int32 Result = (int32)( Real32 + 0.5f );
	int32 Result = (int32)roundf( Real32 );
	return Result;
}

inline uint32 
RoundReal32ToUInt32( real32 Real32 )
{
	//uint32 Result = (uint32)( Real32 + 0.5f );
	uint32 Result = (uint32)roundf( Real32 );
	return Result;
}

inline int32 
FloorReal32ToInt32( real32 Real32 )
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

inline int32 
TruncateReal32ToInt32( real32 Real32 )
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

struct bit_scan_result
{
	bool32 Found;
	uint32 Index;
};

inline bit_scan_result
FindLeastSignificantSetBit( uint32 Value )
{
	bit_scan_result Result = {};

#if COMPILER_MSVC
	Result.Found = (bool32)_BitScanForward( (unsigned long*)&Result.Index, Value );
#else
	for ( uint32 Test = 0; Test < 32; ++Test )
	{
		if ( Value & ( 1 << Test ) )
		{
			Result.Index = Test;
			Result.Found = true;
			break;
		}
	}
#endif

	return Result;
}

#endif // _HANDMADE_INTRINSICS_H_
