

inline void
RecanonicalizeCoord( tile_map *TileMap, uint32 *Tile, real32 *TileRel )
{
	// overflow/underflow if TileRel goes < 0 or > TileSideInMeters
	// Gives -1, 0 or 1, mostly. Could be 2, -3...
	int32 Offset = RoundReal32ToInt32( *TileRel / TileMap->TileSideInMeters );
	// for center based, we used round instead of floor

	// NOTE(nfauvet): TileMap is toroidal. You exit on one side, your pop up on the other side.

	// go to next Tile, or previous, or dont move.
	*Tile += Offset;
	// Take the remainder of the modified TileRel.
	*TileRel -= (real32)Offset * TileMap->TileSideInMeters;

	// Assert relative pos well within the bounds of a tile coordinates
	Assert( *TileRel >= -0.5f * TileMap->TileSideInMeters );
	Assert( *TileRel <= 0.5f * TileMap->TileSideInMeters );
}

// Takes a canonical which TileRelX and Y have been messed up with, 
// and RE-canonicalize it.
inline tile_map_position
RecanonicalizePosition( tile_map *TileMap, tile_map_position Pos )
{
	tile_map_position Result = Pos;

	RecanonicalizeCoord( TileMap, &Result.AbsTileX, &Result.TileRelX );
	RecanonicalizeCoord( TileMap, &Result.AbsTileY, &Result.TileRelY );

	return Result;
}

inline tile_chunk*
GetTileChunk( tile_map *TileMap, int32 TileChunkX, int32 TileChunkY )
{
	tile_chunk *TileChunk = 0;

	if ( ( TileChunkX >= 0 ) && ( TileChunkX < TileMap->TileChunkCountX ) &&
		( TileChunkY >= 0 ) && ( TileChunkY < TileMap->TileChunkCountY ) )
	{
		TileChunk = &TileMap->TileChunks[TileMap->TileChunkCountX * TileChunkY + TileChunkX];
	}
	return TileChunk;
}

inline uint32
GetTileValueUnchecked( tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY )
{
	Assert( TileChunk );
	Assert( TileX < TileMap->ChunkDim );
	Assert( TileY < TileMap->ChunkDim );

	uint32 TileChunkValue = TileChunk->Tiles[TileMap->ChunkDim * TileY + TileX];
	return TileChunkValue;
}

internal uint32
GetTileValue( tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY )
{
	uint32 TileChunkValue = 0;

	if ( TileChunk ) {
		TileChunkValue = GetTileValueUnchecked( TileMap, TileChunk, TestTileX, TestTileY );
	}

	return TileChunkValue;
}

internal tile_chunk_position GetChunkPositionFor( tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY )
{
	tile_chunk_position Result;

	Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
	Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
	Result.RelTileX = AbsTileX & TileMap->ChunkMask;
	Result.RelTileY = AbsTileY & TileMap->ChunkMask;

	return Result;
}

internal uint32
GetTileValue( tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY )
{
	tile_chunk_position ChunkPos = GetChunkPositionFor( TileMap, AbsTileX, AbsTileY );
	tile_chunk *TileChunk = GetTileChunk( TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY );
	uint32 TileChunkValue = GetTileValue( TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY );

	return TileChunkValue;
}

internal bool32
IsTileMapPointEmpty( tile_map *TileMap, tile_map_position Pos )
{
	uint32 TileChunkValue = GetTileValue( TileMap, Pos.AbsTileX, Pos.AbsTileY );
	bool32 Empty = ( TileChunkValue == 0 );

	return Empty;
}
