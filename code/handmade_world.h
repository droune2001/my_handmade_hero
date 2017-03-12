#ifndef _HANDMADE_WORLD_H_
#define _HANDMADE_WORLD_H_

struct world_difference 
{
	v2 dXY;
	real32 dZ;
};

// world position
struct world_position
{
	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

	// position inside a CHUNK, relative to its center.
	v2 Offset_;
};

struct world_entity_block
{
	uint32 EntityCount;
	uint32 LowEntityIndex[16];
	world_entity_block *Next;
};

struct world_chunk
{
	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;
	
	// TODO(nfauvet): profile this and deternie if a pointer would be better
	world_entity_block FirstBlock;

	world_chunk *NextInHash;
};

struct world
{
	real32 TileSideInMeters;
	real32 ChunkSideInMeters;

	world_entity_block *FirstFree;

	// TODO(nfauvet): ChunkHash should probably switch to pointers IF
	// tile entity blocks continue to be stored en masse in the world_chunk!
	// NOTE(nfauvet): At the moment, this must be a power of two.
	world_chunk ChunkHash[4096];
};

#endif // _HANDMADE_WORLD_H_
