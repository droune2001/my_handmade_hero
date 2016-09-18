#ifndef _HANDMADE_TILE_H_
#define _HANDMADE_TILE_H_

// world position
struct tile_map_position
{
	// Packed Tile Index
	// => virtual system into a tile store
	// => 24bits for chunks index (page)
	// => 8bits for tiles inside a chunk
	uint32 AbsTileX;
	uint32 AbsTileY;

	// TODO(nfauvet): rename "Offset"
	real32 TileRelX; // position inside a tile, relative to top left corner, in meters
	real32 TileRelY;
};

struct tile_chunk_position
{
	uint32 TileChunkX;
	uint32 TileChunkY;

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
	int32 TileSideInPixels;
	real32 MetersToPixels;

	int32 TileChunkCountX;
	int32 TileChunkCountY;

	tile_chunk *TileChunks;
};


#endif // _HANDMADE_TILE_H_
