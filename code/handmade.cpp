#include "handmade.h"

internal void
GameOutputSound( game_state *GameState, game_sound_output_buffer *pSoundBuffer, int ToneHz )
{
	local_persist real32 tSine;
	int16 ToneVolume = 1000;
	int WavePeriod = pSoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = pSoundBuffer->Samples;
	for ( int SampleIndex = 0; SampleIndex < pSoundBuffer->SampleCount; ++SampleIndex ) {
#if 1
		real32 SineValue = sinf( GameState->tSine );
		int16 SampleValue = (int16)(SineValue * ToneVolume ); // scale sin
#else
		int16 SampleValue = 0; // be quiet!
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;

		if ( GameState->tSine > 2.0f * Pi32 ) {
			GameState->tSine -= 2.0f * Pi32;
		}
	}
}

internal void 
RenderWeirdGradient( game_offscreen_buffer *pBuffer, int XOffset, int YOffset )
{
	uint8 *Row = (uint8 *)pBuffer->Memory;
	for (int Y = 0; Y < pBuffer->Height; ++Y) {
		uint32 *Pixel = (uint32*)Row;
		for (int X = 0; X < pBuffer->Width; ++X) {
			// Memory:  BB GG RR xx
			// Register xx RR GG BB (little endian)
			uint8 RR = (uint8)(X + XOffset);
			uint8 GG = (uint8)(Y + YOffset);
			//*Pixel++ = (RR << 16) | (GG << 8);
			*Pixel++ = ( RR << 8 ) | ( GG << 16 );
		}
		Row += pBuffer->Pitch;
	}

}

internal void
RenderPlayer( game_offscreen_buffer *pBuffer, int PlayerX, int PlayerY )
{	
	int PlayerSize = 10;

	if ( PlayerX < 0 ) { PlayerX = 0; }
	if ( PlayerX > pBuffer->Width - PlayerSize - 1 ) { PlayerX = pBuffer->Width - PlayerSize - 1; }
	if ( PlayerY < 0 ) { PlayerY = 0; }
	if ( PlayerY > pBuffer->Height - PlayerSize - 1 ) { PlayerY = pBuffer->Height - PlayerSize - 1; }

	int Color = 0xffffffff;
	int Top = PlayerY;
	int Bottom = PlayerY + 10;
	for ( int X = PlayerX; X < PlayerX + 10; ++X ) {
		uint8 *Pixel = (uint8*)pBuffer->Memory 
			+ X * pBuffer->BytesPerPixel 
			+ Top * pBuffer->Pitch;
		for ( int Y = Top; Y < Bottom; ++Y )
		{
			*( (uint32*)Pixel ) = Color;
			Pixel += pBuffer->Pitch;
		}
	}
}

extern "C" GAME_UPDATE_AND_RENDER( GameUpdateAndRender )
{
	// check if we did not forget to update the union "array of buttons" / "struct with every buttons"
	Assert( (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == ArrayCount(Input->Controllers[0].Buttons) );

	// check if we did not let our game state grow above the reserved memory.
	Assert( sizeof( game_state ) <= Memory->PermanentStorageSize );

	// state takes all the place in the permanent storage.
	game_state *GameState = (game_state *)Memory->PermanentStorage; 
	if ( !Memory->IsInitialized ) {

		char *Filename = __FILE__;
		debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile( Filename );
		if ( File.ContentsSize ) {
			Memory->DEBUGPlatformWriteEntireFile( ".\\test.out", File.ContentsSize, File.Contents );
			Memory->DEBUGPlatformFreeFileMemory( File.Contents );
		}

		GameState->ToneHz = 256;
		GameState->tSine = 0.0f;
		GameState->XOffset = 0;
		GameState->YOffset = 0;

		GameState->PlayerX = 100;
		GameState->PlayerY = 100;

		Memory->IsInitialized = true;
	}

    for ( int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex ) {
        game_controller_input *Controller = GetController( Input, ControllerIndex );
        if ( Controller->IsConnected ) {
            if ( Controller->IsAnalog ) {
                GameState->ToneHz = 256 + (int)(128.0f * (Controller->StickAverageX));

                GameState->XOffset += (int)(4.0f * Controller->StickAverageX);
                GameState->YOffset -= (int)(4.0f * Controller->StickAverageY);

				GameState->PlayerX += (int)( 4.0f * Controller->StickAverageX );
				GameState->PlayerY -= (int)( 4.0f * Controller->StickAverageY + sinf( GameState->tJump ) );
            } else {
                
				int ToneHz = GameState->ToneHz;
                if ( Controller->MoveUp.EndedDown ) {
                    ToneHz += 5;
                }
                if ( Controller->MoveDown.EndedDown ) {
                    ToneHz -= 5;	
                }
				if ( ToneHz <= 1 ){
					ToneHz = 1;
				}
				if ( ToneHz >= 512 ) {
					ToneHz = 512;
				}
				GameState->ToneHz = ToneHz;

                //GameState->XOffset += 4 * ( Controller->MoveLeft.EndedDown ? 1 : 0 );
                //GameState->XOffset -= 4 * ( Controller->MoveRight.EndedDown ? 1 : 0 );
                //GameState->YOffset -= 4 * ( Controller->MoveUp.EndedDown ? 1 : 0 );
                //GameState->YOffset += 4 * ( Controller->MoveDown.EndedDown ? 1 : 0 );

				GameState->PlayerX += (int)( 10 * ( Controller->MoveRight.EndedDown ? 1 : 0 ) );
				GameState->PlayerX += (int)( 10 * ( Controller->MoveLeft.EndedDown ? -1 : 0 ) );
				GameState->PlayerY += (int)( 10 * ( Controller->MoveUp.EndedDown ? -1 : 0 ) );
				GameState->PlayerY += (int)( 10 * ( Controller->MoveDown.EndedDown ? 1 : 0 ) );

				// jump
				if ( GameState->tJump > 0.0f ) {
					GameState->PlayerY += (int)( 10.0f * sinf( Pi32 * GameState->tJump ) );
				}
            }
            
            if ( Controller->ActionDown.EndedDown ) { 
				GameState->tJump = 2.0f;
            }
			GameState->tJump -= 2.0f * 0.033f;
        }
    }
	
	RenderWeirdGradient( pBuffer, GameState->XOffset, GameState->YOffset );
	RenderPlayer( pBuffer, GameState->PlayerX, GameState->PlayerY );
}

extern "C" GAME_GET_SOUND_SAMPLES( GameGetSoundSamples )
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	GameOutputSound( GameState, pSoundBuffer, GameState->ToneHz );
}
