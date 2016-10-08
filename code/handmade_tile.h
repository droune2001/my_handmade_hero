#ifndef _HANDMADE_TILE_H_
#define _HANDMADE_TILE_H_

struct tile_map_difference 
{
	real32 dX;
	real32 dY;
	real32 dZ;
};

// world position
struct tile_map_position
{
	// Packed Tile Index
	// => virtual system into a tile store
	// => 24bits for chunks index (page)
	// => 8bits for tiles inside a chunk
	uint32 AbsTileX;
	uint32 AbsTileY;
	uint32 AbsTileZ;

	// position inside a tile, relative to the center of the tile.
	real32 OffsetX; 
	real32 OffsetY;
};

struct tile_chunk_position
{
	uint32 TileChunkX;
	uint32 TileChunkY;
	uint32 TileChunkZ;

	uint32 RelTileX;
	uint32 RelTileY;
};

struct tile_chunk
{
	uint32 *Tiles;
};

struct tile_map
{
	uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;

	uint32 TileChunkCountX;
	uint32 TileChunkCountY;
	uint32 TileChunkCountZ;

	tile_chunk *TileChunks;
};


#endif // _HANDMADE_TILE_H_
