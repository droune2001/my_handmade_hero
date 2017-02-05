#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

internal void
GameOutputSound( game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz )
{
	local_persist real32 tSine;
	int16 ToneVolume = 1000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;
	for ( int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex ) {
#if 0
		real32 SineValue = sinf( GameState->tSine );
		int16 SampleValue = (int16)(SineValue * ToneVolume ); // scale sin
#else
		int16 SampleValue = 0; // be quiet!
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
#if 0
		GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;

		if ( GameState->tSine > 2.0f * Pi32 ) {
			GameState->tSine -= 2.0f * Pi32;
		}
#endif
	}
}

internal void
DrawRectangle(	game_offscreen_buffer *Buffer, 
				v2 vMin, v2 vMax,
				real32 R, real32 G, real32 B )
{	
	int32 MinX = RoundReal32ToInt32( vMin.X );
	int32 MinY = RoundReal32ToInt32( vMin.Y );
	int32 MaxX = RoundReal32ToInt32( vMax.X );
	int32 MaxY = RoundReal32ToInt32( vMax.Y );

	if( MinX < 0 ) MinX = 0;
	if( MinY < 0 ) MinY = 0;
	if ( MaxX > Buffer->Width ) MaxX = Buffer->Width;
	if ( MaxY > Buffer->Height ) MaxY = Buffer->Height;

	// BIT PATTERN: 0x AA RR GG BB
	uint32 Color =	(RoundReal32ToUInt32( R * 255.0f ) << 16) |
					(RoundReal32ToUInt32( G * 255.0f ) << 8 ) |
					 RoundReal32ToUInt32( B * 255.0f );

	// Starting pointer. Top-left of rectangle
	uint8 *Row = (uint8*)Buffer->Memory
		+ MinY * Buffer->Pitch
		+ MinX * Buffer->BytesPerPixel;

	for ( int Y = MinY; Y < MaxY; ++Y ) {	
		uint32 *Pixel = (uint32*)Row;
		for ( int X = MinX; X < MaxX; ++X ) {
			*Pixel++ = Color;
		}
		Row += Buffer->Pitch;
	}
}

internal void
DrawBitmap( game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, 
			real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0,
			real32 CAlpha = 1.0f)
{
	int32 MinX = RoundReal32ToInt32( RealX - AlignX );
	int32 MinY = RoundReal32ToInt32( RealY - AlignY );
	int32 MaxX = MinX + Bitmap->Width;
	int32 MaxY = MinY + Bitmap->Height;

	int32 SourceOffsetX = 0;
	if ( MinX < 0 ) {
		SourceOffsetX = -MinX;
		MinX = 0;
	}
	int32 SourceOffsetY = 0;
	if ( MinY < 0 ) {
		SourceOffsetY = -MinY;
		MinY = 0;
	}
	if ( MaxX > Buffer->Width ) MaxX = Buffer->Width;
	if ( MaxY > Buffer->Height ) MaxY = Buffer->Height;

	uint32 *SourceRow = Bitmap->Pixels + Bitmap->Width * ( Bitmap->Height - 1 );
	SourceRow += -Bitmap->Width * SourceOffsetY + SourceOffsetX; // top and left clipping
	uint8 *DestRow = (uint8*)Buffer->Memory + 
					MinY * Buffer->Pitch +
					MinX * Buffer->BytesPerPixel;

	for ( int32 Y = MinY; Y < MaxY; ++Y )
	{
		uint32 *Source = SourceRow;
		uint32 *Dest = (uint32*)DestRow;
		for ( int32 X = MinX; X < MaxX; ++X )
		{
			real32 A = (real32)( ( *Source >> 24 ) & 0xFF ) / 255.0f;
			A *= CAlpha;

			real32 SR = (real32)( ( *Source >> 16 ) & 0xFF );
			real32 SG = (real32)( ( *Source >> 8 ) & 0xFF );
			real32 SB = (real32)( ( *Source >> 0 ) & 0xFF );

			real32 DR = (real32)( ( *Dest >> 16 ) & 0xFF );
			real32 DG = (real32)( ( *Dest >> 8 ) & 0xFF );
			real32 DB = (real32)( ( *Dest >> 0 ) & 0xFF );

			real32 R = ( 1.0f - A ) * DR + A * SR;
			real32 G = ( 1.0f - A ) * DG + A * SG;
			real32 B = ( 1.0f - A ) * DB + A * SB;

			// truncate
			*Dest = (((uint32)( R + 0.5f ) << 16 ) |
					 ((uint32)( G + 0.5f ) << 8  ) |
					 ((uint32)( B + 0.5f )) );
			
			++Dest;
			++Source;
		}

		SourceRow -= Bitmap->Width; // bottom to top
		DestRow += Buffer->Pitch;
	}

#if 0
	// Clear screen
	DrawRectangle( Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f );
#endif
}

