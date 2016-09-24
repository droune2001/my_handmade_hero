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

struct memory_arena
{
	memory_index Size;
	uint8 *Base;
	memory_index Used;
};

internal void
InitializeArena( memory_arena *Arena, memory_index Size, uint8 *Base )
{
	Arena->Size = Size;
	Arena->Base = Base;
	Arena->Used = 0;
}

#define PushStruct(Arena, type) (type*)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type*)PushSize_(Arena, (Count)*sizeof(type))

internal void*
PushSize_( memory_arena *Arena, memory_index Size )
{
	Assert( ( Arena->Used + Size ) <= Arena->Size );
	// pointer to the beginning of the free space of the arena
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;
	return Result;
}

#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world
{
	tile_map *TileMap;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;
	tile_map_position PlayerP;

	uint32 * PixelPointer;
};

#endif // HANDMADE_H
