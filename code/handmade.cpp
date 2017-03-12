#include "handmade.h"
#include "handmade_world.cpp"
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

inline low_entity*
GetLowEntity(game_state *GameState, uint32 Index)
{
	low_entity *Result = 0;
	if ((Index > 0) && (Index < GameState->LowEntityCount))
	{
		Result = GameState->LowEntities + Index;
	}
	return Result;
}

inline high_entity*
MakeEntityHighFrequency(game_state *GameState, uint32 LowIndex)
{
	high_entity *EntityHigh = 0;

	low_entity *LowEntity = &GameState->LowEntities[LowIndex];
	if (LowEntity->HighEntityIndex) // if not already in high
	{
		EntityHigh = GameState->HighEntities_ + LowEntity->HighEntityIndex;
	}
	else
	{
		if (GameState->HighEntityCount < ArrayCount(GameState->HighEntities_))
		{
			uint32 HighIndex = GameState->HighEntityCount++;
			high_entity *HighEntity = GameState->HighEntities_ + HighIndex;
			world_difference Diff = Subtract(GameState->World, &LowEntity->P, &GameState->CameraP);
			HighEntity->P = Diff.dXY;
			HighEntity->dP = V2(0, 0);
			HighEntity->ChunkZ = LowEntity->P.ChunkZ;
			HighEntity->FacingDirection = 0;
			HighEntity->LowEntityIndex = LowIndex;

			LowEntity->HighEntityIndex = HighIndex;
		}
		else
		{
			InvalidCodePath;
		}
	}

	return EntityHigh;
}

inline entity
GetHighEntity( game_state *GameState, uint32 LowIndex )
{
	entity Result = {};
	
	if ((LowIndex > 0) && (LowIndex < GameState->LowEntityCount))
	{
		Result.LowIndex = LowIndex;
		Result.Low = GameState->LowEntities + LowIndex;
		Result.High = MakeEntityHighFrequency(GameState, LowIndex);
	}
	return Result;
}

inline void
MakeEntityLowFrequency( game_state *GameState, uint32 LowIndex )
{
	low_entity *LowEntity = &GameState->LowEntities[LowIndex];
	uint32 HighIndex = LowEntity->HighEntityIndex;
	if ( HighIndex ) // if low has a high
	{
		uint32 LastHighIndex = GameState->HighEntityCount - 1;
		if ( HighIndex != LastHighIndex )
		{
			high_entity *LastEntity = GameState->HighEntities_ + LastHighIndex;
			high_entity *DelEntity = GameState->HighEntities_ + HighIndex;

			*DelEntity = *LastEntity; // move last to the deleted position
			GameState->LowEntities[LastEntity->LowEntityIndex].HighEntityIndex = HighIndex;
		}
		--GameState->HighEntityCount;
		LowEntity->HighEntityIndex = 0;
	}
}

inline void
OffsetAndCheckFrequencyByArea( game_state *GameState, v2 Offset, rectangle2 HighFrequencyBounds )
{
	// kick entities too far into low residency
	for (uint32 EntityIndex = 1;
		EntityIndex < GameState->HighEntityCount;
		)
	{
		high_entity *High = GameState->HighEntities_ + EntityIndex;
		High->P += Offset;

		if (IsInRectangle(HighFrequencyBounds, High->P))
		{
			++EntityIndex;
		}
		else
		{
			// mutates the array
			MakeEntityLowFrequency(GameState, High->LowEntityIndex);
		}
	}
}

internal uint32
AddLowEntity( game_state *GameState, entity_type Type )
{
	Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
	uint32 EntityIndex = GameState->LowEntityCount++;

	GameState->LowEntities[EntityIndex] = {};
	GameState->LowEntities[EntityIndex].Type = Type;

	return EntityIndex;
}

internal uint32
AddPlayer( game_state *GameState )
{
	uint32 EntityIndex = AddLowEntity( GameState, EntityType_Hero );
	low_entity *Entity = GetLowEntity( GameState, EntityIndex );

	Entity->P = GameState->CameraP;
	//Entity->P.AbsTileX = 2;
	//Entity->P.AbsTileY = 3;
	Entity->P.Offset_.X = 0.0f;
	Entity->P.Offset_.Y = 0.0f;
	Entity->Height = 0.5f; // 1.4f;
	Entity->Width = 1.0f; // 0.75f * Entity->Height;
	Entity->Collides = true;

	//MakeEntityHighFrequency( GameState, EntityIndex );

	// if first entity, camera will follow it.
	if ( GameState->CameraFollowingEntityIndex == 0 )
	{
		GameState->CameraFollowingEntityIndex = EntityIndex;
	}

	return EntityIndex;
}

