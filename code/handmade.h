#ifndef HANDMADE_H
#define HANDMADE_H

/*
	HANDMADE_INTERNAL:
		0 - Build for public release
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - fast
		1 - slow
*/

#if HANDMADE_SLOW
#	define Assert(Expression) if(!(Expression)){*(int *)0 = 0;}
#else
#	define Assert(Expression)
#endif

inline uint32 SafeTruncateSize32( uint64 Value )
{
	Assert( Value <= 0xFFFFFFFF );
	uint32 Result = (uint32)Value;
	return Result;
}

#define Kilobytes(val) ((val)*1024LL)
#define Megabytes(val) (Kilobytes(val)*1024LL)
#define Gigabytes(val) (Megabytes(val)*1024LL)
#define Terabytes(val) (Gigabytes(val)*1024LL)

#define ArrayCount(tab) (sizeof(tab) / sizeof((tab)[0]))

// -----------------------------------------------------
// Services the platform layer provides to the game.
#if HANDMADE_INTERNAL
/*
	These are not for doing anything in the shipping game - they are
	blocking and the write doesn't protect against lost data!
*/
	struct debug_read_file_result
	{
		uint32 ContentsSize;
		void *Contents;
	};
	internal debug_read_file_result DEBUGPlatformReadEntireFile( char *Filename );
	internal void DEBUGPlatformFreeFileMemory( void *Memory );
	internal bool32 DEBUGPlatformWriteEntireFile( char *Filename, uint32 MemorySize, void *Memory );
#endif

// -----------------------------------------------------
// Services that the game provides to the plafrom layer.

struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
};

// can rebuild the history with just these 2 data
struct game_button_state 
{
	int HalfTransitionCount;
	bool32 EndedDown;
};

struct game_controller_input 
{
    bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;

	union
	{
		game_button_state Buttons[12];
		struct
		{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;

            game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Start;
			game_button_state Back;

			// NOTE(nfauvet): all buttons must be added above this one
			game_button_state Terminator;

		};
	};
};

struct game_input 
{
	game_controller_input Controllers[5]; // 4 manettes et 1 keyboard
};

// accessor just to add an assert.
inline game_controller_input *GetController( game_input *Input, int ControllerIndex ) {
    Assert( ControllerIndex < ArrayCount( Input->Controllers ) );
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return Result;
}

struct game_memory
{
	uint64 PermanentStorageSize;
	void *PermanentStorage; // REQUIRED to be cleared to zero at startup (every platform have to do it).
	
	uint64 TransientStorageSize;
	void *TransientStorage; // scratch buffer

	bool32 IsInitialized;
};

// timing
// input controller
// bitmap to output image
// buffer to output sound
internal void 
GameUpdateAndRender( 
	game_memory *Memory,
	game_input *Input,
	game_offscreen_buffer *pBuffer
);

internal void
GameGetSoundSamples(
	game_memory *Memory,
	game_sound_output_buffer *pSoundBuffer
);

//
//
//
struct game_state
{
	int ToneHz;
	int XOffset;
	int YOffset;
};

#endif // HANDMADE_H
