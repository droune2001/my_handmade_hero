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

#include <math.h> // TODO: implement sine ourselves
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.1415926535f

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef float real32;
typedef double real64;



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

// * passed through each execution chain in the game
// * when you use operating system services, you are going
//   to need one of these thread_context (not on windows).
// * passed to platform calls.
// * Because not all platforms give you good information on the current thread
//   when doing multi-threading.
struct thread_context
{
	int Placeholder;
};

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

#define DEBUG_PLATFORM_FREE_FILE_MEMORY( name ) void name( thread_context *Thread, void *Memory )
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY( debug_platform_free_file_memory );

#define DEBUG_PLATFORM_READ_ENTIRE_FILE( name ) debug_read_file_result name( thread_context *Thread, char *Filename )
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE( debug_platform_read_entire_file );

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE( name ) bool32 name( thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory )
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE( debug_platform_write_entire_file );

#endif

// -----------------------------------------------------
// Services that the game provides to the plafrom layer.

struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
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
	// for debugging purposes
	game_button_state MouseButtons[5];
	int32 MouseX, MouseY, MouseZ;

	// TODO(nfauvet): insert clock values here.
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
	bool32 IsInitialized;

	uint64 PermanentStorageSize;
	void *PermanentStorage; // REQUIRED to be cleared to zero at startup (every platform have to do it).
	
	uint64 TransientStorageSize;
	void *TransientStorage; // scratch buffer

	debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER( name ) void name( thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *pBuffer )
typedef GAME_UPDATE_AND_RENDER( game_update_and_render );

#define GAME_GET_SOUND_SAMPLES( name ) void name( thread_context *Thread, game_memory *Memory, game_sound_output_buffer *pSoundBuffer )
typedef GAME_GET_SOUND_SAMPLES( game_get_sound_samples );

//
//
//
struct game_state
{
	int ToneHz;
	int XOffset;
	int YOffset;

	real32 tSine;

	real32 tJump;
	int PlayerX;
	int PlayerY;
};

#endif // HANDMADE_H
