#include "handmade.h"
#include "handmade_intrinsics.h"

internal void
GameOutputSound( game_state *GameState, game_sound_output_buffer *pSoundBuffer, int ToneHz )
{
	local_persist real32 tSine;
	int16 ToneVolume = 1000;
	int WavePeriod = pSoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = pSoundBuffer->Samples;
	for ( int SampleIndex = 0; SampleIndex < pSoundBuffer->SampleCount; ++SampleIndex ) {
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

inline tile_map*
GetTileMap( world *World, int32 TileMapX, int32 TileMapY )
{
	tile_map *TileMap = 0;

	if (( TileMapX >= 0 ) && ( TileMapX < World->TileMapCountX ) &&
		( TileMapY >= 0 ) && ( TileMapY < World->TileMapCountY ) )
	{
		TileMap = &World->TileMaps[World->TileMapCountX * TileMapY + TileMapX];
	}
	return TileMap;
}


inline uint32
GetTileValueUnchecked( world *World, tile_map *TileMap, int32 TileX, int32 TileY )
{
	Assert( TileMap );
	Assert( ( TileX >= 0 ) && ( TileX < World->CountX ) &&
			( TileY >= 0 ) && ( TileY < World->CountY ) );

	uint32 TileMapValue = TileMap->Tiles[World->CountX * TileY + TileX];
	return TileMapValue;
}

internal bool32
IsTileMapPointEmpty( world *World, tile_map *TileMap, int32 TestTileX, int32 TestTileY )
{
	bool32 Empty = false;

	if ( TileMap )
	{
		bool32 IsValid = false;
		if (( TestTileX >= 0 ) && ( TestTileX < World->CountX ) &&
			( TestTileY >= 0 ) && ( TestTileY < World->CountY ) )
		{
			uint32 TileMapValue = GetTileValueUnchecked( World, TileMap, TestTileX, TestTileY );
			Empty = ( TileMapValue == 0 );
		}
	}

	return Empty;
}

inline canonical_position
GetCanonicalPosition( world *World, raw_position Pos )
{
	canonical_position Result;
	Result.TileMapX = Pos.TileMapX;
	Result.TileMapY = Pos.TileMapY;

	real32 X = Pos.X - World->UpperLeftX; // relative to the tilemap.
	real32 Y = Pos.Y - World->UpperLeftY; // relative to the tilemap.

	Result.TileX = FloorReal32ToInt32( X / World->TileSideInPixels );
	Result.TileY = FloorReal32ToInt32( Y / World->TileSideInPixels );

	Result.X = X - ( (real32)Result.TileX * (real32)World->TileSideInPixels ); // position inside the tile, relative to the top left corner
	Result.Y = Y - ( (real32)Result.TileY * (real32)World->TileSideInPixels );

	// Assert relative pos well within the bounds of a tile coordinates
	Assert( Result.X >= 0 );
	Assert( Result.Y >= 0 );
	Assert( Result.X < World->TileSideInPixels );
	Assert( Result.Y < World->TileSideInPixels );

	// wrap around, going left makes you enter right in the previous tilemap
	if ( Result.TileX < 0 )
	{
		Result.TileX = World->CountX + Result.TileX;
		--Result.TileMapX;
	}
	if ( Result.TileY < 0 )
	{
		Result.TileY = World->CountY + Result.TileY;
		--Result.TileMapY;
	}
	if ( Result.TileX >= World->CountX )
	{
		Result.TileX = Result.TileX - World->CountX;
		++Result.TileMapX;
	}
	if ( Result.TileY >= World->CountY )
	{
		Result.TileY = Result.TileY - World->CountY;
		++Result.TileMapY;
	}

	return Result;
}

internal bool32
IsWorldPointEmpty( world *World, raw_position RawPos )
{
	canonical_position CanPos = GetCanonicalPosition( World, RawPos );
	tile_map *TileMap = GetTileMap( World, CanPos.TileMapX, CanPos.TileMapY );
	return IsTileMapPointEmpty( World, TileMap, CanPos.TileX, CanPos.TileY );
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == ArrayCount(Input->Controllers[0].Buttons) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );

	#define TILE_MAP_COUNT_X 17
	#define TILE_MAP_COUNT_Y 9
	uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0 },
		{ 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1 },
		{ 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
	};

	uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	};

	uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1 },
		{ 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1 },
		{ 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
	};

	uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	};

	

	world World;
	World.TileMapCountX = 2;
	World.TileMapCountY = 2;
	World.CountX = TILE_MAP_COUNT_X;
	World.CountY = TILE_MAP_COUNT_Y;

	World.TileSideInMeters = 1.4f;
	World.TileSideInPixels = 60; 

	World.UpperLeftX = -World.TileSideInPixels / 2.0f;
	World.UpperLeftY = 0.0f;

	real32 PlayerWidth = 0.75f * World.TileSideInPixels;
	real32 PlayerHeight = (real32)World.TileSideInPixels;

	tile_map TileMaps[2][2];
	TileMaps[0][0].Tiles = (uint32*)Tiles00;
	TileMaps[0][1].Tiles = (uint32*)Tiles10;
	TileMaps[1][0].Tiles = (uint32*)Tiles01;
	TileMaps[1][1].Tiles = (uint32*)Tiles11;

	World.TileMaps = (tile_map*)TileMaps;

	// state takes all the place in the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage; 
	if ( !Memory->IsInitialized ) 
	{
		GameState->PlayerX = 480.0f;
		GameState->PlayerY = 360.0f;

		Memory->IsInitialized = true;
	}

    for ( int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex ) 
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
					dPlayerY = -1.0f;
				}
				if ( Controller->MoveDown.EndedDown ){
					dPlayerY = 1.0f;
				}
				if ( Controller->MoveRight.EndedDown ){
					dPlayerX = 1.0f;
				}
				if ( Controller->MoveLeft.EndedDown ){
					dPlayerX = -1.0f;
				}
				real32 Speed = 128.0f;

				real32 NewPlayerX = GameState->PlayerX + Input->dtForFrame * Speed * dPlayerX;
				real32 NewPlayerY = GameState->PlayerY + Input->dtForFrame * Speed * dPlayerY;

				raw_position PlayerPos = { GameState->PlayerTileMapX, GameState->PlayerTileMapY, NewPlayerX, NewPlayerY };
				raw_position PlayerLeft = { GameState->PlayerTileMapX, GameState->PlayerTileMapY, NewPlayerX - 0.5f * PlayerWidth, NewPlayerY };
				raw_position PlayerRight = { GameState->PlayerTileMapX, GameState->PlayerTileMapY, NewPlayerX + 0.5f * PlayerWidth, NewPlayerY };

				// test bottom left, middle and right points.
				if (IsWorldPointEmpty( &World, PlayerLeft ) &&
					IsWorldPointEmpty( &World, PlayerRight ) &&
					IsWorldPointEmpty( &World, PlayerPos ) )
				{
					canonical_position CanPos = GetCanonicalPosition( &World, PlayerPos );

					GameState->PlayerTileMapX = CanPos.TileMapX;
					GameState->PlayerTileMapY = CanPos.TileMapY;

					GameState->PlayerX = World.UpperLeftX + World.TileSideInPixels * CanPos.TileX + CanPos.X;
					GameState->PlayerY = World.UpperLeftY + World.TileSideInPixels * CanPos.TileY + CanPos.Y;
				}
			}
        }
    }

	tile_map *TileMap = GetTileMap( &World, GameState->PlayerTileMapX, GameState->PlayerTileMapY );
	Assert( TileMap );


	// Clear screen
	DrawRectangle( pBuffer, 0.0f, 0.0f, (real32)pBuffer->Width, (real32)pBuffer->Height, 1.0f, 0.0f, 1.0f );

	// TileMap
	for ( int Row = 0; Row < World.CountY; ++Row ) {
		for ( int Column = 0; Column < World.CountX; ++Column ) {
			uint32 TileID = GetTileValueUnchecked( &World, TileMap, Column, Row );
			real32 Gray = 0.5;
			if ( TileID == 1 ) {
				Gray = 1.0f;
			}
			
			real32 MinX = World.UpperLeftX + (real32)( Column * World.TileSideInPixels );
			real32 MinY = World.UpperLeftY + (real32)( Row * World.TileSideInPixels );
			real32 MaxX = MinX + World.TileSideInPixels;
			real32 MaxY = MinY + World.TileSideInPixels;
			DrawRectangle( pBuffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray );
		}
	}

	// Draw player
	real32 PlayerR = 0.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 1.0f;
	
	real32 PlayerLeft = GameState->PlayerX - ( 0.5f * PlayerWidth );
	real32 PlayerTop = GameState->PlayerY - PlayerHeight;
	DrawRectangle( pBuffer, PlayerLeft, PlayerTop, PlayerLeft + PlayerWidth, PlayerTop + PlayerHeight, PlayerR, PlayerG, PlayerB );
}

extern "C" GAME_GET_SOUND_SAMPLES( GameGetSoundSamples )
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	GameOutputSound( GameState, pSoundBuffer, 512 );
}



// old stuff
#if 0
internal void
RenderWeirdGradient( game_offscreen_buffer *pBuffer, int XOffset, int YOffset )
{
	uint8 *Row = (uint8 *)pBuffer->Memory;
	for ( int Y = 0; Y < pBuffer->Height; ++Y ) {
		uint32 *Pixel = (uint32*)Row;
		for ( int X = 0; X < pBuffer->Width; ++X ) {
			// Memory:  BB GG RR xx
			// Register xx RR GG BB (little endian)
			uint8 RR = (uint8)( X + XOffset );
			uint8 GG = (uint8)( Y + YOffset );
			//*Pixel++ = (RR << 16) | (GG << 8);
			*Pixel++ = ( RR << 8 ) | ( GG << 16 );
		}
		Row += pBuffer->Pitch;
	}

}
#endif