internal uint32
AddWall( game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	uint32 EntityIndex = AddLowEntity( GameState, EntityType_Wall );
	low_entity *Entity = GetLowEntity( GameState, EntityIndex );

	Entity->P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ );
	Entity->Height = GameState->World->TileSideInMeters;
	Entity->Width = Entity->Height;
	Entity->Collides = true;

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
	world *World = GameState->World;

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

	uint32 EntityTileWidth = CeilReal32ToInt32( Entity->Width / World->TileSideInMeters );
	uint32 EntityTileHeight = CeilReal32ToInt32( Entity->Height / World->TileSideInMeters );

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
		uint32 HitHighEntityIndex = 0;

		v2 DesiredPosition = Entity.High->P + PlayerDelta;

		// iterate over all entities
		for (uint32 TestHighEntityIndex = 1;
			TestHighEntityIndex < GameState->HighEntityCount;
			++TestHighEntityIndex)
		{
			if ( TestHighEntityIndex != Entity.Low->HighEntityIndex ) // dont collision test with self!!
			{
				entity TestEntity;
				TestEntity.High = GameState->HighEntities_ + TestHighEntityIndex;
				TestEntity.LowIndex = TestEntity.High->LowEntityIndex;
				TestEntity.Low = GameState->LowEntities + TestEntity.LowIndex;
				if ( TestEntity.Low->Collides )
				{
					// bbox with the two tested entities
					real32 DiameterW = TestEntity.Low->Width + Entity.Low->Width;
					real32 DiameterH = TestEntity.Low->Height + Entity.Low->Height;

					v2 MinCorner = -0.5f * v2{ DiameterW, DiameterH };
					v2 MaxCorner = 0.5f * v2{ DiameterW, DiameterH };

					v2 Rel = Entity.High->P - TestEntity.High->P;

					if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = { -1,0 };
						HitHighEntityIndex = TestHighEntityIndex;
					}

					if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = { 1,0 };
						HitHighEntityIndex = TestHighEntityIndex;
					}

					if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = { 0,-1 };
						HitHighEntityIndex = TestHighEntityIndex;
					}

					if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = { 0,1 };
						HitHighEntityIndex = TestHighEntityIndex;
					}
				}
			}
		}		

		Entity.High->P += tMin * PlayerDelta; // hit position

		if ( HitHighEntityIndex )
		{
			// commit movement
			Entity.High->dP = Entity.High->dP - Inner(Entity.High->dP, WallNormal) * WallNormal;
			PlayerDelta = DesiredPosition - Entity.High->P; // remaining delta after the hit position
			PlayerDelta = PlayerDelta - Inner(PlayerDelta, WallNormal) * WallNormal; // along the wall
			
			high_entity *HitHigh = GameState->HighEntities_ + HitHighEntityIndex; // TODO(nfauvet): call a GetHighEntity() which checks bounds;
			low_entity *HitLow = GetLowEntity( GameState, HitHigh->LowEntityIndex );
			// TODO(nfauvet): stairs
			//Entity.High->AbsTileZ += HitLow->dAbsTileZ;
		}
		else 
		{
			break;
		}
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

	// after the float position computation, map back into tile space
	Entity.Low->P = MapIntoTileSpace(GameState->World, GameState->CameraP, Entity.High->P);
}

