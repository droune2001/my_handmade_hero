#include "handmade.h"
#include "handmade_world.cpp"
#include "handmade_random.h"
#include "handmade_sim_region.cpp"
#include "handmade_entity.cpp"

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
			real32 RealX, real32 RealY,	real32 CAlpha = 1.0f)
{
	int32 MinX = RoundReal32ToInt32( RealX );
	int32 MinY = RoundReal32ToInt32( RealY );
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

//inline low_entity*
//GetLowEntity(game_state *GameState, uint32 Index)
//{
//	low_entity *Result = 0;
//	if ((Index > 0) && (Index < GameState->LowEntityCount))
//	{
//		Result = GameState->LowEntities + Index;
//	}
//	return Result;
//}

inline v2
GetCameraSpaceP( game_state *GameState, low_entity *LowEntity )
{
	// NOTE(nfauvet): map the entity into camera space
	world_difference Diff = Subtract(GameState->World, &LowEntity->P, &GameState->CameraP);
	v2 Result = Diff.dXY;

	return Result;
}

//inline void
//MakeEntityLowFrequency( game_state *GameState, uint32 LowIndex )
//{
//	low_entity *LowEntity = &GameState->LowEntities[LowIndex];
//	uint32 HighIndex = LowEntity->HighEntityIndex;
//	if ( HighIndex ) // if low has a high
//	{
//		uint32 LastHighIndex = GameState->HighEntityCount - 1;
//		if ( HighIndex != LastHighIndex )
//		{
//			high_entity *LastEntity = GameState->HighEntities_ + LastHighIndex;
//			high_entity *DelEntity = GameState->HighEntities_ + HighIndex;
//
//			*DelEntity = *LastEntity; // move last to the deleted position
//			GameState->LowEntities[LastEntity->LowEntityIndex].HighEntityIndex = HighIndex;
//		}
//		--GameState->HighEntityCount;
//		LowEntity->HighEntityIndex = 0;
//	}
//}

struct add_low_entity_result
{
	low_entity *Low;
	uint32 LowIndex;
};

internal add_low_entity_result
AddLowEntity( game_state *GameState, entity_type Type, world_position *P )
{
	Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
	uint32 EntityIndex = GameState->LowEntityCount++;

	low_entity *EntityLow = GameState->LowEntities + EntityIndex;
	*EntityLow = {};
	EntityLow->Sim.Type = Type;

	ChangeEntityLocation( &GameState->WorldArena, GameState->World, EntityIndex, EntityLow, 0, P );
	
	add_low_entity_result Result;
	Result.Low = EntityLow;
	Result.LowIndex = EntityIndex;

	return Result;
}

internal add_low_entity_result
AddWall( game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddLowEntity( GameState, EntityType_Wall, &P );
	
	Entity.Low->Sim.Height = GameState->World->TileSideInMeters;
	Entity.Low->Sim.Width = Entity.Low->Sim.Height;
	Entity.Low->Sim.Collides = true;

	return Entity;
}

internal void
InitHitPoints( low_entity *EntityLow, int HitPointCount )
{
	Assert(HitPointCount <= ArrayCount(EntityLow->Sim.HitPoint));
	EntityLow->Sim.HitPointMax = HitPointCount;
	for (uint32 HitPointIndex = 0;
		HitPointIndex < EntityLow->Sim.HitPointMax;
		++HitPointIndex)
	{
		hit_point *HitPoint = EntityLow->Sim.HitPoint + HitPointIndex;
		HitPoint->Flags = 0;
		HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

internal add_low_entity_result
AddSword(game_state *GameState)
{
	add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, nullptr);

	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = false;

	return Entity;
}

internal add_low_entity_result
AddPlayer(game_state *GameState)
{
	world_position P = GameState->CameraP;
	P.Offset_.X -= 3.0f; // NICO
	P.Offset_.Y -= 4.0f;
	add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Hero, &P);

	Entity.Low->Sim.Height = 0.5f; // 1.4f;
	Entity.Low->Sim.Width = 1.0f; // 0.75f * Entity->Height;
	Entity.Low->Sim.Collides = true;

	InitHitPoints(Entity.Low, 3);

	add_low_entity_result Sword = AddSword(GameState);
	Entity.Low->Sim.Sword.Index = Sword.LowIndex;

	// if first entity, camera will follow it.
	if (GameState->CameraFollowingEntityIndex == 0)
	{
		GameState->CameraFollowingEntityIndex = Entity.LowIndex;
	}

	return Entity;
}

internal add_low_entity_result
AddMonstar( game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ )
{
	world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Monstar, &P);
	
	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = true;

	InitHitPoints(Entity.Low, 3);

	return Entity;
}

