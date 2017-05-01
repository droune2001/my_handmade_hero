
internal sim_entity_hash *
GetHashFromStorageIndex( sim_region *SimRegion, uint32 StorageIndex )
{
	Assert(StorageIndex);

	sim_entity_hash *Result = nullptr;
	uint32 HashValue = StorageIndex;
	for (uint32 Offset = 0;
		Offset < ArrayCount(SimRegion->Hash);
		++Offset)
	{
		// NOTE(nfauvet): masking is a modulo if size is power of two
		sim_entity_hash *Entry = SimRegion->Hash +
			((HashValue + Offset) & (ArrayCount(SimRegion->Hash) - 1));

		// found
		if ((Entry->Index = 0) || (Entry->Index = StorageIndex))
		{
			Result = Entry;
			break;
		}
	}

	return Result;
}

internal void
MapStorageIndexToEntity( sim_region *SimRegion, uint32 StorageIndex, sim_entity *Entity )
{
	sim_entity_hash *Entry = GetHashFromStorageIndex( SimRegion, StorageIndex );
	Assert((Entry->Index == 0) || (Entry->Index == StorageIndex));
	Entry->Index = StorageIndex;
	Entry->Ptr = Entity;
}

inline sim_entity *
GetEntityByStorageIndeX( sim_region *SimRegion, uint32 StorageIndex )
{
	sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	sim_entity *Result = Entry->Ptr;
	return Result;
}


internal sim_entity *AddEntity(game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source);

inline void
LoadEntityReference(game_state *GameState, sim_region *SimRegion, entity_reference *Ref )
{
	if (Ref->Index)
	{
		sim_entity_hash *Entry = GetHashFromStorageIndex( SimRegion, Ref->Index );
		// entity_ref not in hash map yet
		if (Entry->Ptr == nullptr)
		{
			Entry->Index = Ref->Index;
			Ref->Ptr = AddEntity(GameState, SimRegion, Ref->Index, GetLowEntity(GameState, Ref->Index));
		}
		Ref->Ptr = Entry->Ptr;
	}
}

inline void
StoreEntityReference(entity_reference *Ref)
{
	if (Ref->Ptr != 0)
	{
		Ref->Index = Ref->Ptr->StorageIndex;
	}
}

internal sim_entity *
AddEntity(game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source)
{
	Assert(StorageIndex);
	sim_entity *Entity = nullptr;
	if ( SimRegion->EntityCount < SimRegion->MaxEntityCount )
	{
		Entity = SimRegion->Entities + SimRegion->EntityCount++;
		MapStorageIndexToEntity(SimRegion, StorageIndex, Entity);

		if (Source)
		{
			// TODO(nfauvet): this should be a decompress step and not just a copy.
			*Entity = Source->Sim;
			LoadEntityReference( GameState, SimRegion, &Entity->Sword );
		}

		Entity->StorageIndex = StorageIndex;
	}
	else 
	{
		InvalidCodePath;
	}

	return Entity;
}

inline v2
GetSimSpaceP( sim_region *SimRegion, low_entity *Stored )
{
	// NOTE(nfauvet): map the entity into camera space
	world_difference Diff = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin );
	v2 Result = Diff.dXY;

	return Result;
}

internal sim_entity *
AddEntity( game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source, v2 *SimP )
{
	sim_entity *Dest = AddEntity( GameState, SimRegion, StorageIndex, Source );
	if ( Dest )
	{
		if ( SimP )
		{
			Dest->P = *SimP;
		}
		else
		{
			Dest->P = GetSimSpaceP( SimRegion, Source );
		}
	}

	return Dest;
}

internal sim_region*
BeginSim( memory_arena *SimArena, game_state *GameState, world *World, world_position Origin, rectangle2 Bounds )
{
	sim_region *SimRegion = PushStruct( SimArena, sim_region );
	SimRegion->World = World;
	SimRegion->Origin = Origin;
	SimRegion->Bounds = Bounds;
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray( SimArena, SimRegion->MaxEntityCount, sim_entity );

	world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
	world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));
	for (int32 ChunkY = MinChunkP.ChunkY;
		ChunkY <= MaxChunkP.ChunkY;
		++ChunkY)
	{
		for (int32 ChunkX = MinChunkP.ChunkX;
			ChunkX <= MaxChunkP.ChunkX;
			++ChunkX)
		{
			world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, SimRegion->Origin.ChunkZ);
			if (Chunk)
			{
				for (world_entity_block *Block = &Chunk->FirstBlock;
					Block;
					Block = Block->Next)
				{
					for (uint32 EntityIndexIndex = 0;
						EntityIndexIndex < Block->EntityCount;
						++EntityIndexIndex)
					{
						uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexIndex];
						low_entity *Low = GameState->LowEntities + LowEntityIndex;
						v2 SimSpaceP = GetSimSpaceP( SimRegion, Low );
						if ( IsInRectangle( SimRegion->Bounds, SimSpaceP ) )
						{
							AddEntity( GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP );
						}
					}
				}
			}
		}
	}
}

