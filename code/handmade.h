#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"

#define Minimum(A, B) ((A<B)?(A):(B))
#define Maximum(A, B) ((A>B)?(A):(B))

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
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	uint32 *Pixels;
};

struct hero_bitmaps
{
	v2 Align;
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
};

enum entity_type
{
	EntityType_Null,
	EntityType_Hero,
	EntityType_Wall,
	EntityType_Familiar,
	EntityType_Monstar,
	EntityType_Sword
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	uint8 Flags;
	uint8 FilledAmount;
};

struct low_entity
{
	entity_type Type;

	world_position P;
	v2 dP; // velocity (derivative of P)
	real32 Width, Height;

	uint32 FacingDirection;
	real32 tBob;

	bool32 Collides;
	int32 dAbsTileZ; // for "stairs"

	uint32 HighEntityIndex;

	uint32 HitPointMax;
	hit_point HitPoint[16];

	uint32 SwordLowIndex;
	real32 DistanceRemaining;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	// TODO(nfauvet): should we allow split screen??
	uint32 CameraFollowingEntityIndex;
	world_position CameraP;

	uint32 PlayerIndexForController[ArrayCount( ( (game_input*)0 )->Controllers )];

	// TODO(nfauvet):  change name to stored_entities
	uint32 LowEntityCount;
	low_entity LowEntities[100000];
	
	loaded_bitmap Backdrop;
	loaded_bitmap Shadow;
	hero_bitmaps HeroBitmaps[4];

	loaded_bitmap Tree;
	loaded_bitmap Sword;

	real32 MetersToPixels;
};

struct entity_visible_piece
{
	loaded_bitmap *Bitmap;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;

	real32 R, G, B, A;
	v2 Dim;
};

struct entity_visible_piece_group
{
	game_state *GameState;
	uint32 PieceCount;
	entity_visible_piece Pieces[32];
};

#endif // HANDMADE_H
