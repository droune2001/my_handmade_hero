#ifndef HANDMADE_H
#define HANDMADE_H

/*
HANDMADE_INTERNAL:
0 - Build for public release
1 - Build for developer only

HANDMADE_SLOW:
0 - fast
1 - slow
*/

#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.1415926535f

#if HANDMADE_SLOW
#	define Assert(Expression) if(!(Expression)){*(int *)0 = 0;}
#else
#	define Assert(Expression)
#endif

inline uint32 SafeTruncateSize32( uint64 Value )
{
	Assert( Value <= 0xFFFFFFFF );
	uint32 Result = (uint32)Value;
	return Result;
}

#define Kilobytes(val) ((val)*1024LL)
#define Megabytes(val) (Kilobytes(val)*1024LL)
#define Gigabytes(val) (Megabytes(val)*1024LL)
#define Terabytes(val) (Gigabytes(val)*1024LL)

#define ArrayCount(tab) (sizeof(tab) / sizeof((tab)[0]))

// accessor just to add an assert.
inline game_controller_input *GetController( game_input *Input, int ControllerIndex ) 
{
    Assert( ControllerIndex < ArrayCount( Input->Controllers ) );
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return Result;
}

//
//
//

struct canonical_position
{
	// TODO(nfauvet): pack the TileMapX/Y and TileX/Y into one uint
	// => virtual system into a tile store
	// => 28bits for chunks index (page)
	// => 4bits for chunks inside index
	int32 TileMapX; // which tilemap in the world
	int32 TileMapY;

	int32 TileX; // which tile index in the tilemap
	int32 TileY;

	// TODO(nfauvet): converts these to math friendly resolution independant world units.
	real32 X; // position inside a tile, relative to top left corner.
	real32 Y;
};

struct raw_position
{
	int32 TileMapX;
	int32 TileMapY;

	real32 X; // position within a tilemap which may be out of bounds.
	real32 Y;
};


struct tile_map
{
	uint32 *Tiles;
};

struct world
{
	real32 TileSideInMeters;
	int32 TileSideInPixels;

	int32 CountX;
	int32 CountY;

	float UpperLeftX;
	float UpperLeftY;

	int32 TileMapCountX;
	int32 TileMapCountY;

	tile_map *TileMaps;
};

struct game_state
{
	int32 PlayerTileMapX;
	int32 PlayerTileMapY;

	real32 PlayerX;
	real32 PlayerY;
};

#endif // HANDMADE_H
