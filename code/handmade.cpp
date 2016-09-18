#include "handmade.h"
#include "handmade_tile.cpp"

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

internal void
InitializeArena( memory_arena *Arena, memory_index Size, uint8 *Base )
{
	Arena->Size = Size;
	Arena->Base = Base;
	Arena->Used = 0;
}

#define PushStruct(Arena, type) (type*)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type*)PushSize_(Arena, (Count)*sizeof(type))

internal void*
PushSize_( memory_arena *Arena, memory_index Size )
{
	Assert( ( Arena->Used + Size ) <= Arena->Size );
	// pointer to the beginning of the free space of the arena
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;
	return Result;
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == ArrayCount(Input->Controllers[0].Buttons) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );
	
	real32 PlayerHeight = 1.4f;
	real32 PlayerWidth = 0.75f * PlayerHeight;

	float LowerLeftX = 60 / 2.0f; //TileMap->TileSideInPixels / 2.0f;
	float LowerLeftY = (real32)pBuffer->Height;

	// game_state occupies the beginning of the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage; 

	if ( !Memory->IsInitialized ) 
	{
		GameState->PlayerP.AbsTileX = 8;
		GameState->PlayerP.AbsTileY = 5;
		GameState->PlayerP.TileRelX = 0.2f;
		GameState->PlayerP.TileRelY = 0.3f;

		// WorldArena comes just after the game_state chunk.
		// Uses all the memory left.
		InitializeArena( &GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
			(uint8*)Memory->PermanentStorage + sizeof(game_state)  );

		GameState->World = PushStruct( &GameState->WorldArena, world );
		world *World = GameState->World;
		World->TileMap = PushStruct( &GameState->WorldArena, tile_map );

		tile_map *TileMap = World->TileMap;
		TileMap->ChunkShift = 4; // 16 x 16 tile chunks
		TileMap->ChunkMask = ( 1 << TileMap->ChunkShift ) - 1; // 0x0010 - 1 = 0x000F
		TileMap->ChunkDim = ( 1 << TileMap->ChunkShift ); // 16

		TileMap->TileChunkCountX = 128;
		TileMap->TileChunkCountY = 128;

		// allocate all chunks
		TileMap->TileChunks = PushArray( &GameState->WorldArena, 
			TileMap->TileChunkCountX * TileMap->TileChunkCountY, tile_chunk );

		// allocate all tiles in each chunk
		for ( uint32 Y = 0; Y < TileMap->TileChunkCountY; ++Y ) {
			for ( uint32 X = 0; X < TileMap->TileChunkCountX; ++X ) {
				TileMap->TileChunks[Y * TileMap->TileChunkCountX + X].Tiles = 
					PushArray( &GameState->WorldArena, TileMap->ChunkDim * TileMap->ChunkDim, uint32 );
			}
		}

		TileMap->TileSideInMeters = 1.4f;
		TileMap->TileSideInPixels = 60;
		TileMap->MetersToPixels = (real32)TileMap->TileSideInPixels / TileMap->TileSideInMeters;

		const uint32 TilesPerWidth = 17;
		const uint32 TilesPerHeight = 9;

		for ( uint32 ScreenY = 0; ScreenY < 32; ++ScreenY ) {
			for ( uint32 ScreenX = 0; ScreenX < 32; ++ScreenX ) {
				for ( uint32 TileY = 0; TileY < TilesPerHeight; ++TileY ) {
					for ( uint32 TileX = 0; TileX < TilesPerWidth; ++TileX ) {

						uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
						uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

						SetTileValue( &GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY,
							( ( TileX == TileY ) && ( TileY % 2 ) ) ? 1 : 0 );

					}
				}
			}
		}



		Memory->IsInitialized = true;
	}

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

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

				real32 Speed = 5.0f; // m/s
				if ( Controller->ActionUp.EndedDown ){
					Speed = 10.0f;
				}

				// temporary position
				tile_map_position NewPlayerP = GameState->PlayerP;
				NewPlayerP.TileRelX += Input->dtForFrame * Speed * dPlayerX;
				NewPlayerP.TileRelY += Input->dtForFrame * Speed * dPlayerY;
				NewPlayerP = RecanonicalizePosition( TileMap, NewPlayerP );
				 
				tile_map_position PlayerLeft = NewPlayerP;
				PlayerLeft.TileRelX -= 0.5f * PlayerWidth;
				PlayerLeft = RecanonicalizePosition( TileMap, PlayerLeft );

				tile_map_position PlayerRight = NewPlayerP;
				PlayerRight.TileRelX += 0.5f * PlayerWidth;
				PlayerRight = RecanonicalizePosition( TileMap, PlayerRight );

				// test bottom left, middle and right points.
				if (IsTileMapPointEmpty( TileMap, PlayerLeft ) &&
					IsTileMapPointEmpty( TileMap, PlayerRight ) &&
					IsTileMapPointEmpty( TileMap, NewPlayerP ) )
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

	real32 ScreenCenterX = 0.5f * (real32)pBuffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)pBuffer->Height;

	for ( int32 RelRow = -10; RelRow < 10; ++RelRow ) {
		for ( int32 RelColumn = -20; RelColumn < 20; ++RelColumn ) {

			uint32 Column = GameState->PlayerP.AbsTileX + RelColumn;
			uint32 Row = GameState->PlayerP.AbsTileY + RelRow;
			uint32 TileID = GetTileValue( TileMap, Column, Row );
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
			
			real32 CenterX = ScreenCenterX - TileMap->MetersToPixels * GameState->PlayerP.TileRelX + (real32)( RelColumn * TileMap->TileSideInPixels ); // LowerLeftX +
			real32 CenterY = ScreenCenterY + TileMap->MetersToPixels * GameState->PlayerP.TileRelY - (real32)( RelRow * TileMap->TileSideInPixels ); // LowerLeftY - 
			real32 MinX = CenterX - 0.5f * TileMap->TileSideInPixels;
			real32 MinY = CenterY - 0.5f * TileMap->TileSideInPixels;
			real32 MaxX = CenterX + 0.5f * TileMap->TileSideInPixels;
			real32 MaxY = CenterY + 0.5f * TileMap->TileSideInPixels;
			DrawRectangle( pBuffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray );
		}
	}

	// Draw player
	real32 PlayerR = 0.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 1.0f;

	//real32 PlayerLeft = LowerLeftX + TileMap->TileSideInPixels * GameState->PlayerP.AbsTileX +
	//	TileMap->MetersToPixels * ( GameState->PlayerP.TileRelX - ( 0.5f * PlayerWidth ) );
	//real32 PlayerTop = LowerLeftY - TileMap->TileSideInPixels * GameState->PlayerP.AbsTileY -
	//	TileMap->MetersToPixels * ( GameState->PlayerP.TileRelY + PlayerHeight );

	real32 PlayerLeft = ScreenCenterX - TileMap->MetersToPixels * ( 0.5f * PlayerWidth );
	real32 PlayerTop = ScreenCenterY - TileMap->MetersToPixels * PlayerHeight;

	DrawRectangle( pBuffer, 
		PlayerLeft, PlayerTop, 
		PlayerLeft + TileMap->MetersToPixels * PlayerWidth,
		PlayerTop + TileMap->MetersToPixels * PlayerHeight,
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

