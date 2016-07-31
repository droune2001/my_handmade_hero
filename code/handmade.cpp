#include "handmade.h"

internal void
GameOutputSound( game_state *GameState, game_sound_output_buffer *pSoundBuffer, int ToneHz )
{
	local_persist real32 tSine;
	int16 ToneVolume = 1000;
	int WavePeriod = pSoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = pSoundBuffer->Samples;
	for ( int SampleIndex = 0; SampleIndex < pSoundBuffer->SampleCount; ++SampleIndex ) {
		real32 SineValue = sinf( GameState->tSine );
		int16 SampleValue = (int16)(SineValue * ToneVolume ); // scale sin
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

		Memory->IsInitialized = true;
	}

    for ( int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex ) {
        game_controller_input *Controller = GetController( Input, ControllerIndex );
        if ( Controller->IsConnected ) {
            if ( Controller->IsAnalog ) {
                GameState->ToneHz = 256 + (int)(128.0f * (Controller->StickAverageX));
                GameState->XOffset += (int)(4.0f * Controller->StickAverageX);
                GameState->YOffset -= (int)(4.0f * Controller->StickAverageY);
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

                GameState->XOffset += 4 * ( Controller->MoveLeft.EndedDown ? 1 : 0 );
                GameState->XOffset -= 4 * ( Controller->MoveRight.EndedDown ? 1 : 0 );
                GameState->YOffset -= 4 * ( Controller->MoveUp.EndedDown ? 1 : 0 );
                GameState->YOffset += 4 * ( Controller->MoveDown.EndedDown ? 1 : 0 );
            }
            
            if ( Controller->ActionDown.EndedDown ) { 
                GameState->XOffset += 1;
            }
        }
    }
	
	RenderWeirdGradient( pBuffer, GameState->XOffset, GameState->YOffset );
}

extern "C" GAME_GET_SOUND_SAMPLES( GameGetSoundSamples )
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	GameOutputSound( GameState, pSoundBuffer, GameState->ToneHz );
}
//
//#if HANDMADE_WIN32
//#include <windows.h>
//BOOL WINAPI DllMain(
//	_In_ HINSTANCE hinstDLL,
//	_In_ DWORD     fdwReason,
//	_In_ LPVOID    lpvReserved
//	)
//{
//	return TRUE;
//}
//#endif