internal void
EndSim( sim_region *SimRegion, game_state *GameState )
{
	sim_entity *Entity = SimRegion->Entities;
	for ( uint32 EntityIndex = 0;
		  EntityIndex < SimRegion->EntityCount;
		  ++EntityIndex, +Entity)
	{
		low_entity *Stored = GameState->LowEntities + Entity->StorageIndex;

		Stored->Sim = *Entity;
		StoreEntityReference(&Stored->Sim.Sword);

		world_position NewP = MapIntoChunkSpace(GameState->World, SimRegion->Origin, Entity->P);

		ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex,
							Stored, &Stored->P, &NewP);


		// if we just treated the camera following entity
		if ( Entity->StorageIndex == GameState->CameraFollowingEntityIndex )
		{
			world_position NewCameraP = GameState->CameraP;

			// Z of the camera is always the same as the player
			NewCameraP.ChunkZ = Stored->P.ChunkZ;

#if 0
			// Move camera from screen to screen depending camera space distance
			// between the entity and the camera.
			if (CameraFollowingEntity->P.X > (9.0f * World->TileSideInMeters)) {
				NewCameraP.AbsTileX += 17;
			}
			if (CameraFollowingEntity->P.X < (-9.0f * World->TileSideInMeters)) {
				NewCameraP.AbsTileX -= 17;
			}
			if (CameraFollowingEntity->P.Y >(5.0f * World->TileSideInMeters)) {
				NewCameraP.AbsTileY += 9;
			}
			if (CameraFollowingEntity->P.Y < (-5.0f * World->TileSideInMeters)) {
				NewCameraP.AbsTileY -= 9;
			}
#else
			NewCameraP = Stored->P;
#endif
		}
	}
}

internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, real32 *tMin, real32 MinY, real32 MaxY)
{
	bool32 Hit = false;

	real32 tEpsilon = 0.001f;
	if (PlayerDeltaX != 0.0f)
	{
		real32 tResult = (WallX - RelX) / PlayerDeltaX;
		real32 Y = RelY + tResult * PlayerDeltaY;
		if ((tResult >= 0.0f) && (*tMin > tResult))
		{
			if ((Y >= MinY) && (Y <= MaxY))
			{
				//*tMin = Maximum( tEpsilon, tResult - tEpsilon );
				*tMin = Maximum(0.0f, tResult - tEpsilon);
				Hit = true;
			}
		}
	}

	return Hit;
}

internal void
MoveEntity( sim_region *SimRegion, sim_entity *Entity, real32 dt, move_spec *MoveSpec, v2 ddP )
{
	world *World = SimRegion->World;

	if (MoveSpec->UnitMaxAccelVector)
	{
		real32 ddLengthSq = LengthSq(ddP);
		if (ddLengthSq > 1.0f)
		{
			ddP *= 1.0f / SquareRoot(ddLengthSq);
		}
	}

	ddP *= MoveSpec->Speed;

	// friction
	ddP += -MoveSpec->Drag * Entity->dP;

	// temporary position
	v2 OldPlayerP = Entity->P;
	v2 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dP * dt);
	Entity->dP = ddP * dt + Entity->dP;
	v2 NewPlayerP = OldPlayerP + PlayerDelta;

	for (uint32 Iteration = 0;
		Iteration < 4;
		++Iteration)
	{
		real32 tMin = 1.0f; // tRemaining;
		v2 WallNormal = {};
		sim_entity *HitEntity = nullptr;

		v2 DesiredPosition = Entity->P + PlayerDelta;

		if (Entity->Collides)
		{
			// iterate over all entities
			for (uint32 TestHighEntityIndex = 0;
				TestHighEntityIndex < SimRegion->EntityCount;
				++TestHighEntityIndex)
			{
				sim_entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;
				if ( Entity != TestEntity ) // dont collision test with self!!
				{
					if ( TestEntity->Collides )
					{
						// bbox with the two tested entities
						real32 DiameterW = TestEntity->Width + Entity->Width;
						real32 DiameterH = TestEntity->Height + Entity->Height;

						v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
						v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

						v2 Rel = Entity->P - TestEntity->P;

						if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = { -1,0 };
							HitEntity = TestEntity;
						}

						if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = { 1,0 };
							HitEntity = TestEntity;
						}

						if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = { 0,-1 };
							HitEntity = TestEntity;
						}

						if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = { 0,1 };
							HitEntity = TestEntity;
						}
					}
				}
			}
		}

		Entity->P += tMin * PlayerDelta; // hit position

		if ( HitEntity )
		{
			// commit movement
			Entity->dP = Entity->dP - Inner(Entity->dP, WallNormal) * WallNormal;
			PlayerDelta = DesiredPosition - Entity->P; // remaining delta after the hit position
			PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal; // along the wall

			// TODO(nfauvet): stairs
			//Entity->AbsTileZ += HitLow->dAbsTileZ;
		}
		else
		{
			break;
		}
	}

	//
	// FACING DIRECTION
	//

	if ((Entity->dP.X == 0.0f) && (Entity->dP.Y == 0.0f))
	{
		// NOTE(nfauvet): leave facing direction to whatever it was
	}
	else if (AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y))
	{
		if (Entity->dP.X > 0)
		{
			// RIGHT
			Entity->FacingDirection = 0;
		}
		else
		{
			// LEFT
			Entity->FacingDirection = 2;
		}
	}
	else
	{
		if (Entity->dP.Y > 0)
		{
			// UP
			Entity->FacingDirection = 1;
		}
		else
		{
			// DOWN
			Entity->FacingDirection = 3;
		}
	}
}

//
