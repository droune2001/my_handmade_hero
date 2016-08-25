#include "handmade.h"

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

inline internal
int32 RoundReal32ToInt32( real32 Real32 )
{
	int32 Result = (int32)( Real32 + 0.5f );
	return Result;
}

inline internal
uint32 RoundReal32ToUInt32( real32 Real32 )
{
	int32 Result = (uint32)( Real32 + 0.5f );
	return Result;
}

inline internal
int32 TruncateReal32ToInt32( real32 Real32 )
{
	int32 Result = (int32)Real32;
	return Result;
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
GetTileValueUnchecked( tile_map *TileMap, int32 TileX, int32 TileY )
{
	uint32 TileMapValue = TileMap->Tiles[TileMap->CountX * TileY + TileX];
	return TileMapValue;
}

internal bool32
IsTileMapPointEmpty( tile_map *TileMap, real32 TestX, real32 TestY )
{
	bool32 Empty = false;

	int PlayerTileX = TruncateReal32ToInt32( ( TestX - TileMap->UpperLeftX ) / TileMap->TileWidth );
	int PlayerTileY = TruncateReal32ToInt32( ( TestY - TileMap->UpperLeftY ) / TileMap->TileHeight );

	bool32 IsValid = false;
	if ( ( PlayerTileX >= 0 ) && ( PlayerTileX < TileMap->CountX ) &&
		( PlayerTileY >= 0 ) && ( PlayerTileY < TileMap->CountY ) )
	{
		uint32 TileMapValue = GetTileValueUnchecked( TileMap, PlayerTileX, PlayerTileY );
		Empty = ( TileMapValue == 0 );
	}

	return Empty;
}

internal bool32
IsWorldPointEmpty( world *World, int32 TileMapX, int32 TileMapY, real32 TestX, real32 TestY )
{
	bool32 Empty = false;

	tile_map *TileMap = GetTileMap( World, TileMapX, TileMapY );
	if ( TileMap ) 
	{
		int PlayerTileX = TruncateReal32ToInt32( ( TestX - TileMap->UpperLeftX ) / TileMap->TileWidth );
		int PlayerTileY = TruncateReal32ToInt32( ( TestY - TileMap->UpperLeftY ) / TileMap->TileHeight );

		if ( ( PlayerTileX >= 0 ) && ( PlayerTileX < TileMap->CountX ) &&
			( PlayerTileY >= 0 ) && ( PlayerTileY < TileMap->CountY ) )
		{
			uint32 TileMapValue = GetTileValueUnchecked( TileMap, PlayerTileX, PlayerTileY );
			Empty = ( TileMapValue == 0 );
		}
	}
	return Empty;
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

	tile_map TileMaps[2][2];
	TileMaps[0][0].CountX = TILE_MAP_COUNT_X;
	TileMaps[0][0].CountY = TILE_MAP_COUNT_Y;
	TileMaps[0][0].TileWidth = 60;
	TileMaps[0][0].TileHeight = 60;
	TileMaps[0][0].UpperLeftX = -30.0f;
	TileMaps[0][0].UpperLeftY = 0.0f;
	TileMaps[0][0].Tiles = (uint32*)Tiles00;

	TileMaps[0][1] = TileMaps[0][0];
	TileMaps[0][1].Tiles = (uint32*)Tiles01;

	TileMaps[1][0] = TileMaps[0][0];
	TileMaps[1][0].Tiles = (uint32*)Tiles10;

	TileMaps[1][1] = TileMaps[0][0];
	TileMaps[1][1].Tiles = (uint32*)Tiles11;

	tile_map *TileMap = &TileMaps[0][0];

	world World;
	World.TileMapCountX = 2;
	World.TileMapCountY = 2;
	World.TileMaps = (tile_map*)TileMaps;


	real32 PlayerWidth = 0.75f * TileMap->TileWidth;
	real32 PlayerHeight = (real32)TileMap->TileHeight;

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

				

				// test bottom left, middle and right points.
				if (IsTileMapPointEmpty( TileMap, NewPlayerX - 0.5f * PlayerWidth, NewPlayerY ) &&
					IsTileMapPointEmpty( TileMap, NewPlayerX + 0.5f * PlayerWidth, NewPlayerY )  &&
					IsTileMapPointEmpty( TileMap, NewPlayerX, NewPlayerY ) )
				{
					GameState->PlayerX = NewPlayerX;
					GameState->PlayerY = NewPlayerY;
				}
			}
        }
    }

	

	// Clear screen
	DrawRectangle( pBuffer, 0.0f, 0.0f, (real32)pBuffer->Width, (real32)pBuffer->Height, 1.0f, 0.0f, 1.0f );

	// TileMap
	for ( int Row = 0; Row < TileMap->CountY; ++Row ) {
		for ( int Column = 0; Column < TileMap->CountX; ++Column ) {
			uint32 TileID = GetTileValueUnchecked( TileMap, Column, Row );
			real32 Gray = 0.5;
			if ( TileID == 1 ) {
				Gray = 1.0f;
			}
			
			real32 MinX = TileMap->UpperLeftX + (real32)( Column * TileMap->TileHeight );
			real32 MinY = TileMap->UpperLeftY + (real32)( Row * TileMap->TileWidth );
			real32 MaxX = MinX + TileMap->TileWidth;
			real32 MaxY = MinY + TileMap->TileHeight;
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

