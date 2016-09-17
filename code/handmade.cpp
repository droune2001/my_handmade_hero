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

inline tile_chunk*
GetTileChunk( world *World, int32 TileChunkX, int32 TileChunkY )
{
	tile_chunk *TileChunk = 0;

	if (( TileChunkX >= 0 ) && ( TileChunkX < World->TileChunkCountX ) &&
		( TileChunkY >= 0 ) && ( TileChunkY < World->TileChunkCountY ) )
	{
		TileChunk = &World->TileChunks[World->TileChunkCountX * TileChunkY + TileChunkX];
	}
	return TileChunk;
}

inline uint32
GetTileValueUnchecked( world *World, tile_chunk *TileChunk, uint32 TileX, uint32 TileY )
{
	Assert( TileChunk );
	Assert( TileX < World->ChunkDim );
	Assert( TileY < World->ChunkDim );

	uint32 TileChunkValue = TileChunk->Tiles[World->ChunkDim * TileY + TileX];
	return TileChunkValue;
}

internal uint32
GetTileValue( world *World, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY )
{
	uint32 TileChunkValue = 0;

	if ( TileChunk ) {
		TileChunkValue = GetTileValueUnchecked( World, TileChunk, TestTileX, TestTileY );
	}

	return TileChunkValue;
}

inline void
RecanonicalizeCoord( world *World, uint32 *Tile, real32 *TileRel )
{
	// overflow/underflow if TileRel goes < 0 or > TileSideInMeters
	// Gives -1, 0 or 1, mostly. Could be 2, -3...
	int32 Offset = FloorReal32ToInt32( *TileRel / World->TileSideInMeters );

	// NOTE(nfauvet): World is toroidal. You exit on one side, your pop up on the other side.

	// go to next Tile, or previous, or dont move.
	*Tile += Offset;
	// Take the remainder of the modified TileRel.
	*TileRel -= (real32)Offset * (real32)World->TileSideInMeters; 
	
	Assert( *TileRel >= 0.0f );
	// Assert relative pos well within the bounds of a tile coordinates
	Assert( *TileRel < World->TileSideInMeters );
}

// Takes a canonical which TileRelX and Y have been messed up with, 
// and RE-canonicalize it.
inline world_position
RecanonicalizePosition( world *World, world_position Pos )
{
	world_position Result = Pos;
	
	RecanonicalizeCoord( World, &Result.AbsTileX, &Result.TileRelX );
	RecanonicalizeCoord( World, &Result.AbsTileY, &Result.TileRelY );

	return Result;
}

internal tile_chunk_position GetChunkPositionFor( world *World, uint32 AbsTileX, uint32 AbsTileY )
{
	tile_chunk_position Result;

	Result.TileChunkX = AbsTileX >> World->ChunkShift;
	Result.TileChunkY = AbsTileY >> World->ChunkShift;
	Result.RelTileX = AbsTileX & World->ChunkMask;
	Result.RelTileY = AbsTileY & World->ChunkMask;

	return Result;
}

internal uint32
GetTileValue( world *World, uint32 AbsTileX, uint32 AbsTileY )
{
	tile_chunk_position ChunkPos = GetChunkPositionFor( World, AbsTileX, AbsTileY );
	tile_chunk *TileChunk = GetTileChunk( World, ChunkPos.TileChunkX, ChunkPos.TileChunkY );
	uint32 TileChunkValue = GetTileValue( World, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY );

	return TileChunkValue;
}

