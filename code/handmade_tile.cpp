
inline tile_chunk*
GetTileChunk( tile_map *TileMap, uint32 TileChunkX, uint32 TileChunkY, uint32 TileChunkZ )
{
	tile_chunk *TileChunk = 0;

	if (( TileChunkX >= 0 ) && ( TileChunkX < TileMap->TileChunkCountX ) &&
		( TileChunkY >= 0 ) && ( TileChunkY < TileMap->TileChunkCountY ) &&
		( TileChunkZ >= 0 ) && ( TileChunkZ < TileMap->TileChunkCountZ ) )
	{
		TileChunk = &TileMap->TileChunks[TileMap->TileChunkCountX * TileMap->TileChunkCountY * TileChunkZ +
										 TileMap->TileChunkCountX * TileChunkY +
										 TileChunkX];
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

inline void
SetTileValueUnchecked( tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY, uint32 TileValue )
{
	Assert( TileChunk );
	Assert( TileX < TileMap->ChunkDim );
	Assert( TileY < TileMap->ChunkDim );

	TileChunk->Tiles[TileMap->ChunkDim * TileY + TileX] = TileValue;
}



internal void
SetTileValue( tile_map *TileMap, tile_chunk *TileChunk, 
				uint32 TestTileX, uint32 TestTileY,
				uint32 TileValue )
{
	if ( TileChunk && TileChunk->Tiles ) {
		SetTileValueUnchecked( TileMap, TileChunk, TestTileX, TestTileY, TileValue );
	}
}

internal tile_chunk_position GetChunkPositionFor( tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	tile_chunk_position Result;

	Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
	Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
	Result.TileChunkZ = AbsTileZ;
	Result.RelTileX = AbsTileX & TileMap->ChunkMask;
	Result.RelTileY = AbsTileY & TileMap->ChunkMask;

	return Result;
}

internal uint32
GetTileValue( tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY )
{
	uint32 TileChunkValue = 0;

	if ( TileChunk && TileChunk->Tiles ) {
		TileChunkValue = GetTileValueUnchecked( TileMap, TileChunk, TestTileX, TestTileY );
	}

	return TileChunkValue;
}

internal uint32
GetTileValue( tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	tile_chunk_position ChunkPos = GetChunkPositionFor( TileMap, AbsTileX, AbsTileY, AbsTileZ );
	tile_chunk *TileChunk = GetTileChunk( TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ );
	uint32 TileChunkValue = GetTileValue( TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY );

	return TileChunkValue;
}

internal uint32
GetTileValue( tile_map *TileMap, tile_map_position Pos )
{
	uint32 TileChunkValue = GetTileValue( TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ );
	return TileChunkValue;
}

internal bool32
IsTileValueEmpty( uint32 TileValue )
{
	bool32 Empty = ( ( TileValue == 1 ) || // empty space
		( TileValue == 3 ) || // up stair
		( TileValue == 4 ) ); // down stair
	return Empty;
}

internal bool32
IsTileMapPointEmpty( tile_map *TileMap, tile_map_position Pos )
{
	uint32 TileChunkValue = GetTileValue( TileMap, Pos );
	bool32 Empty = IsTileValueEmpty( TileChunkValue );
	return Empty;
}

internal void
SetTileValue( memory_arena *Arena, tile_map *TileMap, 
			  uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
			  uint32 TileValue )
{
	tile_chunk_position ChunkPos = GetChunkPositionFor( TileMap, AbsTileX, AbsTileY, AbsTileZ );
	tile_chunk *TileChunk = GetTileChunk( TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ );

	// allocate on demand
	Assert( TileChunk );
	if ( !TileChunk->Tiles ) {
		uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
		TileChunk->Tiles = PushArray( Arena, TileCount, uint32 );
		// init all allocated tiles to 0
		for ( uint32 TileIndex = 0; TileIndex < TileCount; ++TileIndex ) {
			TileChunk->Tiles[TileIndex] = 1;
		}
	}

	SetTileValue( TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue );
}


//
//
//

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

	RecanonicalizeCoord( TileMap, &Result.AbsTileX, &Result.Offset.X );
	RecanonicalizeCoord( TileMap, &Result.AbsTileY, &Result.Offset.Y );

	return Result;
}

inline bool32
AreOnSameTile( tile_map_position *A, tile_map_position *B )
{
	bool32 Result = ( 
		( A->AbsTileX == B->AbsTileX ) &&
		( A->AbsTileY == B->AbsTileY ) &&
		( A->AbsTileZ == B->AbsTileZ ) );

	return Result;
}

tile_map_difference Substract( tile_map *TileMap, tile_map_position *A, tile_map_position *B )
{
	tile_map_difference Result;

	v2 dTileXY = {  (real32)A->AbsTileX - (real32)B->AbsTileX,
					(real32)A->AbsTileY - (real32)B->AbsTileY };
	real32 dTileZ = 0;

	Result.dXY = TileMap->TileSideInMeters * dTileXY + ( A->Offset - B->Offset );
	Result.dZ = 0;

	return Result;
}

inline tile_map_position
CenteredTilePoint( uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	tile_map_position Result = {};

	Result.AbsTileX = AbsTileX;
	Result.AbsTileY = AbsTileY;
	Result.AbsTileZ = AbsTileZ;
	
	return Result;
}
