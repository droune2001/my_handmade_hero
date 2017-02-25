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
	// Packed Tile Index
	// => virtual system into a tile store
	// => 24bits for chunks index (page)
	// => 8bits for tiles inside a chunk
	int32 AbsTileX;
	int32 AbsTileY;
	int32 AbsTileZ;

	// position inside a tile, relative to the center of the tile.
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

	world_entity_block FirstBlock;

	world_chunk *NextInHash;
};

struct world
{
	real32 TileSideInMeters;

	// TODO(nfauvet): ChunkHash should probably switch to pointers IF
	// tile entity blocks continue to be stored en masse in the world_chunk!
	// NOTE(nfauvet): At the moment, this must be a power of two.
	int32 ChunkShift;
	int32 ChunkMask;
	int32 ChunkDim;
	world_chunk ChunkHash[4096];
};

#endif // _HANDMADE_WORLD_H_
