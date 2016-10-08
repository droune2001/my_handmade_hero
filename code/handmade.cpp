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
				real32 RealMinX, real32 RealMinY,
				real32 RealMaxX, real32 RealMaxY,
				real32 R, real32 G, real32 B )
{	
	int32 MinX = RoundReal32ToInt32( RealMinX );
	int32 MinY = RoundReal32ToInt32( RealMinY );
	int32 MaxX = RoundReal32ToInt32( RealMaxX );
	int32 MaxY = RoundReal32ToInt32( RealMaxY );

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
			real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0 )
{
	int32 MinX = RoundReal32ToInt32( RealX - AlignX );
	int32 MinY = RoundReal32ToInt32( RealY - AlignY );
	int32 MaxX = RoundReal32ToInt32( RealX - AlignX + (real32)Bitmap->Width );
	int32 MaxY = RoundReal32ToInt32( RealY - AlignY + (real32)Bitmap->Height );

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

		bit_scan_result RedShift = FindLeastSignificantSetBit( RedMask );
		bit_scan_result GreenShift = FindLeastSignificantSetBit( GreenMask );
		bit_scan_result BlueShift = FindLeastSignificantSetBit( BlueMask );
		bit_scan_result AlphaShift = FindLeastSignificantSetBit( AlphaMask );
		
		Assert( RedShift.Found );
		Assert( GreenShift.Found );
		Assert( BlueShift.Found );
		Assert( AlphaShift.Found );

		// We want 0xARGB
		uint32 *SourceDest = Pixels;
		for ( int32 Y = 0; Y < Header->Height; ++Y )
		{
			for ( int32 X = 0; X < Header->Width; ++X )
			{
				uint32 C = *SourceDest;
				*SourceDest++ = ((( C >> AlphaShift.Index ) & 0xFF ) << 24 ) |
								((( C >> RedShift.Index ) & 0xFF ) << 16 ) |
								((( C >> GreenShift.Index ) & 0xFF ) << 8 ) |
								((( C >> BlueShift.Index ) & 0xFF ) );
			}
		}

	}

	return Result;
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( ( &Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0] ) == ArrayCount( Input->Controllers[0].Buttons ) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );

	real32 PlayerHeight = 1.4f;
	real32 PlayerWidth = 0.75f * PlayerHeight;

	// game_state occupies the beginning of the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	if ( !Memory->IsInitialized )
	{
		GameState->Backdrop = DEBUGLoadBMP( Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp" );

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

		GameState->CameraP.AbsTileX = 17 / 2;
		GameState->CameraP.AbsTileY = 9 / 2;

		GameState->PlayerP.AbsTileX = 2;
		GameState->PlayerP.AbsTileY = 3;
		GameState->PlayerP.OffsetX = 0.2f;
		GameState->PlayerP.OffsetY = 0.3f;

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
		uint32 ScreenX = 0;
		uint32 ScreenY = 0;

		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;

		uint32 AbsTileZ = 0;

		for ( uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex ) {

			Assert( RandomNumberIndex < ArrayCount( RandomNumberTable ) );
			uint32 RandomChoice;
			if ( DoorUp || DoorDown ) {
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2; // roll only for top and right doors
			}
			else {
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3; // roll for doors AND stairs
			}

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



		Memory->IsInitialized = true;
	}

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	int32 TileSideInPixels = 60;
	float MetersToPixels = (real32)TileSideInPixels / TileMap->TileSideInMeters;

	for ( int ControllerIndex = 0; ControllerIndex < ArrayCount( Input->Controllers ); ++ControllerIndex )
	{
		game_controller_input *Controller = GetController( Input, ControllerIndex );
		if ( Controller->IsConnected )
		{
			if ( Controller->IsAnalog )
			{

			}
			else
			{
				real32 dPlayerX = 0.0f;
				real32 dPlayerY = 0.0f;
				if ( Controller->MoveUp.EndedDown ){
					GameState->HeroFacingDirection = 1;
					dPlayerY = 1.0f;
				}
				if ( Controller->MoveDown.EndedDown ){
					GameState->HeroFacingDirection = 3;
					dPlayerY = -1.0f;
				}
				if ( Controller->MoveRight.EndedDown ){
					GameState->HeroFacingDirection = 0;
					dPlayerX = 1.0f;
				}
				if ( Controller->MoveLeft.EndedDown ){
					GameState->HeroFacingDirection = 2;
					dPlayerX = -1.0f;
				}

				real32 Speed = 5.0f; // m/s
				if ( Controller->ActionUp.EndedDown ){
					Speed = 10.0f;
				}

				// temporary position
				tile_map_position NewPlayerP = GameState->PlayerP;
				NewPlayerP.OffsetX += Input->dtForFrame * Speed * dPlayerX;
				NewPlayerP.OffsetY += Input->dtForFrame * Speed * dPlayerY;
				NewPlayerP = RecanonicalizePosition( TileMap, NewPlayerP );

				tile_map_position PlayerLeft = NewPlayerP;
				PlayerLeft.OffsetX -= 0.5f * PlayerWidth;
				PlayerLeft = RecanonicalizePosition( TileMap, PlayerLeft );

				tile_map_position PlayerRight = NewPlayerP;
				PlayerRight.OffsetX += 0.5f * PlayerWidth;
				PlayerRight = RecanonicalizePosition( TileMap, PlayerRight );

				// test bottom left, middle and right points.
				if (IsTileMapPointEmpty( TileMap, PlayerLeft ) &&
					IsTileMapPointEmpty( TileMap, PlayerRight ) &&
					IsTileMapPointEmpty( TileMap, NewPlayerP ) )
				{
					if ( !AreOnSameTile( &GameState->PlayerP, &NewPlayerP ) )
					{
						uint32 NewTileValue = GetTileValue( TileMap, NewPlayerP );
						if ( NewTileValue == 3 )
						{
							++NewPlayerP.AbsTileZ;
						}
						else if ( NewTileValue == 4 )
						{
							--NewPlayerP.AbsTileZ;
						}
					}

					// if position validated, commit it.
					GameState->PlayerP = NewPlayerP;
				}

				// Z of the camera is always the same as the player
				GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;

				// Move camera from screen to screen depending on diff in tiles
				tile_map_difference Diff = Substract( TileMap, &GameState->PlayerP, &GameState->CameraP );
				if ( Diff.dX > 9.0f * ( TileMap->TileSideInMeters ) ) {
					GameState->CameraP.AbsTileX += 17;
				}
				if ( Diff.dX < -9.0f * ( TileMap->TileSideInMeters ) ) {
					GameState->CameraP.AbsTileX -= 17;
				}
				if ( Diff.dY > 5.0f * ( TileMap->TileSideInMeters ) ) {
					GameState->CameraP.AbsTileY += 9;
				}
				if ( Diff.dY < -5.0f * ( TileMap->TileSideInMeters ) ) {
					GameState->CameraP.AbsTileY -= 9;
				}
			}
		}
	}


	// DRAW Background
	DrawBitmap( Buffer, &GameState->Backdrop, 0.0f, 0.0f );

	// Draw TileMap
	real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

	for ( int32 RelRow = -10; RelRow < 10; ++RelRow ) {
		for ( int32 RelColumn = -20; RelColumn < 20; ++RelColumn ) {

			uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
			uint32 Row = GameState->CameraP.AbsTileY + RelRow;
			uint32 Depth = GameState->CameraP.AbsTileZ;
			uint32 TileID = GetTileValue( TileMap, Column, Row, Depth );

			if ( TileID > 1 ) {

				real32 Gray = 0.5f;
				if ( TileID == 2 ) {
					Gray = 1.0f;
				}

				if ( TileID > 2 ) {
					Gray = 0.25f;
				}

				// debug
				if ( ( Row == GameState->CameraP.AbsTileY ) &&
					( Column == GameState->CameraP.AbsTileX ) )
				{
					Gray = 0.0f;
				}

				real32 CenterX = ScreenCenterX - MetersToPixels * GameState->CameraP.OffsetX + (real32)( RelColumn * TileSideInPixels ); // LowerLeftX +
				real32 CenterY = ScreenCenterY + MetersToPixels * GameState->CameraP.OffsetY - (real32)( RelRow * TileSideInPixels ); // LowerLeftY - 
				real32 MinX = CenterX - 0.5f * TileSideInPixels;
				real32 MinY = CenterY - 0.5f * TileSideInPixels;
				real32 MaxX = CenterX + 0.5f * TileSideInPixels;
				real32 MaxY = CenterY + 0.5f * TileSideInPixels;
				DrawRectangle( Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray );
			}
		}
	}

	// Draw player

	tile_map_difference Diff = Substract( TileMap, &GameState->PlayerP, &GameState->CameraP );

	real32 PlayerR = 0.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 1.0f;

	real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels * Diff.dX;
	real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels * Diff.dY;

	real32 PlayerLeft = PlayerGroundPointX - MetersToPixels * ( 0.5f * PlayerWidth );
	real32 PlayerTop = PlayerGroundPointY - MetersToPixels * PlayerHeight;

	DrawRectangle( Buffer,
		PlayerLeft, PlayerTop,
		PlayerLeft + MetersToPixels * PlayerWidth,
		PlayerTop + MetersToPixels * PlayerHeight,
		PlayerR, PlayerG, PlayerB );

	hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[GameState->HeroFacingDirection];
	DrawBitmap( Buffer, &HeroBitmaps->Torso, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY );
	DrawBitmap( Buffer, &HeroBitmaps->Cape, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY );
	DrawBitmap( Buffer, &HeroBitmaps->Head, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY );
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

