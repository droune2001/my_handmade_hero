
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline world_chunk*
GetWorldChunk( world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ, memory_arena *Arena = 0 )
{
	world_chunk *WorldChunk = 0;

	// coordinates inside a rectangle of the whole available space
	Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

	uint32 HashValue = 19 * ChunkX + 7 * ChunkY + 3 * ChunkZ;
	uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1); // mask to array size
	Assert( HashSlot < ArrayCount(World->ChunkHash) );

	world_chunk *Chunk = World->ChunkHash + HashSlot;
	do
	{
		if ((ChunkX == Chunk->ChunkX) &&
			(ChunkY == Chunk->ChunkY) &&
			(ChunkZ == Chunk->ChunkZ))
		{
			WorldChunk = Chunk;
			break;
		}

		// slot occupied by a valid (Chunk->ChunkX != TILE_CHUNK_UNINITIALIZED) chunk, but no link yet.
		if ( Arena && (Chunk->ChunkX != TILE_CHUNK_UNINITIALIZED) && (!Chunk->NextInHash) )
		{
			Chunk->NextInHash = PushStruct( Arena, world_chunk );
			Chunk = Chunk->NextInHash;
			Chunk->ChunkX = TILE_CHUNK_UNINITIALIZED;
		}

		// we are either on the newly allocated chunk in the LL or an originally empty one.
		// uninitialized in any case.
		if ( Arena && (Chunk->ChunkX == TILE_CHUNK_UNINITIALIZED))
		{
			uint32 TileCount = World->ChunkDim * World->ChunkDim;

			Chunk->ChunkX = ChunkX;
			Chunk->ChunkY = ChunkY;
			Chunk->ChunkZ = ChunkZ;

			Chunk->NextInHash = 0;
			WorldChunk = Chunk;
			break;
		}

		Chunk = Chunk->NextInHash;
	} while ( Chunk );

	return WorldChunk;
}

//internal tile_chunk_position
//GetChunkPositionFor(world *World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
//{
//	tile_chunk_position Result;
//
//	Result.TileChunkX = AbsTileX >> World->ChunkShift;
//	Result.TileChunkY = AbsTileY >> World->ChunkShift;
//	Result.TileChunkZ = AbsTileZ;
//	Result.RelTileX = AbsTileX & World->ChunkMask;
//	Result.RelTileY = AbsTileY & World->ChunkMask;
//
//	return Result;
//}

internal void
InitializeWorld(world *World, real32 TileSideInMeters)
{
	World->ChunkShift = 4; // 16 x 16 tile chunks
	World->ChunkMask = (1 << World->ChunkShift) - 1; // 0x0010 - 1 = 0x000F
	World->ChunkDim = (1 << World->ChunkShift); // 16
	World->TileSideInMeters = TileSideInMeters;

	for (uint32 TileChunkIndex = 0;
		TileChunkIndex < ArrayCount(World->ChunkHash);
		++TileChunkIndex)
	{
		// enough for our test of "uninitialized" chunk
		World->ChunkHash[TileChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
	}
}

//
//
//

inline void
RecanonicalizeCoord( world *World, int32 *Tile, real32 *TileRel )
{
	// overflow/underflow if TileRel goes < 0 or > TileSideInMeters
	// Gives -1, 0 or 1, mostly. Could be 2, -3...
	int32 Offset = RoundReal32ToInt32( *TileRel / World->TileSideInMeters );
	// for center based, we used round instead of floor

	// NOTE(nfauvet): World is toroidal. You exit on one side, your pop up on the other side.

	// go to next Tile, or previous, or dont move.
	*Tile += Offset;
	// Take the remainder of the modified TileRel.
	*TileRel -= (real32)Offset * World->TileSideInMeters;

	// Assert relative pos well within the bounds of a tile coordinates
	Assert( *TileRel > -0.5f * World->TileSideInMeters );
	Assert( *TileRel < 0.5f * World->TileSideInMeters );
}

// Takes a canonical which TileRelX and Y have been messed up with, 
// and RE-canonicalize it.
inline world_position
MapIntoTileSpace( world *World, world_position BasePos, v2 Offset )
{
	world_position Result = BasePos;
	Result.Offset_ += Offset;

	RecanonicalizeCoord( World, &Result.AbsTileX, &Result.Offset_.X );
	RecanonicalizeCoord( World, &Result.AbsTileY, &Result.Offset_.Y );

	return Result;
}

inline bool32
AreOnSameTile( world_position *A, world_position *B )
{
	bool32 Result = ( 
		( A->AbsTileX == B->AbsTileX ) &&
		( A->AbsTileY == B->AbsTileY ) &&
		( A->AbsTileZ == B->AbsTileZ ) );

	return Result;
}

world_difference Subtract( world *World, world_position *A, world_position *B )
{
	world_difference Result;

	v2 dTileXY = {  (real32)A->AbsTileX - (real32)B->AbsTileX,
					(real32)A->AbsTileY - (real32)B->AbsTileY };
	real32 dTileZ = 0;

	Result.dXY = World->TileSideInMeters * dTileXY + ( A->Offset_ - B->Offset_ );
	Result.dZ = 0;

	return Result;
}

inline world_position
CenteredTilePoint( uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	world_position Result = {};

	Result.AbsTileX = AbsTileX;
	Result.AbsTileY = AbsTileY;
	Result.AbsTileZ = AbsTileZ;
	
	return Result;
}