internal void
SetCamera( game_state *GameState, world_position NewCameraP )
{
	world *World = GameState->World;
	world_difference dCameraP = Subtract( World, &NewCameraP, &GameState->CameraP );
	GameState->CameraP = NewCameraP;

	// 3 x 3 screens around the camera
	uint32 TileSpanX = 17 * 3;
	uint32 TileSpanY = 9 * 3;
	rectangle2 CameraBounds = RectCenterDim( V2(0,0), 
								World->TileSideInMeters * V2((real32)TileSpanX,(real32)TileSpanY));

	v2 EntityOffsetForFrame = -dCameraP.dXY;
	OffsetAndCheckFrequencyByArea(GameState, EntityOffsetForFrame, CameraBounds);

// TODO(nfauvet): do it in CHUNK space now
#if 0
	// move close entities into high
	int32 MinTileX = 0; // NewCameraP.AbsTileX - TileSpanX / 2;
	int32 MaxTileX = NewCameraP.AbsTileX + TileSpanX/2;
	int32 MinTileY = 0; // NewCameraP.AbsTileY - TileSpanY / 2;
	int32 MaxTileY = NewCameraP.AbsTileY + TileSpanY/2;
	for (uint32 EntityIndex = 1;
		EntityIndex < GameState->LowEntityCount;
		++EntityIndex)
	{
		low_entity *Low = GameState->LowEntities + EntityIndex;
		if ( Low->HighEntityIndex == 0 )
		{
			if ((Low->P.AbsTileZ == NewCameraP.AbsTileZ) &&
				(Low->P.AbsTileX >= MinTileX) &&
				(Low->P.AbsTileX <= MaxTileX) &&
				(Low->P.AbsTileY >= MinTileY) &&
				(Low->P.AbsTileY <= MaxTileY))
			{
				MakeEntityHighFrequency( GameState, EntityIndex );
			}
		}
	}
#endif
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
		AddLowEntity( GameState, EntityType_Null );
		GameState->HighEntityCount = 1;

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

		InitializeWorld( World, 1.4f );

		uint32 RandomNumberIndex = 0;

		const uint32 TilesPerWidth = 17;
		const uint32 TilesPerHeight = 9;
		uint32 ScreenBaseX = 0;
		uint32 ScreenBaseY = 0;
		uint32 ScreenBaseZ = 0;
		uint32 ScreenX = ScreenBaseX;
		uint32 ScreenY = ScreenBaseY;
		uint32 AbsTileZ = ScreenBaseZ;

		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;

		uint32 NbScreens = 2000;
		for ( uint32 ScreenIndex = 0; 
			ScreenIndex < NbScreens; 
			++ScreenIndex ) {

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
				if ( AbsTileZ == ScreenBaseZ )
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

			if ( RandomChoice == 0 ) 
			{
				ScreenX++;
			}
			else if ( RandomChoice == 1 )
			{
				ScreenY++;
			}
			else 
			{
				if ( AbsTileZ == ScreenBaseZ ) 
				{
					AbsTileZ = ScreenBaseZ + 1;
				}
				else 
				{
					AbsTileZ = ScreenBaseZ;
				}
			}
		}

		// CAMERA
		world_position NewCameraP = {};
		NewCameraP = ChunkPositionFromTilePosition(	GameState->World,
													ScreenBaseX*TilesPerWidth + ( 17 / 2 ),
													ScreenBaseY*TilesPerHeight + (9 / 2 ),
													ScreenBaseZ );
		SetCamera(GameState, NewCameraP);

		Memory->IsInitialized = true;
	} // END OF INIT

	//
	//
	//

	world *World = GameState->World;

	int32 TileSideInPixels = 60;
	float MetersToPixels = (real32)TileSideInPixels / World->TileSideInMeters;

	for(int ControllerIndex = 0; 
		ControllerIndex < ArrayCount( Input->Controllers ); 
		++ControllerIndex )
	{
		game_controller_input *Controller = GetController( Input, ControllerIndex );
		uint32 LowIndex = GameState->PlayerIndexForController[ControllerIndex];
		if ( LowIndex == 0 )
		{
			if ( Controller->Start.EndedDown )
			{
				uint32 EntityIndex = AddPlayer(GameState);
				GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
			}
		}
		else
		{
			entity ControllingEntity = GetHighEntity( GameState, LowIndex );
			v2 ddP = {}; // acceleration (second derivative of PlayerP)

			if (Controller->IsAnalog)
			{
				ddP = v2{ Controller->StickAverageX, Controller->StickAverageY };
			}
			else
			{
				if (Controller->MoveUp.EndedDown) {
					ddP.Y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown) {
					ddP.Y = -1.0f;
				}
				if (Controller->MoveRight.EndedDown) {
					ddP.X = 1.0f;
				}
				if (Controller->MoveLeft.EndedDown) {
					ddP.X = -1.0f;
				}
			}

			if (Controller->ActionUp.EndedDown)
			{
				ControllingEntity.High->dZ = 3.0f;
			}

			MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddP);
		}		
	}

	//
	// CAMERA
	//
	//v2 EntityOffsetForFrame = {};
	entity CameraFollowingEntity = GetHighEntity( GameState, GameState->CameraFollowingEntityIndex );
	if ( CameraFollowingEntity.High )
	{
		world_position NewCameraP = GameState->CameraP;

		// Z of the camera is always the same as the player
		NewCameraP.ChunkZ = CameraFollowingEntity.Low->P.ChunkZ;

#if 0
		// Move camera from screen to screen depending camera space distance
		// between the entity and the camera.
		if (CameraFollowingEntity.High->P.X > ( 9.0f * World->TileSideInMeters ) ) {
			NewCameraP.AbsTileX += 17;
		}
		if (CameraFollowingEntity.High->P.X < ( -9.0f * World->TileSideInMeters ) ) {
			NewCameraP.AbsTileX -= 17;
		}
		if (CameraFollowingEntity.High->P.Y > ( 5.0f * World->TileSideInMeters ) ) {
			NewCameraP.AbsTileY += 9;
		}
		if (CameraFollowingEntity.High->P.Y < ( -5.0f * World->TileSideInMeters ) ) {
			NewCameraP.AbsTileY -= 9;
		}
#else
		NewCameraP = CameraFollowingEntity.Low->P;
#endif
		SetCamera( GameState, NewCameraP );
	}

	//
	// RENDER
	//

	// DRAW Background
	DrawBitmap( Buffer, &GameState->Backdrop, 0.0f, 0.0f );

	// Draw World
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
			uint32 TileID = GetTileValue( World, Column, Row, Depth );

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
	for(uint32 HighEntityIndex = 0; 
		HighEntityIndex < GameState->HighEntityCount;
		++HighEntityIndex)
	{
		high_entity *HighEntity = GameState->HighEntities_ + HighEntityIndex;
		low_entity *LowEntity = GameState->LowEntities + HighEntity->LowEntityIndex;
			
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
			PlayerGroundPoint.X - MetersToPixels * ( 0.5f * LowEntity->Width ),
			PlayerGroundPoint.Y - MetersToPixels * ( 0.5f * LowEntity->Height ) 
		};

		v2 PlayerWidthHeight = { 
			LowEntity->Width, LowEntity->Height 
		};

		if (LowEntity->Type == EntityType_Hero)
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

