#ifndef _HANDMADE_TILE_H_
#define _HANDMADE_TILE_H_

struct tile_map_difference 
{
	v2 dXY;
	real32 dZ;
};

// world position
struct tile_map_position
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

struct tile_chunk_position
{
	int32 TileChunkX;
	int32 TileChunkY;
	int32 TileChunkZ;

	int32 RelTileX;
	int32 RelTileY;
};

struct tile_chunk
{
	int32 TileChunkX;
	int32 TileChunkY;
	int32 TileChunkZ;

	uint32 *Tiles;

	tile_chunk *NextInHash;
};

struct tile_map
{
	uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;

	tile_chunk TileChunkHash[4096];
};

#endif // _HANDMADE_TILE_H_