#pragma pack( push, 1 )
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 R1;
	uint16 R2;
	uint32 Offset;

	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitPerPixel;

	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorzResolution;
	int32 VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;

	uint32 RedMask;
	uint32 GreenMask;
	uint32 BlueMask;
};
#pragma pack( pop )

internal loaded_bitmap
DEBUGLoadBMP( thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *FileName )
{
	loaded_bitmap Result = {};

	// On my PC photoshop 0xAA RR GG BB
	// so in memory BGRA

	debug_read_file_result ReadResult = ReadEntireFile( Thread, FileName );
	if ( ReadResult.ContentsSize != 0 )
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixels = (uint32 *)( (uint8 *)ReadResult.Contents + Header->Offset );
		Result.Pixels = Pixels;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		Assert( Header->Compression == 3 );

		uint32 RedMask = Header->RedMask;
		uint32 GreenMask = Header->GreenMask;
		uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~( RedMask | GreenMask | BlueMask );

		bit_scan_result RedScan = FindLeastSignificantSetBit( RedMask );
		bit_scan_result GreenScan = FindLeastSignificantSetBit( GreenMask );
		bit_scan_result BlueScan = FindLeastSignificantSetBit( BlueMask );
		bit_scan_result AlphaScan = FindLeastSignificantSetBit( AlphaMask );

		Assert( RedScan.Found );
		Assert( GreenScan.Found );
		Assert( BlueScan.Found );
		Assert( AlphaScan.Found );

		int32 RedShift = 16 - (int32)RedScan.Index;
		int32 GreenShift = 8 - (int32)GreenScan.Index;
		int32 BlueShift = 0 - (int32)BlueScan.Index;
		int32 AlphaShift = 24 - (int32)AlphaScan.Index;

		// We want 0xARGB
		uint32 *SourceDest = Pixels;
		for ( int32 Y = 0; Y < Header->Height; ++Y )
		{
			for ( int32 X = 0; X < Header->Width; ++X )
			{
				uint32 C = *SourceDest;
#if 0
				*SourceDest++ = ((( C >> AlphaShift.Index ) & 0xFF ) << 24 ) |
								((( C >> RedShift.Index ) & 0xFF ) << 16 ) |
								((( C >> GreenShift.Index ) & 0xFF ) << 8 ) |
								((( C >> BlueShift.Index ) & 0xFF ) );
#else
				*SourceDest++ =(RotateLeft( C & RedMask,	RedShift	) |
								RotateLeft( C & GreenMask,	GreenShift	) |
								RotateLeft( C & BlueMask,	BlueShift	) |
								RotateLeft( C & AlphaMask,	AlphaShift	));
#endif
			}
		}

	}

	return Result;
}

internal void
ChangeEntityResidence( game_state *GameState, uint32 EntityIndex, entity_residence Residence )
{
	// TODO(nfauvet): implement this
	if (Residence == EntityResidence_High)
	{
		if (GameState->EntityResidence[EntityIndex] != EntityResidence_High)
		{
			high_entity *HighEntity = &GameState->HighEntities[EntityIndex];
			dormant_entity *DormantEntity = &GameState->DormantEntities[EntityIndex];
			tile_map_difference Diff = Subtract(GameState->World->TileMap, &DormantEntity->P, &GameState->CameraP);
			HighEntity->P = Diff.dXY;
			HighEntity->dP = V2(0, 0);
			HighEntity->AbsTileZ = DormantEntity->P.AbsTileZ;
			HighEntity->FacingDirection = 0;
		}
	}

	GameState->EntityResidence[EntityIndex] = Residence;
}

