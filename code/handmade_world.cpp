
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline bool32
IsCanonical( world *World, real32 TileRel )
{
	// Assert relative pos well within the bounds of a tile coordinates
	// TODO(nfauvet): Fix floating point math so this can be exact?
	bool32 Result = ((TileRel >= -0.5f * World->ChunkSideInMeters) &&
					(TileRel <= 0.5f * World->ChunkSideInMeters));
	return Result;
}

inline bool32
IsCanonical( world *World, v2 Offset )
{
	bool32 Result = (IsCanonical(World, Offset.X) && IsCanonical(World, Offset.Y));
	return Result;
}

inline bool32
AreInSameChunk( world *World, world_position *A, world_position *B )
{
	Assert( IsCanonical( World, A->Offset_ ) );
	Assert( IsCanonical( World, B->Offset_ ) );

	bool32 Result = (
		(A->ChunkX == B->ChunkX) &&
		(A->ChunkY == B->ChunkY) &&
		(A->ChunkZ == B->ChunkZ));

	return Result;
}

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

internal void
InitializeWorld(world *World, real32 TileSideInMeters)
{
	World->TileSideInMeters = TileSideInMeters;
	World->ChunkSideInMeters = (real32)TILES_PER_CHUNK * TileSideInMeters;
	World->FirstFree = 0;

	for (uint32 ChunkIndex = 0;
		ChunkIndex < ArrayCount(World->ChunkHash);
		++ChunkIndex)
	{
		// enough for our test of "uninitialized" chunk
		World->ChunkHash[ChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
		World->ChunkHash[ChunkIndex].FirstBlock.EntityCount = 0;
	}
}

inline void
RecanonicalizeCoord( world *World, int32 *Tile, real32 *TileRel )
{
	// overflow/underflow if TileRel goes < 0 or > TileSideInMeters
	// Gives -1, 0 or 1, mostly. Could be 2, -3...
	int32 Offset = RoundReal32ToInt32( *TileRel / World->ChunkSideInMeters );
	// for center based, we used round instead of floor

	// NOTE(nfauvet): world is NOT WRAPPING, so all coordinates are assumed
	// to be within safe margins.
	*Tile += Offset; // go to next Tile, or previous, or dont move.
	// Take the remainder of the modified TileRel.
	*TileRel -= (real32)Offset * World->ChunkSideInMeters;

	Assert( IsCanonical( World, *TileRel ) );
}

// Takes a canonical which TileRelX and Y have been messed up with, 
// and RE-canonicalize it.
inline world_position
MapIntoTileSpace( world *World, world_position BasePos, v2 Offset )
{
	world_position Result = BasePos;
	Result.Offset_ += Offset;

	RecanonicalizeCoord( World, &Result.ChunkX, &Result.Offset_.X );
	RecanonicalizeCoord( World, &Result.ChunkY, &Result.Offset_.Y );

	return Result;
}

inline world_position
ChunkPositionFromTilePosition( world* World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ )
{
	world_position Result = {};

	// TOTOD(nfauvet): move to 3D Z!!

	Result.ChunkX = AbsTileX / TILES_PER_CHUNK;
	Result.ChunkX = AbsTileY / TILES_PER_CHUNK;
	Result.ChunkZ = AbsTileZ / TILES_PER_CHUNK;

	Result.Offset_.X = (real32)(AbsTileX - (Result.ChunkX * TILES_PER_CHUNK)) * World->TileSideInMeters;
	Result.Offset_.Y = (real32)(AbsTileY - (Result.ChunkY * TILES_PER_CHUNK)) * World->TileSideInMeters;

	return Result;
}

world_difference Subtract( world *World, world_position *A, world_position *B )
{
	world_difference Result;

	v2 dTileXY = {  (real32)A->ChunkX - (real32)B->ChunkX,
					(real32)A->ChunkY - (real32)B->ChunkY };
	real32 dTileZ = (real32)A->ChunkZ - (real32)B->ChunkZ;

	Result.dXY = World->ChunkSideInMeters * dTileXY + ( A->Offset_ - B->Offset_ );
	Result.dZ = World->ChunkSideInMeters * dTileZ;

	return Result;
}

inline world_position
CenteredChunkPoint( uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ )
{
	world_position Result = {};

	Result.ChunkX = ChunkX;
	Result.ChunkY = ChunkY;
	Result.ChunkZ = ChunkZ;
	
	return Result;
}

inline void
ChangeEntityLocation( memory_arena *Arena, world* World, uint32 LowEntityIndex, 
					world_position *OldP, world_position *NewP )
{
	if ( OldP && AreInSameChunk( World, OldP, NewP ) )
	{
		// NOTE(nfauvet): Leave entity where it is.
	}
	else
	{
		if ( OldP )
		{
			// NOTE(nfauvet): Pull the entity out of its old entity block
			world_chunk *Chunk = GetWorldChunk( World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ, Arena );
			Assert( Chunk );
			if ( Chunk )
			{
				// find entity in linked list
				world_entity_block *FirstBlock = &Chunk->FirstBlock;
				for (world_entity_block *Block = FirstBlock;
					Block;
					Block = Block->Next)
				{
					// scan each block
					for ( uint32 Index = 0; Index < Block->EntityCount; ++Index )
					{
						if ( Block->LowEntityIndex[Index] == LowEntityIndex )
						{
							// insert last element of current block into the hole
							// of the Entity we are removing. Current block can be
							// the FirstBlock.
							Assert(FirstBlock->EntityCount > 0);
							Block->LowEntityIndex[Index] =
								FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];

							// if FirstBlock is now empty, remove it.
							if (FirstBlock->EntityCount == 0)
							{
								// Replace it with its next. Reclaim next block memory
								if (FirstBlock->Next)
								{
									world_entity_block *NextBlock = FirstBlock->Next;
									*FirstBlock = *NextBlock;

									// free block
									NextBlock->Next = World->FirstFree;
									World->FirstFree = NextBlock;
								}
							}

							Block = 0; // stop the outer for-loop
							break; // exit the inner for-loop
						}
					}
				}
			}
		}

		// NOTE(nfauvet): Inset the entity into its new entity block
		world_chunk *Chunk = GetWorldChunk( World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena );
		world_entity_block *Block = &Chunk->FirstBlock;
		if ( Block->EntityCount == ArrayCount(Block->LowEntityIndex))
		{
			// NOTE(nfauvet): we're out of room, get a new block!
			world_entity_block *OldBlock = World->FirstFree;
			if ( OldBlock )
			{
				World->FirstFree = OldBlock->Next;
			}
			else // no free cell to use, allocate one.
			{
				OldBlock = PushStruct( Arena, world_entity_block );
			}

			*OldBlock = *Block;
			Block->Next = OldBlock;
			Block->EntityCount = 0;
			// on a insere le nouveau block en tete, pour que le code d'apres se serve
			// toujours du premier block, qu'on en ait cree un nouveau ou pas.
		}

		Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
		Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
	}
}