internal add_low_entity_result
AddFamiliar(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Familiar, &P);

	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = false;

	return Entity;
}

inline void
PushPiece(entity_visible_piece_group *Group, loaded_bitmap *Bitmap,
	v2 Offset, real32 OffsetZ, v2 Align, v2 Dim, v4 Color, real32 EntityZC )
{
	Assert(Group->PieceCount < ArrayCount(Group->Pieces));
	entity_visible_piece *Piece = Group->Pieces + Group->PieceCount++;
	Piece->Bitmap = Bitmap;
	Piece->Offset = Group->GameState->MetersToPixels * V2(Offset.X, -Offset.Y) - Align;
	Piece->OffsetZ = Group->GameState->MetersToPixels*OffsetZ;
	Piece->EntityZC = EntityZC;
	Piece->R = Color.R;
	Piece->G = Color.G;
	Piece->B = Color.B;
	Piece->A = Color.A;
	Piece->Dim = Dim;
}

internal void
PushBitmap(	entity_visible_piece_group *Group, loaded_bitmap *Bitmap,
			v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f,
			real32 EntityZC = 1.0f)
{
	PushPiece(Group, Bitmap, Offset, OffsetZ, Align, V2(0, 0), V4( 1.0f, 1.0f, 1.0f, Alpha), EntityZC);
}

internal void
PushRect(entity_visible_piece_group *Group,	v2 Offset, real32 OffsetZ, 
	v2 Dim, v4 Color,
	real32 EntityZC = 1.0f)
{
	PushPiece(Group, 0, Offset, OffsetZ, V2(0, 0), Dim, Color, EntityZC );
}