inline entity
GetEntity(game_state *GameState, entity_residence Residence, uint32 Index)
{
	entity Entity = {};

	if ((Index > 0) && (Index < GameState->EntityCount)) // <= ??
	{
		if (GameState->EntityResidence[Index] < Residence)
		{
			ChangeEntityResidence(GameState, Index, Residence);
			Assert(GameState->EntityResidence[Index] >= Residence);
		}

		Entity.Residence = Residence;
		Entity.Dormant = &GameState->DormantEntities[Index];
		Entity.Low = &GameState->LowEntities[Index];
		Entity.High = &GameState->HighEntities[Index];
	}

	return Entity;
}

internal uint32
AddEntity( game_state *GameState, entity_type Type )
{
	uint32 EntityIndex = GameState->EntityCount++;

	Assert(GameState->EntityCount < ArrayCount(GameState->DormantEntities));
	Assert(GameState->EntityCount < ArrayCount(GameState->LowEntities));
	Assert(GameState->EntityCount < ArrayCount(GameState->HighEntities));

	GameState->EntityResidence[EntityIndex] = EntityResidence_Dormant;
	GameState->DormantEntities[EntityIndex] = {};
	GameState->LowEntities[EntityIndex] = {};
	GameState->HighEntities[EntityIndex] = {};
	GameState->DormantEntities[EntityIndex].Type = Type;

	return EntityIndex;
}

internal uint32
AddPlayer( game_state *GameState )
{
	uint32 EntityIndex = AddEntity( GameState, EntityType_Hero );
	entity Entity = GetEntity( GameState, EntityResidence_Dormant, EntityIndex );

	Entity.Dormant->P.AbsTileX = 2;
	Entity.Dormant->P.AbsTileY = 3;
	Entity.Dormant->P.Offset_.X = 0.0f;
	Entity.Dormant->P.Offset_.Y = 0.0f;
	Entity.Dormant->Height = 0.5f; // 1.4f;
	Entity.Dormant->Width = 1.0f; // 0.75f * Entity->Height;
	Entity.Dormant->Collides = true;

	ChangeEntityResidence( GameState, EntityIndex, EntityResidence_High );

	// if first entity, camera will follow it.
	if ( EntityResidence_Nonexistent == GetEntity( GameState, EntityResidence_Dormant, GameState->CameraFollowingEntityIndex ).Residence )
	{
		GameState->CameraFollowingEntityIndex = EntityIndex;
	}

	return EntityIndex;
}

internal uint32
AddWall( game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	uint32 EntityIndex = AddEntity( GameState, EntityType_Wall );
	entity Entity = GetEntity(GameState, EntityResidence_Dormant, EntityIndex);

	Entity.Dormant->P.AbsTileX = AbsTileX;
	Entity.Dormant->P.AbsTileY = AbsTileY;
	Entity.Dormant->P.AbsTileZ = AbsTileZ;
	Entity.Dormant->Height = GameState->World->TileMap->TileSideInMeters;
	Entity.Dormant->Width = Entity.Dormant->Height;
	Entity.Dormant->Collides = true;

	return EntityIndex;
}

internal bool32
TestWall( real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, real32 *tMin, real32 MinY, real32 MaxY )
{
	bool32 Hit = false;

	real32 tEpsilon = 0.001f;
	if ( PlayerDeltaX != 0.0f )
	{
		real32 tResult = ( WallX - RelX ) / PlayerDeltaX;
		real32 Y = RelY + tResult * PlayerDeltaY;
		if ( ( tResult >= 0.0f ) && ( *tMin > tResult ) )
		{
			if ( ( Y >= MinY ) && ( Y <= MaxY ) )
			{
				//*tMin = Maximum( tEpsilon, tResult - tEpsilon );
				*tMin = Maximum( 0.0f, tResult - tEpsilon );
				Hit = true;
			}
		}
	}

	return Hit;
}

