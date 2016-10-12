#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"

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

#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world
{
	tile_map *TileMap;
};

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	uint32 *Pixels;
};

struct hero_bitmaps
{
	int32 AlignX;
	int32 AlignY;
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	tile_map_position CameraP;
	tile_map_position PlayerP;

	loaded_bitmap Backdrop;
	uint32 HeroFacingDirection;
	hero_bitmaps HeroBitmaps[4];
};

#endif // HANDMADE_H