inline void
DrawHitpoints( sim_entity *Entity, entity_visible_piece_group *PieceGroup )
{
	if (Entity->HitPointMax >= 1)
	{
		v2 HealthDim = { 0.2f, 0.2f };
		real32 SpacingX = 1.5f * HealthDim.X;
		v2 HitP = { -0.5f * (Entity->HitPointMax - 1) * SpacingX , -0.25f };
		v2 dHitP = { SpacingX, 0.0f };
		for (uint32 HealthIndex = 0;
			HealthIndex < Entity->HitPointMax;
			++HealthIndex)
		{
			hit_point *HitPoint = Entity->HitPoint + HealthIndex;
			v4 Color = { 1.0f, 0.0f, 0.0f, 1.0f };
			if (HitPoint->FilledAmount == 0)
			{
				Color = { 0.2f, 0.2f, 0.2f, 1.0f };
			}

			PushRect(PieceGroup, HitP, 0, HealthDim, Color, 0.0f );
			HitP += dHitP;
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
		AddLowEntity( GameState, EntityType_Null, 0 );
		
		GameState->Backdrop = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp" );
		GameState->Shadow = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");
		GameState->Tree = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tree01.bmp");
		GameState->Sword = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/rock03.bmp");

		hero_bitmaps *Bitmap = &GameState->HeroBitmaps[0];

		Bitmap->Align = { 72, 182 };
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp" );
		++Bitmap;

		Bitmap->Align = { 72, 182 };
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp" );
		++Bitmap;

		Bitmap->Align = { 72, 182 };
		Bitmap->Head = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp" );
		Bitmap->Cape = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp" );
		Bitmap->Torso = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp" );
		++Bitmap;

		Bitmap->Align = { 72, 182 };
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

		int32 TileSideInPixels = 60;
		GameState->MetersToPixels = (real32)TileSideInPixels / World->TileSideInMeters;

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
//			if ( DoorUp || DoorDown ) 
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2; // roll only for top and right doors
			}
#if 0
			else 
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3; // roll for doors AND stairs
			}
#endif
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
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}

			// Fill the room
			for ( uint32 TileY = 0; TileY < TilesPerHeight; ++TileY ) 
			{
				for ( uint32 TileX = 0; TileX < TilesPerWidth; ++TileX ) 
				{
					uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

					uint32 TileValue = 1; // default empty space ( door )
					if ( ( TileX == 0 ) && ( !DoorLeft || ( TileY != ( TilesPerHeight / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileX == (TilesPerWidth - 1) ) && ( !DoorRight || ( TileY != ( TilesPerHeight / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileY == 0 ) && ( !DoorBottom || ( TileX != ( TilesPerWidth / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					if ( ( TileY == (TilesPerHeight - 1) ) && ( !DoorTop || ( TileX != ( TilesPerWidth / 2 ) ) ) )
					{
						TileValue = 2; // wall
					}

					// stairs
					if ( ( TileX == 10 ) && ( TileY == 6 ) )
					{
						if ( DoorUp ) 
						{
							TileValue = 3;
						}

						if ( DoorDown ) 
						{
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
				ScreenY++;
			}
			else if ( RandomChoice == 1 )
			{
				ScreenX++;
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
		uint32 CameraTileX = ScreenBaseX*TilesPerWidth + (17 / 2);
		uint32 CameraTileY = ScreenBaseY*TilesPerHeight + (9 / 2);
		uint32 CameraTileZ = ScreenBaseZ;
		NewCameraP = ChunkPositionFromTilePosition(	GameState->World,
													CameraTileX,
													CameraTileY,
													CameraTileZ );
		
		AddMonstar(GameState, CameraTileX + 2, CameraTileY + 2, CameraTileZ );
		for (int FamiliarIndex = 0;
			FamiliarIndex < 1;
			++FamiliarIndex)
		{
			int FamiliarOffsetX = (RandomNumberTable[RandomNumberIndex++] % 10) - 7;
			int FamiliarOffsetY = (RandomNumberTable[RandomNumberIndex++] % 10) - 3;
			if ((FamiliarOffsetX != 0) ||
				(FamiliarOffsetX != 0))
			{
				AddFamiliar(GameState, CameraTileX + FamiliarOffsetX,
					CameraTileY + FamiliarOffsetY, CameraTileZ);
			}
		}

		Memory->IsInitialized = true;
	} // END OF INIT

	//
	//
	//

	world *World = GameState->World;

	float MetersToPixels = GameState->MetersToPixels;

	for(int ControllerIndex = 0; 
		ControllerIndex < ArrayCount( Input->Controllers ); 
		++ControllerIndex )
	{
		game_controller_input *Controller = GetController( Input, ControllerIndex );
		controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
		if (ConHero->EntityIndex == 0 )
		{
			if ( Controller->Start.EndedDown )
			{
				*ConHero = {};
				ConHero->EntityIndex = AddPlayer(GameState).LowIndex;
			}
		}
		else
		{
			ConHero->ddP = {}; // acceleration (second derivative of PlayerP)

			if (Controller->IsAnalog)
			{
				ConHero->ddP = v2{ Controller->StickAverageX, Controller->StickAverageY };
			}
			else
			{
				if (Controller->MoveUp.EndedDown) {
					ConHero->ddP.Y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown) {
					ConHero->ddP.Y = -1.0f;
				}
				if (Controller->MoveRight.EndedDown) {
					ConHero->ddP.X = 1.0f;
				}
				if (Controller->MoveLeft.EndedDown) {
					ConHero->ddP.X = -1.0f;
				}
			}

			if (Controller->Start.EndedDown)
			{
				ConHero->dZ = 3.0f;
			}

			ConHero->dSword = {};
			if (Controller->ActionUp.EndedDown)
			{
				ConHero->dSword = V2(0.0f, 1.0f);
			}
			if (Controller->ActionDown.EndedDown)
			{
				ConHero->dSword = V2(0.0f, -1.0f);
			}
			if (Controller->ActionLeft.EndedDown)
			{
				ConHero->dSword = V2(-1.0f, 0.0f);
			}
			if (Controller->ActionRight.EndedDown)
			{
				ConHero->dSword = V2(1.0f, 0.0f);
			}
		}		
	}

	//
	// CAMERA
	//

	// 3 x 3 screens around the camera
	uint32 TileSpanX = 17 * 3;
	uint32 TileSpanY = 9 * 3;
	rectangle2 CameraBounds = RectCenterDim(V2(0, 0),
		World->TileSideInMeters * V2((real32)TileSpanX, (real32)TileSpanY));

	memory_arena SimArena;
	InitializeArena(&SimArena, Memory->TransientStorageSize, Memory->TransientStorage);
	sim_region *SimRegion = BeginSim( &SimArena, GameState, World, GameState->CameraP, CameraBounds );

	//
	// RENDER
	//

	// DRAW Background
#if 1
	DrawRectangle( Buffer, V2(0.0f,0.0f), V2((real32)Buffer->Width, (real32)Buffer->Height), 0.5f, 0.5f, 0.5f);
#else
	DrawBitmap(Buffer, &GameState->Backdrop, 0.0f, 0.0f);
#endif
	// Draw World
	real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

	// for each entity
	entity_visible_piece_group PieceGroup = {};
	PieceGroup.GameState = GameState;

	sim_entity *Entity = SimRegion->Entities;
	for(uint32 EntityIndex = 1; 
		EntityIndex < SimRegion->EntityCount;
		++EntityIndex, ++Entity)
	{
		PieceGroup.PieceCount = 0;
		real32 dt = Input->dtForFrame;

		// TODO(nfauvet): this is incorrect, should be conputer after update!!!
		real32 ShadowAlpha = 1.0f - 0.5f * Entity->Z;
		if (ShadowAlpha < 0.0f)
		{
			ShadowAlpha = 0.0f;
		}

		// Draw Entities
		hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
		switch (Entity->Type)
		{
			case EntityType_Hero:
			{
				for(uint32 ControlIndex = 0;
					ControlIndex < ArrayCount(GameState->ControlledHeroes);
					++ControlIndex)
				{
					controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;
					if (Entity->StorageIndex == ConHero->EntityIndex)
					{
						// move ourselves
						Entity->dZ = ConHero->dZ;

						move_spec MoveSpec;
						MoveSpec.UnitMaxAccelVector = true;
						MoveSpec.Speed = 50.0f;
						MoveSpec.Drag = 8.0f;

						MoveEntity(SimRegion, Entity, Input->dtForFrame, &MoveSpec, ConHero->ddP);

						// if sword fired
						if ((ConHero->dSword.X != 0.0f) || (ConHero->dSword.Y != 0.0f))
						{
							sim_entity *Sword = Entity->Sword.Ptr;
							if(Sword)
							{
								Sword->P = Entity->P;
								Sword->DistanceRemaining = 5.0f;
								Sword->dP = 5.0f * ConHero->dSword;
							}
						}
					}
				}

				PushBitmap( &PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f );
				PushBitmap( &PieceGroup, &HeroBitmaps->Torso, V2(0, 0), 0, HeroBitmaps->Align);
				PushBitmap( &PieceGroup, &HeroBitmaps->Cape, V2(0, 0), 0, HeroBitmaps->Align);
				PushBitmap( &PieceGroup, &HeroBitmaps->Head, V2(0, 0), 0, HeroBitmaps->Align);

				DrawHitpoints( Entity, &PieceGroup );
			} break;

			case EntityType_Wall:
			{
				PushBitmap( &PieceGroup, &GameState->Tree, V2(0, 0), 0, V2(45, 80));
			} break;

			case EntityType_Sword:
			{
				UpdateSword( SimRegion, Entity, dt);
				PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
				PushBitmap(&PieceGroup, &GameState->Sword, V2(0, 0), 0, V2(29, 10));
			} break;

			case EntityType_Monstar:
			{
				UpdateMonstar( SimRegion, Entity, dt );
				PushBitmap( &PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
				PushBitmap( &PieceGroup, &HeroBitmaps->Torso, V2(0, 0), 0, HeroBitmaps->Align );
				DrawHitpoints( Entity, &PieceGroup );
			} break;

			case EntityType_Familiar:
			{
				UpdateFamiliar(SimRegion, Entity, dt);
				Entity->tBob += dt;
				if (Entity->tBob > (2.0f*Pi32))
				{
					Entity->tBob -= 2.0f*Pi32;
				}
				real32 BobSin = Sin(2.0f*Entity->tBob);
				PushBitmap( &PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, (0.5f*ShadowAlpha)+0.2f*BobSin, 0.0f);
				PushBitmap( &PieceGroup, &HeroBitmaps->Head, V2(0, 0), 0.25f * BobSin, HeroBitmaps->Align );
			} break;

			default: 
			{
				InvalidCodePath;
			} break;
		}



		// JUMP
		real32 ddZ = -9.81f;
		Entity->Z = 0.5f * ddZ * Square(dt) + Entity->dZ * dt + Entity->Z; // Acceleration Speed Position
		Entity->dZ = ddZ*dt + Entity->dZ;
		if (Entity->Z < 0)
		{
			Entity->Z = 0;
		}
		
		



		v2 EntityGroundPoint = { ScreenCenterX + MetersToPixels * Entity->P.X,
								ScreenCenterY - MetersToPixels * Entity->P.Y };
		real32 EntityZ = -MetersToPixels * Entity->Z;

#if 0
		v2 PlayerLeftTop = { EntityGroundPoint.X - MetersToPixels * (0.5f * LowEntity->Width),
							EntityGroundPoint.Y - MetersToPixels * (0.5f * LowEntity->Height) };
		v2 EntityWidthHeight = { LowEntity->Width, LowEntity->Height };
		DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + 0.9f*MetersToPixels*EntityWidthHeight, 1.0f, 1.0f, 0.0f);
#endif

		for ( uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex )
		{
			entity_visible_piece *Piece = PieceGroup.Pieces + PieceIndex;
			v2 Center = { EntityGroundPoint.X + Piece->Offset.X,
				EntityGroundPoint.Y + Piece->Offset.Y + Piece->OffsetZ + Piece->EntityZC * EntityZ };
			if (Piece->Bitmap)
			{
				DrawBitmap( Buffer, Piece->Bitmap, Center.X, Center.Y, Piece->A );
			}
			else
			{
				v2 HalfDim = 0.5f * MetersToPixels * Piece->Dim;
				DrawRectangle(Buffer,
					Center - HalfDim,
					Center + HalfDim,
					Piece->R, Piece->G, Piece->B );
			}
		}
	}

	// TODO(nfauvet): Figure out why the origin is where it is...
	world_position WorldOrigin = {};
	world_difference Diff = Subtract(SimRegion->World, &WorldOrigin, &SimRegion->Origin);
	DrawRectangle( Buffer, Diff.dXY, V2(10,10), 1,1,0 );

	EndSim( SimRegion, GameState );
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