internal void 
MovePlayer( game_state *GameState, entity Entity, real32 dt, v2 ddP )
{
	tile_map *TileMap = GameState->World->TileMap;

	real32 ddLengthSq = LengthSq( ddP );
	if ( ddLengthSq > 1.0f )
	{
		ddP *= 1.0f / SquareRoot( ddLengthSq );
	}

	real32 Acceleration = 50.0f; // m/s*s
	//if ( Controller->ActionUp.EndedDown ){
	//	Acceleration = 30.0f;
	//}
	ddP *= Acceleration;

	// friction
	ddP += -8.0f * Entity.High->dP;

	// temporary position
	v2 OldPlayerP = Entity.High->P;
	v2 PlayerDelta = ( 0.5f * ddP * Square( dt ) + Entity.High->dP * dt );
	Entity.High->dP = ddP * dt + Entity.High->dP;
	v2 NewPlayerP = OldPlayerP + PlayerDelta;

	/*
	// find the rect encompassing the previous_to_new positions
	uint32 MinTileX = Minimum( OldPlayerP.AbsTileX, NewPlayerP.AbsTileX );
	uint32 MinTileY = Minimum( OldPlayerP.AbsTileY, NewPlayerP.AbsTileY );
	uint32 MaxTileX = Maximum( OldPlayerP.AbsTileX, NewPlayerP.AbsTileX );
	uint32 MaxTileY = Maximum( OldPlayerP.AbsTileY, NewPlayerP.AbsTileY );

	uint32 EntityTileWidth = CeilReal32ToInt32( Entity->Width / TileMap->TileSideInMeters );
	uint32 EntityTileHeight = CeilReal32ToInt32( Entity->Height / TileMap->TileSideInMeters );

	// expand by entity width "in tiles"
	MinTileX -= EntityTileWidth;
	MinTileY -= EntityTileHeight;
	MaxTileX += EntityTileWidth;
	MaxTileY += EntityTileHeight;

	// assert against wrapping
	Assert((MaxTileX - MinTileX) < 32);
	Assert((MaxTileY - MinTileY) < 32);

	uint32 AbsTileZ = Entity->P.AbsTileZ;
	*/

	for(uint32 Iteration = 0; 
		Iteration < 4; 
		++Iteration )
	{
		real32 tMin = 1.0f; // tRemaining;
		v2 WallNormal = {};
		uint32 HitEntityIndex = 0;

		v2 DesiredPosition = Entity.High->P + PlayerDelta;

		// iterate over all entities
		for (uint32 EntityIndex = 1;
			EntityIndex < GameState->EntityCount;
			++EntityIndex)
		{
			entity TestEntity = GetEntity(GameState, EntityResidence_High, EntityIndex);
			if (TestEntity.High != Entity.High) // dont collision test with self!!
			{
				if (TestEntity.Dormant->Collides)
				{
					// bbox with the two tested entities
					real32 DiameterW = TestEntity.Dormant->Width + Entity.Dormant->Width;
					real32 DiameterH = TestEntity.Dormant->Height + Entity.Dormant->Height;
					v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
					v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

					v2 Rel = Entity.High->P - TestEntity.High->P;

					if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = { -1,0 };
						HitEntityIndex = EntityIndex;
					}

					if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = { 1,0 };
						HitEntityIndex = EntityIndex;
					}

					if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = { 0,-1 };
						HitEntityIndex = EntityIndex;
					}

					if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = { 0,1 };
						HitEntityIndex = EntityIndex;
					}
				}
			}
		}		

		Entity.High->P += tMin * PlayerDelta; // hit position

		if (HitEntityIndex)
		{
			// commit movement
			Entity.High->dP = Entity.High->dP - Inner(Entity.High->dP, WallNormal) * WallNormal;
			PlayerDelta = DesiredPosition - Entity.High->P; // remaining delta after the hit position
			PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal; // along the wall
			
			entity HitEntity = GetEntity(GameState, EntityResidence_Dormant, HitEntityIndex);
			Entity.High->AbsTileZ += HitEntity.Dormant->dAbsTileZ;
		}
		else 
		{
			break;
		}

		// after the float position computation, map back into tile space
		Entity.Dormant->P = MapIntoTileSpace(GameState->World->TileMap, GameState->CameraP, Entity.High->P);
	}

	//
	// FACING DIRECTION
	//

	if ( ( Entity.High->dP.X == 0.0f ) && ( Entity.High->dP.Y == 0.0f ) )
	{
		// NOTE(nfauvet): leave facing direction to whatever it was
	}
	else if ( AbsoluteValue( Entity.High->dP.X ) > AbsoluteValue( Entity.High->dP.Y ) ) 
	{
		if ( Entity.High->dP.X > 0 ) 
		{
			// RIGHT
			Entity.High->FacingDirection = 0;
		}
		else 
		{
			// LEFT
			Entity.High->FacingDirection = 2;
		}
	}
	else
	{
		if ( Entity.High->dP.Y > 0 ) 
		{
			// UP
			Entity.High->FacingDirection = 1;
		}
		else 
		{
			// DOWN
			Entity.High->FacingDirection = 3;
		}
	}
}

