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

struct tile_chunk_position 
{
	uint32 TileChunkX;
	uint32 TileChunkY;

	uint32 RelTileX;
	uint32 RelTileY;
};

struct world_position
{
	// TODO(nfauvet): pack the TileMapX/Y and TileX/Y into one uint
	// => virtual system into a tile store
	// => 28bits for chunks index (page)
	// => 4bits for chunks inside index
	uint32 AbsTileX;
	uint32 AbsTileY;

	// TODO(nfauvet): Center coordinates?
	// TODO(nfauvet): rename "Offset"
	real32 TileRelX; // position inside a tile, relative to top left corner, in meters
	real32 TileRelY;
};

struct tile_chunk
{
	uint32 *Tiles;
};

struct world
{
	uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;
	int32 TileSideInPixels;
	real32 MetersToPixels;

	int32 TileChunkCountX;
	int32 TileChunkCountY;

	tile_chunk *TileChunks;
};

struct game_state
{
	world_position PlayerP;
#if 0
	int32 PlayerTileMapX;
	int32 PlayerTileMapY;

	real32 PlayerX;
	real32 PlayerY;
#endif
};

#endif // HANDMADE_H