internal bool32
IsWorldPointEmpty( world *World, world_position Pos )
{
	uint32 TileChunkValue = GetTileValue( World, Pos.AbsTileX, Pos.AbsTileY );
	bool32 Empty = ( TileChunkValue == 0 );

	return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == ArrayCount(Input->Controllers[0].Buttons) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );

	#define TILE_MAP_COUNT_X 256
	#define TILE_MAP_COUNT_Y 256
	uint32 TempTiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 },
		{ 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
	};

	world World;
	World.ChunkShift = 8; // 256 x 256 tile chunks
	World.ChunkMask = ( 1 << World.ChunkShift ) - 1; // 0x0100 - 1 = 0x00FF
	World.ChunkDim = 256;

	World.TileChunkCountX = 1;
	World.TileChunkCountY = 1;

	World.TileSideInMeters = 1.4f;
	World.TileSideInPixels = 60; 
	World.MetersToPixels = (real32)World.TileSideInPixels / World.TileSideInMeters;

	float LowerLeftX = World.TileSideInPixels / 2.0f;
	float LowerLeftY = (real32)pBuffer->Height;
	
	real32 PlayerHeight = 1.4f; 
	real32 PlayerWidth = 0.75f * PlayerHeight;

	tile_chunk TileChunk;
	TileChunk.Tiles = (uint32*)TempTiles;
	World.TileChunks = &TileChunk;

	// state takes all the place in the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage; 
	if ( !Memory->IsInitialized ) 
	{
		GameState->PlayerP.AbsTileX = 8;
		GameState->PlayerP.AbsTileY = 5;
		GameState->PlayerP.TileRelX = 0.2f;
		GameState->PlayerP.TileRelY = 0.3f;
#if 0
		GameState->PlayerX = 480.0f;
		GameState->PlayerY = 360.0f;
#endif
		Memory->IsInitialized = true;
	}

    for ( int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex ) 
	{
        game_controller_input *Controller = GetController( Input, ControllerIndex );
        if ( Controller->IsConnected ) 
		{
			// tmp Nico pour scroll map
			static float LLX = LowerLeftX;
			static float LLY = LowerLeftY;

            if ( Controller->IsAnalog ) 
			{
				// map scrolling with sticks!!!
				const float scrolling_speed = 10.0f;
				LLX -= Controller->StickAverageX * scrolling_speed;
				LLY += Controller->StickAverageY * scrolling_speed;
			} 
			else 
			{
				real32 dPlayerX = 0.0f;
				real32 dPlayerY = 0.0f;
				if ( Controller->MoveUp.EndedDown ){
					dPlayerY = 1.0f;
				}
				if ( Controller->MoveDown.EndedDown ){
					dPlayerY = -1.0f;
				}
				if ( Controller->MoveRight.EndedDown ){
					dPlayerX = 1.0f;
				}
				if ( Controller->MoveLeft.EndedDown ){
					dPlayerX = -1.0f;
				}

				real32 Speed = 8.0f; // m/s

				// temporary position
				world_position NewPlayerP = GameState->PlayerP;
				NewPlayerP.TileRelX += Input->dtForFrame * Speed * dPlayerX;
				NewPlayerP.TileRelY += Input->dtForFrame * Speed * dPlayerY;
				NewPlayerP = RecanonicalizePosition( &World, NewPlayerP );
				 
				world_position PlayerLeft = NewPlayerP;
				PlayerLeft.TileRelX -= 0.5f * PlayerWidth;
				//PlayerLeft = RecanonicalizePosition( &World, PlayerLeft );

				world_position PlayerRight = NewPlayerP;
				PlayerRight.TileRelX += 0.5f * PlayerWidth;
				//PlayerRight = RecanonicalizePosition( &World, PlayerRight );

				// test bottom left, middle and right points.
				if (IsWorldPointEmpty( &World, PlayerLeft ) &&
					IsWorldPointEmpty( &World, PlayerRight ) &&
					IsWorldPointEmpty( &World, NewPlayerP ) )
				{
					// if position validated, commit it.
					GameState->PlayerP = NewPlayerP;
				}
			}

			// tmp Nico
			LowerLeftX = LLX;
			LowerLeftY = LLY;
        }
    }

	// Clear screen
	DrawRectangle( pBuffer, 0.0f, 0.0f, (real32)pBuffer->Width, (real32)pBuffer->Height, 1.0f, 0.0f, 1.0f );

	// TileMap
	const uint32 nb_rows_to_draw = 18; // 9
	const uint32 nb_colunns_to_draw = 35; // 17

	for ( uint32 Row = 0; Row < nb_rows_to_draw; ++Row ) {
		for ( uint32 Column = 0; Column < nb_colunns_to_draw; ++Column ) {
			uint32 TileID = GetTileValue( &World, Column, Row );
			real32 Gray = 0.5;
			if ( TileID == 1 ) {
				Gray = 1.0f;
			}
			
			// debug
			if (( Row == GameState->PlayerP.AbsTileY ) &&
				( Column == GameState->PlayerP.AbsTileX ) )
			{
				Gray = 0.0f;
			}
			real32 MinX = LowerLeftX + (real32)( Column * World.TileSideInPixels );
			real32 MinY = LowerLeftY - (real32)( Row * World.TileSideInPixels );
			real32 MaxX = MinX + World.TileSideInPixels;
			real32 MaxY = MinY - World.TileSideInPixels;
			DrawRectangle( pBuffer, MinX, MaxY, MaxX, MinY, Gray, Gray, Gray );
		}
	}

	// Draw player
	real32 PlayerR = 0.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 1.0f;
	
	real32 PlayerLeft = LowerLeftX + World.TileSideInPixels * GameState->PlayerP.AbsTileX +
		World.MetersToPixels * ( GameState->PlayerP.TileRelX - ( 0.5f * PlayerWidth ) );
	real32 PlayerTop = LowerLeftY - World.TileSideInPixels * GameState->PlayerP.AbsTileY -
		World.MetersToPixels * ( GameState->PlayerP.TileRelY + PlayerHeight );

	DrawRectangle( pBuffer, 
		PlayerLeft, PlayerTop, 
		PlayerLeft + World.MetersToPixels * PlayerWidth,
		PlayerTop + World.MetersToPixels * PlayerHeight,
		PlayerR, PlayerG, PlayerB );
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