internal void
SetCamera( game_state *GameState, tile_map_position NewCameraP )
{
	tile_map *TileMap = GameState->World->TileMap;
	tile_map_difference dCameraP = Subtract( TileMap, &NewCameraP, &GameState->CameraP );
	GameState->CameraP = NewCameraP;
	v2 EntityOffsetForFrame = -dCameraP.dXY;

	// 3 x 3 screens around the camera
	uint32 TileSpanX = 17 * 3;
	uint32 TileSpanY = 9 * 3;
	rectangle2 CameraBounds = RectCenterDim( V2(0,0), 
								TileMap->TileSideInMeters * V2((real32)TileSpanX,(real32)TileSpanY));

	// kick entities too far into low residency
	for (uint32 EntityIndex = 1;
		EntityIndex < ArrayCount(GameState->HighEntities);
		++EntityIndex)
	{
		if ( GameState->EntityResidence[EntityIndex] == EntityResidence_High )
		{
			high_entity *High = GameState->HighEntities + EntityIndex;
			High->P += EntityOffsetForFrame;

			if (!IsInRectangle(CameraBounds, High->P))
			{
				ChangeEntityResidence(GameState, EntityIndex, EntityResidence_Dormant);
			}
		}
	}

	// move close entities into high
	uint32 MinTileX = NewCameraP.AbsTileX - TileSpanX/2;
	uint32 MaxTileX = NewCameraP.AbsTileX + TileSpanX/2;
	uint32 MinTileY = NewCameraP.AbsTileY - TileSpanY/2;
	uint32 MaxTileY = NewCameraP.AbsTileY + TileSpanY/2;
	for (uint32 EntityIndex = 1;
		EntityIndex < ArrayCount(GameState->DormantEntities);
		++EntityIndex)
	{
		if (GameState->EntityResidence[EntityIndex] == EntityResidence_Dormant)
		{
			dormant_entity *Dormant = GameState->DormantEntities + EntityIndex;
			if ((Dormant->P.AbsTileZ == NewCameraP.AbsTileZ) &&
				(Dormant->P.AbsTileX >= MinTileX) &&
				(Dormant->P.AbsTileX <= MaxTileX) &&
				(Dormant->P.AbsTileY >= MinTileY) &&
				(Dormant->P.AbsTileY <= MaxTileY))
			{
				ChangeEntityResidence(GameState, EntityIndex, EntityResidence_High);
			}
		}
	}
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( ( &Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0] ) == ArrayCount( Input->Controllers[0].Buttons ) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );

	// game_state occupies the beginning of the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	if ( !Memory->IsInitialized )
	{
		// NOTE(nfauvet): reserve entity slot 0 for the null entity
		AddEntity( GameState, EntityType_Null );

		GameState->Backdrop = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp" );
		GameState->Shadow = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");

		hero_bitmaps *Bitmap = &GameState->HeroBitmaps[0];

		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp" );
		++Bitmap;

		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp" );
		++Bitmap;

		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp" );
		++Bitmap;

		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp" );
		++Bitmap;

		// WorldArena comes just after the game_state chunk.
		// Uses all the memory left.
		InitializeArena( &GameState->WorldArena, Memory->PermanentStorageSize - sizeof( game_state ),
			(uint8*)Memory->PermanentStorage + sizeof( game_state ) );

		GameState->World = PushStruct( &GameState->WorldArena, world );
		world *World = GameState->World;
		World->TileMap = PushStruct( &GameState->WorldArena, tile_map );

		tile_map *TileMap = World->TileMap;
		TileMap->ChunkShift = 4; // 16 x 16 tile chunks
		TileMap->ChunkMask = ( 1 << TileMap->ChunkShift ) - 1; // 0x0010 - 1 = 0x000F
		TileMap->ChunkDim = ( 1 << TileMap->ChunkShift ); // 16

		TileMap->TileChunkCountX = 128;
		TileMap->TileChunkCountY = 128;
		TileMap->TileChunkCountZ = 2;

		// allocate all chunks
		TileMap->TileChunks = PushArray(
			&GameState->WorldArena,
			TileMap->TileChunkCountX *
			TileMap->TileChunkCountY *
			TileMap->TileChunkCountZ,
			tile_chunk );

		TileMap->TileSideInMeters = 1.4f;

		uint32 RandomNumberIndex = 0;

		const uint32 TilesPerWidth = 17;
		const uint32 TilesPerHeight = 9;
#if 0
		// TODO(nfauvet): waiting for full sparseness
		uint32 ScreenX = INT32_MAX / 2;
		uint32 ScreenY = INT32_MAX / 2;
#else
		uint32 ScreenX = 0;
		uint32 ScreenY = 0;
#endif

		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;

		uint32 AbsTileZ = 0;
		uint32 NbScreens = 3; // 100
		for ( uint32 ScreenIndex = 0; ScreenIndex < NbScreens; ++ScreenIndex ) {

			Assert( RandomNumberIndex < ArrayCount( RandomNumberTable ) );
			uint32 RandomChoice;
//			if ( DoorUp || DoorDown ) {
//				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2; // roll only for top and right doors
//			}
//			else {
//				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3; // roll for doors AND stairs
				RandomChoice = 0; // door right
//			}

			bool32 CreatedZDoor = false;
			// Select which kind of door the room is going to have
			if ( RandomChoice == 2 )
			{
				CreatedZDoor = true;
				if ( AbsTileZ == 0 )
				{
					DoorUp = true;
				}
				else
				{
					DoorDown = true;
				}
			}
			else if ( RandomChoice == 1 )
			{
				DoorTop = true;
			}
			else
			{
				DoorRight = true;
			}

			// Fill the room
			for ( uint32 TileY = 0; TileY < TilesPerHeight; ++TileY ) {
				for ( uint32 TileX = 0; TileX < TilesPerWidth; ++TileX ) {

					uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

					uint32 TileValue = 1; // default empty space ( door )
					if ( ( TileX == 0 ) && ( !DoorLeft || ( TileY != ( TilesPerHeight / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileX == TilesPerWidth - 1 ) && ( !DoorRight || ( TileY != ( TilesPerHeight / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileY == 0 ) && ( !DoorBottom || ( TileX != ( TilesPerWidth / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileY == TilesPerHeight - 1 ) && ( !DoorTop || ( TileX != ( TilesPerWidth / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					// stairs
					if ( ( TileX == 10 ) && ( TileY == 6 ) )
					{
						if ( DoorUp ) {
							TileValue = 3;
						}
						if ( DoorDown ) {
							TileValue = 4;
						}
					}

					SetTileValue( &GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ, TileValue );
					if ( TileValue == 2 )
					{
						AddWall( GameState, AbsTileX, AbsTileY, AbsTileZ );
					}
				}
			}

			// door to get back from where we came
			DoorLeft = DoorRight;
			DoorBottom = DoorTop;

			// stair to get backm or reset for next random
			if ( CreatedZDoor )
			{
				DoorUp = !DoorUp;
				DoorDown = !DoorDown;
			}
			else
			{
				DoorDown = false;
				DoorUp = false;
			}

			// reset for next random
			DoorRight = false;
			DoorTop = false;

			if ( RandomChoice == 0 ) {
				ScreenX++;
			}
			else if ( RandomChoice == 1 ){
				ScreenY++;
			}
			else {
				if ( AbsTileZ == 0 ) {
					AbsTileZ = 1;
				}
				else {
					AbsTileZ = 0;
				}
			}

		}

		// CAMERA
		tile_map_position NewCameraP = {};
		NewCameraP.AbsTileX = 17 / 2;
		NewCameraP.AbsTileY = 9 / 2;
		SetCamera(GameState, NewCameraP);

		Memory->IsInitialized = true;
	} // END OF INIT
	//
	//
	//





	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	int32 TileSideInPixels = 60;
	float MetersToPixels = (real32)TileSideInPixels / TileMap->TileSideInMeters;

	for(int ControllerIndex = 0; 
		ControllerIndex < ArrayCount( Input->Controllers ); 
		++ControllerIndex )
	{
		game_controller_input *Controller = GetController( Input, ControllerIndex );
		entity ControllingEntity = 
			GetEntity( GameState, EntityResidence_High, GameState->PlayerIndexForController[ControllerIndex] );
		if ( ControllingEntity.Residence != EntityResidence_Nonexistent )
		{
			v2 ddP = {}; // acceleration (second derivative of PlayerP)

			if ( Controller->IsAnalog )
			{
				ddP = v2{ Controller->StickAverageX, Controller->StickAverageY };
			}
			else
			{

				if ( Controller->MoveUp.EndedDown ){
					ddP.Y = 1.0f;
				}
				if ( Controller->MoveDown.EndedDown ){
					ddP.Y = -1.0f;
				}
				if ( Controller->MoveRight.EndedDown ){
					ddP.X = 1.0f;
				}
				if ( Controller->MoveLeft.EndedDown ){
					ddP.X = -1.0f;
				}
			}

			if (Controller->ActionUp.EndedDown)
			{
				ControllingEntity.High->dZ = 3.0f;
			}

			MovePlayer( GameState, ControllingEntity, Input->dtForFrame, ddP );
		}
		else
		{
			if ( Controller->Start.EndedDown )
			{
				uint32 EntityIndex = AddPlayer( GameState );
				GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
			}
		}



		//if ( !Controller->IsConnected )	{
		//	continue;
		//}

		
	}

	//
	// CAMERA
	//
	//v2 EntityOffsetForFrame = {};
	entity CameraFollowingEntity = GetEntity( GameState, EntityResidence_High, GameState->CameraFollowingEntityIndex );
	if ( CameraFollowingEntity.Residence != EntityResidence_Nonexistent )
	{
		tile_map_position NewCameraP = GameState->CameraP;

		// Z of the camera is always the same as the player
		NewCameraP.AbsTileZ = CameraFollowingEntity.Dormant->P.AbsTileZ;

#if 1
		// Move camera from screen to screen depending camera space distance
		// between the entity and the camera.
		if (CameraFollowingEntity.High->P.X > ( 9.0f * TileMap->TileSideInMeters ) ) {
			NewCameraP.AbsTileX += 17;
		}
		if (CameraFollowingEntity.High->P.X < ( -9.0f * TileMap->TileSideInMeters ) ) {
			NewCameraP.AbsTileX -= 17;
		}
		if (CameraFollowingEntity.High->P.Y > ( 5.0f * TileMap->TileSideInMeters ) ) {
			NewCameraP.AbsTileY += 9;
		}
		if (CameraFollowingEntity.High->P.Y < ( -5.0f * TileMap->TileSideInMeters ) ) {
			NewCameraP.AbsTileY -= 9;
		}
#else
		if (CameraFollowingEntity.High->P.X > (1.0f * TileMap->TileSideInMeters)) {
			NewCameraP.AbsTileX += 1;
		}
		if (CameraFollowingEntity.High->P.X < (-1.0f * TileMap->TileSideInMeters)) {
			NewCameraP.AbsTileX -= 1;
		}
		if (CameraFollowingEntity.High->P.Y >(1.0f * TileMap->TileSideInMeters)) {
			NewCameraP.AbsTileY += 1;
		}
		if (CameraFollowingEntity.High->P.Y < (-1.0f * TileMap->TileSideInMeters)) {
			NewCameraP.AbsTileY -= 1;
		}
#endif
		SetCamera( GameState, NewCameraP );
	}

	//
	// RENDER
	//

	// DRAW Background
	DrawBitmap( Buffer, &GameState->Backdrop, 0.0f, 0.0f );

	// Draw TileMap
	real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

#if 0
	for ( int32 RelRow = -10; RelRow < 10; ++RelRow ) 
	{
		for ( int32 RelColumn = -20; RelColumn < 20; ++RelColumn ) 
		{
			uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
			uint32 Row = GameState->CameraP.AbsTileY + RelRow;
			uint32 Depth = GameState->CameraP.AbsTileZ;
			uint32 TileID = GetTileValue( TileMap, Column, Row, Depth );

			if ( TileID > 1 ) 
			{
				real32 Gray = 0.5f;
				if ( TileID == 2 ) 
				{
					Gray = 1.0f;
				}

				if ( TileID > 2 ) 
				{
					Gray = 0.25f;
				}

				// debug
				if ( ( Row == GameState->CameraP.AbsTileY ) &&
					( Column == GameState->CameraP.AbsTileX ) )
				{
					Gray = 0.0f;
				}

				v2 TileSide = { 0.5f * TileSideInPixels, 0.5f * TileSideInPixels };
				v2 Center = {	ScreenCenterX - MetersToPixels * GameState->CameraP.Offset_.X + (real32)( RelColumn * TileSideInPixels ), // LowerLeftX +
								ScreenCenterY + MetersToPixels * GameState->CameraP.Offset_.Y - (real32)( RelRow * TileSideInPixels ) };
				v2 Min = Center - 0.9f * TileSide;
				v2 Max = Center + 0.9f * TileSide;
				DrawRectangle( Buffer, Min, Max, Gray, Gray, Gray );
			}
		}
	}
#endif

	// for each entity
	for ( uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; ++EntityIndex )
	{
		if (GameState->EntityResidence[EntityIndex] == EntityResidence_High )
		{
			high_entity *HighEntity = &GameState->HighEntities[EntityIndex];
			low_entity *EntityEntity = &GameState->LowEntities[EntityIndex];
			dormant_entity *DormantEntity = &GameState->DormantEntities[EntityIndex];

			//HighEntity->P += EntityOffsetForFrame;

			// JUMP
			real32 dt = Input->dtForFrame;
			real32 ddZ = -9.81f;
			HighEntity->Z = 0.5f * ddZ * Square(dt) + HighEntity->dZ * dt + HighEntity->Z; // Acceleration Speed Position
			HighEntity->dZ = ddZ*dt + HighEntity->dZ;
			if (HighEntity->Z < 0)
			{
				HighEntity->Z = 0;
			}
			real32 CAlpha = 1.0f - 0.5f * HighEntity->Z;
			if (CAlpha < 0.0f)
			{
				CAlpha = 0.0f;
			}
			real32 Z = -MetersToPixels * HighEntity->Z;

			// Draw player
			real32 PlayerR = 0.0f;
			real32 PlayerG = 1.0f;
			real32 PlayerB = 1.0f;

			v2 PlayerGroundPoint = {
				ScreenCenterX + MetersToPixels * HighEntity->P.X,
				ScreenCenterY - MetersToPixels * HighEntity->P.Y 
			};

			v2 PlayerLeftTop = {
				PlayerGroundPoint.X - MetersToPixels * ( 0.5f * DormantEntity->Width ),
				PlayerGroundPoint.Y - MetersToPixels * ( 0.5f * DormantEntity->Height ) 
			};

			v2 PlayerWidthHeight = { 
				DormantEntity->Width, DormantEntity->Height 
			};

			if (DormantEntity->Type == EntityType_Hero)
			{
				hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
				DrawBitmap(Buffer, &GameState->Shadow, PlayerGroundPoint.X, PlayerGroundPoint.Y, HeroBitmaps->AlignX, HeroBitmaps->AlignY, CAlpha);
				DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundPoint.X, PlayerGroundPoint.Y + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
				DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundPoint.X, PlayerGroundPoint.Y + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
				DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundPoint.X, PlayerGroundPoint.Y + Z, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
			}
			else
			{
				DrawRectangle(Buffer,
					PlayerLeftTop,
					PlayerLeftTop + MetersToPixels * PlayerWidthHeight,
					PlayerR, PlayerG, PlayerB);
			}
		}
	}
}

extern "C" GAME_GET_SOUND_SAMPLES( GameGetSoundSamples )
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	GameOutputSound( GameState, SoundBuffer, 512 );
}



// old stuff
#if 0
internal void
RenderWeirdGradient( game_offscreen_buffer *Buffer, int XOffset, int YOffset )
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for ( int Y = 0; Y < Buffer->Height; ++Y ) {
		uint32 *Pixel = (uint32*)Row;
		for ( int X = 0; X < Buffer->Width; ++X ) {
			// Memory:  BB GG RR xx
			// Register xx RR GG BB (little endian)
			uint8 RR = (uint8)( X + XOffset );
			uint8 GG = (uint8)( Y + YOffset );
			//*Pixel++ = (RR << 16) | (GG << 8);
			*Pixel++ = ( RR << 8 ) | ( GG << 16 );
		}
		Row += Buffer->Pitch;
	}

}
#endif

