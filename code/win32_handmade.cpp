/*
	TODO(nfauvet):

	* Saved game locations
	* Getting a handle to our own executable file
	* Asset loading path
	* Threading (launch a thread)
	* Raw Input (multiple keyboards)
	* Sleep/timeBeginPeriod
	* ClipCursor() for multimonitor support
	* Fullscreen support
	* WM_SETCURSOR (cursor visibility)
	* QueryCancelAutoplay
	* WM_ACTIVATEAPP (for when we are not the active application)
	* Blit seed improvement (BitBlt)
	* Hardware acceleration (OpenGL/DirectX)
	* GetKeyboardLayout (for French keyboards, international WSAD support)
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

// "unity" compilation, all in one.
#include "handmade.cpp"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

#include "win32_handmade.h"

global_variable bool32 GlobalRunning; // no need to init to 0 or false, default behavior, saves time.
global_variable bool32 GlobalPause = false;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER pGlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;

// IO
internal debug_read_file_result DEBUGPlatformReadEntireFile( char *Filename )
{
	debug_read_file_result Result = {};
	HANDLE FileHandle = ::CreateFileA(	Filename, GENERIC_READ,	FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	if ( FileHandle != INVALID_HANDLE_VALUE ) {
		LARGE_INTEGER FileSize;
		if ( ::GetFileSizeEx( FileHandle, &FileSize ) )	{
			Result.Contents = ::VirtualAlloc( 0, FileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
			if ( Result.Contents ) {
				// NOTE: FileSize.QuadPart is 64bits, but ReadFile takes a 32bits. 
				// We cant read a file larger than 4Gb with ReadFile. Check it.
				uint32 FileSize32 = SafeTruncateSize32( FileSize.QuadPart );
				DWORD BytesRead;
				if ( ::ReadFile( FileHandle, Result.Contents, FileSize32, &BytesRead, 0 ) 
					 && ( BytesRead == FileSize32 ) ) {
					// SUCCESS
					Result.ContentsSize = FileSize32;
				} else {
					// FAIL - Free
					DEBUGPlatformFreeFileMemory( Result.Contents );
					Result.Contents = 0;
					Result.ContentsSize = 0;
				}
			} else {
				// FAIL
			}

		} else {
			// FAIL
		}

		if ( !::CloseHandle( FileHandle ) ) {
			// FAIL
		}
	}

	return Result;
}

internal void DEBUGPlatformFreeFileMemory( void *Memory )
{
	if ( Memory ) {
		::VirtualFree( Memory, 0, MEM_RELEASE );
	}
}

internal bool32 DEBUGPlatformWriteEntireFile( char *Filename, uint32 MemorySize, void *Memory )
{
	bool32 Result = 0;

	HANDLE FileHandle = ::CreateFileA( Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0 );
	if ( FileHandle != INVALID_HANDLE_VALUE ) {
		DWORD BytesWritten;
		if ( ::WriteFile( FileHandle, Memory, MemorySize, &BytesWritten, 0 ) ) {
			// SUCCESS
			Result = ( BytesWritten == MemorySize );
		} else {
			// FAIL
		}
		
		if ( !::CloseHandle( FileHandle ) ) {
			// FAIL
		}
	}

	return Result;
}

// XInput

#define X_INPUT_GET_STATE( name ) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_STATE* pState )
typedef X_INPUT_GET_STATE( x_input_get_state );
X_INPUT_GET_STATE( XInputGetStateStub )
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

#define X_INPUT_SET_STATE( name ) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_VIBRATION* pVibration )
typedef X_INPUT_SET_STATE( x_input_set_state );
X_INPUT_SET_STATE( XInputSetStateStub )
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput( void )
{
	HMODULE hXInputLibrary = ::LoadLibraryA( "xinput1_4.dll" );
	if ( !hXInputLibrary ) {
		// TODO: log
		hXInputLibrary = ::LoadLibraryA( "xinput1_3.dll" );
		if ( !hXInputLibrary ) {
			// TODO: log
			hXInputLibrary = ::LoadLibraryA( "xinput9_1_0.dll" );
		}
	}

	if ( hXInputLibrary ) {
		XInputGetState = (x_input_get_state *) ::GetProcAddress( hXInputLibrary, "XInputGetState" );
		XInputSetState = (x_input_set_state *) ::GetProcAddress( hXInputLibrary, "XInputSetState" );
	} else {
		// TODO: log
	}
}

// DirectSound

#define DIRECT_SOUND_CREATE( name ) HRESULT WINAPI name( LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter )
typedef DIRECT_SOUND_CREATE( d_sound_create );

internal void Win32InitDSound( HWND hWnd, int32 iSamplesPerSecond, int32 iBufferSize )
{
	// load lib
	HMODULE hDSoundLibrary = ::LoadLibraryA( "dsound.dll" ); // dsound3d.dll ??
	if ( hDSoundLibrary ) {
		// get a directsound object
		d_sound_create *DirectSoundCreate = ( d_sound_create* ) ::GetProcAddress( hDSoundLibrary, "DirectSoundCreate" );

		LPDIRECTSOUND pDirectSound;
		if ( DirectSoundCreate && SUCCEEDED( DirectSoundCreate( 0, &pDirectSound, 0 ) ) ) {

			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = iSamplesPerSecond;
			WaveFormat.wBitsPerSample = 16; // 16 bits audio
			WaveFormat.nBlockAlign = ( WaveFormat.nChannels * WaveFormat.wBitsPerSample ) / 8; // 2 channels, 16bits each, interleaved [RIGHT LEFT] [RIGHT LEFT], divided by bits_in_a_byte
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if ( SUCCEEDED( pDirectSound->SetCooperativeLevel( hWnd, DSSCL_PRIORITY ) ) ) {
				// create a primary buffer
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof( BufferDescription );
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER; // DSBCAPS_GLOBALFOCUS ?
				
				LPDIRECTSOUNDBUFFER pPrimaryBuffer;
				if ( SUCCEEDED( pDirectSound->CreateSoundBuffer( &BufferDescription, &pPrimaryBuffer, 0 ) ) ) {
					
					if ( SUCCEEDED( pPrimaryBuffer->SetFormat( &WaveFormat ) ) ) {
						OutputDebugStringA( "SUCCESS creating the primary sound buffer.\n" );
					} else {
						// TODO: log
						OutputDebugStringA( "Failed SetFormat on PrimaryBuffer\n" );
					}
				} else {
					// TODO: log
					OutputDebugStringA( "Failed Create Primary Buffer\n" );
				}
			} else {
				// TODO: log
				OutputDebugStringA( "Failed SetCooperativeLevel\n" );
			}

			// even if cooperative and primary buffer failed, we can create the real buffer.
			// create a secondary buffer
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof( BufferDescription );
			BufferDescription.dwBufferBytes = iBufferSize;
			BufferDescription.dwFlags = 0; // DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.lpwfxFormat = &WaveFormat;

			if ( SUCCEEDED( pDirectSound->CreateSoundBuffer( &BufferDescription, &pGlobalSecondaryBuffer, 0 ) ) ) {
				OutputDebugStringA("SUCCESS create sound buffer\n");
			} else {
				// TODO: log
				OutputDebugStringA( "Failed Create Secondary Buffer\n" );
			}
		} else {
			// TODO: log
			OutputDebugStringA( "Failed GetProcAddress DirectSoundCreate and Call\n" );
		}
	} else {
		// TODO: log
		OutputDebugStringA( "Failed LoadLibrary dsound.dll\n" );
	}
}

internal win32_window_dimension Win32GetWindowDimension( HWND hWnd )
{
	win32_window_dimension d;

	RECT r;
	::GetClientRect( hWnd, &r );
	d.width = r.right - r.left;
	d.height = r.bottom - r.top;

	return d;
}

internal void Win32ResizeDIBSection( win32_offscreen_buffer *Buffer, int width, int height )
{
	if ( Buffer->Memory ) {
		::VirtualFree( Buffer->Memory, 0, MEM_RELEASE );
	}

	Buffer->Width = width;
	Buffer->Height = height;
	Buffer->BytesPerPixel = 4; 
	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

	Buffer->Info.bmiHeader.biSize = sizeof( Buffer->Info.bmiHeader );
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32; // to align with 32bits
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = Buffer->BytesPerPixel * ( Buffer->Width * Buffer->Height );
	Buffer->Memory = ::VirtualAlloc( 0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

	// clear to black?
}

internal void Win32CopyBufferToWindow( HDC dc, int client_width, int client_height, 
									   win32_offscreen_buffer *pBuffer )
{
	// TODO: aspect ration correction.
	::StretchDIBits( dc, 
					0, 0, client_width, client_height, // FROM
					0, 0, pBuffer->Width, pBuffer->Height, // TO
					pBuffer->Memory,
					&pBuffer->Info,
					DIB_RGB_COLORS, // or DIB_PAL_COLORS for palette
					SRCCOPY );
}

LRESULT CALLBACK Win32MainWindowCallback(	HWND   hwnd,
											UINT   uMsg,
											WPARAM wParam,
											LPARAM lParam )
{
	LRESULT Result = 0; // 0 is most of the time when we handled the message

	switch ( uMsg )
	{
		case WM_SIZE:
		{
		} break;

		case WM_DESTROY:
		{
			// TODO: catch this and try to recreate the window.
			GlobalRunning = false;
		} break;

		case WM_CLOSE:
		{
			// TODO: send message to the user
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
		{
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert( !"Keyboard Input came in through a non-dispath message!" );
		} break; 

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC dc = ::BeginPaint( hwnd, &Paint );
			{
				win32_window_dimension d = Win32GetWindowDimension( hwnd );
				Win32CopyBufferToWindow( dc, d.width, d.height, &GlobalBackBuffer );
			}
			::EndPaint( hwnd, &Paint );
		} break;

		default:
		{
			Result = DefWindowProcA( hwnd, uMsg, wParam, lParam );
		} break;
	}

	return Result;
}



internal void
Win32ClearBuffer( win32_sound_output *SoundOutput )
{
	VOID *pRegion1;
	DWORD iRegion1Size;
	VOID *pRegion2;
	DWORD iRegion2Size;
	if ( SUCCEEDED(pGlobalSecondaryBuffer->Lock( 0, SoundOutput->SecondaryBufferSize,
												 &pRegion1, &iRegion1Size,
												 &pRegion2, &iRegion2Size, 0 )))
	{
		uint8 *DestSample = (uint8 *)pRegion1;
		for ( DWORD ByteIndex = 0; ByteIndex < iRegion1Size; ++ByteIndex) {
			*DestSample++ = 0;
		}

		DestSample = (uint8 *)pRegion2;
		for (DWORD ByteIndex = 0; ByteIndex < iRegion2Size; ++ByteIndex) {
			*DestSample++ = 0;
		}

		pGlobalSecondaryBuffer->Unlock(pRegion1, iRegion1Size, pRegion2, iRegion2Size);
	}
}

internal void Win32FillSoundBuffer( 
	win32_sound_output *SoundOutput, 
	DWORD ByteToLock, DWORD BytesToWrite,
	game_sound_output_buffer *pSourceBuffer )
{
	VOID *pRegion1;
	DWORD iRegion1Size;
	VOID *pRegion2;
	DWORD iRegion2Size;
	if ( SUCCEEDED( pGlobalSecondaryBuffer->Lock( ByteToLock, BytesToWrite,
												  &pRegion1, &iRegion1Size, 
												  &pRegion2, &iRegion2Size, 0 ))) 
	{
		int16 *DestSample = (int16*)pRegion1;
		int16 *SourceSample = pSourceBuffer->Samples;
		DWORD iRegion1SampleCount = iRegion1Size / SoundOutput->BytesPerSample;
		for ( DWORD SampleIndex = 0; SampleIndex < iRegion1SampleCount; ++SampleIndex ) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DestSample = (int16*)pRegion2;
		DWORD iRegion2SampleCount = iRegion2Size / SoundOutput->BytesPerSample;
		for ( DWORD SampleIndex = 0; SampleIndex < iRegion2SampleCount; ++SampleIndex ) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		pGlobalSecondaryBuffer->Unlock( pRegion1, iRegion1Size, pRegion2, iRegion2Size );
	}
}

internal real32
Win32ProcessXinputStickValue( SHORT StickValue, SHORT DeadZoneThreshold  )
{
	// Normalize the portion out of the deadzone.
    real32 ReturnValue = 0;
    if ( StickValue < -DeadZoneThreshold ) {
        ReturnValue = (real32)( StickValue + DeadZoneThreshold ) / ( 32768.0f - DeadZoneThreshold );
    } else if ( StickValue > DeadZoneThreshold ) {
        ReturnValue = (real32)( StickValue - DeadZoneThreshold ) / ( 32767.0f - DeadZoneThreshold );
    }

    return ReturnValue;
}

internal void 
Win32ProcessXInputDigitalButton( DWORD XInputButtonState,
                                game_button_state *OldState,
                                DWORD ButtonBit,
                                game_button_state *NewState )
{
    NewState->EndedDown = ( ( XInputButtonState & ButtonBit ) == ButtonBit );
    NewState->HalfTransitionCount = ( OldState->EndedDown != NewState->EndedDown ) ? 1 : 0;
}

internal void 
Win32ProcessKeyboardMessage( game_button_state *NewState, bool32 bIsDown )
{
    // assert it is different, it should be.
    Assert( NewState->EndedDown != bIsDown );
    NewState->EndedDown = bIsDown;
    ++NewState->HalfTransitionCount;
}

internal void Win32ProcessPendingMessages( game_controller_input *KeyboardController )
{
    // flush queue
    MSG Message;
    while ( ::PeekMessage( &Message, 0, 0, 0, PM_REMOVE ) ) {
        if ( Message.message == WM_QUIT ) {
            GlobalRunning = false;
        }
        
        switch ( Message.message ) {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool bWasDown = ( ( Message.lParam & ( 1 << 30 ) ) != 0 ); // previous state bit
                bool bIsDown = ( ( Message.lParam & ( 1 << 31 ) ) == 0 );
                bool32 bAltKeyIsDown = ( Message.lParam & ( 1 << 29 ) );
                if ( bIsDown != bWasDown ) {
                    if ( VKCode == 'Z' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveUp, bIsDown );
                    } else if ( VKCode == 'Q' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveLeft, bIsDown );
                    } else if ( VKCode == 'S' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveDown, bIsDown );
                    } else if ( VKCode == 'D' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveRight, bIsDown );
                    } else if ( VKCode == 'A' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->LeftShoulder, bIsDown );
                    } else if ( VKCode == 'E' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->RightShoulder, bIsDown );
#if HANDMADE_INTERNAL
					} else if ( VKCode == 'P' ) {
						if ( bIsDown ) {
							GlobalPause = !GlobalPause;
						}
#endif
                    } else if ( VKCode == VK_LEFT ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionLeft, bIsDown );
                    } else if ( VKCode == VK_RIGHT ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionRight, bIsDown );
                    } else if ( VKCode == VK_UP ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionUp, bIsDown );
                    } else if ( VKCode == VK_DOWN ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionDown, bIsDown );
                    } else if ( VKCode == VK_ESCAPE ) {
                        GlobalRunning = false;
                        Win32ProcessKeyboardMessage( &KeyboardController->Back, bIsDown );
                    } else if ( VKCode == VK_SPACE ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->Start, bIsDown );
                    } else if ( ( VKCode == VK_F4 ) && bAltKeyIsDown ) {
                        GlobalRunning = false;
                    }
                }
            } break;
            
            default:
            {
                TranslateMessage( &Message );
                DispatchMessage( &Message );
            }
        }
    }
}

inline LARGE_INTEGER
Win32GetWallClock()
{
	LARGE_INTEGER Result;
	::QueryPerformanceCounter( &Result );
	return Result;
}

inline real32 Win32GetSecondsElapsed( LARGE_INTEGER Start, LARGE_INTEGER End )
{
	real32 SecondsElapsedForWork =	(real32)( End.QuadPart - Start.QuadPart ) / 
									(real32)GlobalPerfCounterFrequency;
	return SecondsElapsedForWork;
}

internal void
Win32DebugDrawVertical( win32_offscreen_buffer *BackBuffer, int X, int Top, int Bottom, uint32 Color )
{
	if ( Top <= 0 ) { Top = 0; }
	if ( Bottom >= BackBuffer->Height ) { Bottom = BackBuffer->Height - 1; }
	if ( (X >= 0) && (X < BackBuffer->Width) ) {
		uint8 *Pixel = (uint8*)BackBuffer->Memory + X * BackBuffer->BytesPerPixel + Top * BackBuffer->Pitch;
		for ( int Y = Top; Y < Bottom; ++Y )
		{
			*((uint32*)Pixel) = Color;
			Pixel += BackBuffer->Pitch;
		}
	}
}

internal void
Win32DebugDrawHorizontal( win32_offscreen_buffer *BackBuffer, int Y, int Left, int Right, uint32 Color )
{
	uint8 *Pixel = (uint8*)BackBuffer->Memory +	Left * BackBuffer->BytesPerPixel + Y * BackBuffer->Pitch;
	for ( int X = Left; X < Right; ++X ) 
	{
		*((uint32*)Pixel) = Color;
		Pixel += BackBuffer->BytesPerPixel;
	}
}

#if 0
internal void
Win32DebugSyncDisplay(	win32_offscreen_buffer *BackBuffer, 
						int LastPlayCursorCount, win32_debug_time_marker *LastPlayCursor,
						win32_sound_output *SoundOutput, real32 SecondsPerFrame )
{
	int32 PadX = 16;
	int32 PadY = 16;
	int32 Top = PadY;
	int32 Bottom = BackBuffer->Height - PadY;

	real32 SoundSamplesToHPixels = (real32)(BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
	real32 SoundSamplesToVPixels = (real32)(BackBuffer->Height - 2 * PadY) / (real32)SoundOutput->SecondaryBufferSize;

	for ( int PlayCursorIndex = 0; PlayCursorIndex < LastPlayCursorCount; ++PlayCursorIndex ) {
		// remaps sound samples to pixels (minus padding)
		
		int32 PlayCursorX = PadX + (int)( (real32)LastPlayCursor[PlayCursorIndex].PlayCursor * SoundSamplesToPixels );
		int32 WriteCursorX = PadX + (int)( (real32)LastPlayCursor[PlayCursorIndex].WriteCursor * SoundSamplesToPixels );
		Win32DebugDrawVertical( BackBuffer, PlayCursorX, Top, Bottom, 0xFFFF00FF );
		Win32DebugDrawVertical( BackBuffer, WriteCursorX, Top, Bottom, 0xFF00FFFF );
	}
}
#endif

internal void
Win32DebugSyncDisplay(	win32_offscreen_buffer *BackBuffer,
						int LastPlayCursorCount, win32_debug_time_marker *LastPlayCursor,
						int CurrentMarkerIndex,
						win32_sound_output *SoundOutput, real32 SecondsPerFrame )
{
	int32 PadX = 16;
	int32 PadY = 16;
	//int32 Top = PadY;
	int32 Bottom = BackBuffer->Height - PadY;

	real32 SoundSamplesToHPixels = (real32)(BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
	int SoundSampleVPixelSize = (int)((real32)(BackBuffer->Height - 2 * PadY) / (real32)LastPlayCursorCount);

	for ( int PlayCursorIndex = 0; PlayCursorIndex < LastPlayCursorCount; ++PlayCursorIndex )
	{
		// remaps sound samples to pixels (minus padding)
		int Top = PadY + PlayCursorIndex * SoundSampleVPixelSize;

		int32 FlipPlayCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].FlipPlayCursor * SoundSamplesToHPixels);
		int32 FlipWriteCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].FlipWriteCursor * SoundSamplesToHPixels );
		int32 PlayCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputPlayCursor * SoundSamplesToHPixels);
		int32 WriteCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputWriteCursor * SoundSamplesToHPixels);
		int32 OutputLocationX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputLocation * SoundSamplesToHPixels );
		int32 BytesToWrite = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputByteCount * SoundSamplesToHPixels);
		int32 ExpectedX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].ExpectedFlipPlayCursor * SoundSamplesToHPixels);
		
		Win32DebugDrawVertical( BackBuffer, PlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFAA00AA );
		Win32DebugDrawVertical( BackBuffer, WriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00AAAA );

		if ( PlayCursorIndex == CurrentMarkerIndex ) {
			
			Top += SoundSampleVPixelSize;

			Win32DebugDrawVertical( BackBuffer, PlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFFF00FF );
			Win32DebugDrawVertical( BackBuffer, WriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00FFFF );
			Win32DebugDrawVertical(BackBuffer, ExpectedX, Top, Top + SoundSampleVPixelSize, 0xFFFFFF00); // yellow
			Win32DebugDrawHorizontal(BackBuffer, Top + (SoundSampleVPixelSize / 2), PlayCursorX, WriteCursorX, 0xFF000000 ); // black

			Top += SoundSampleVPixelSize;

			Win32DebugDrawVertical( BackBuffer, OutputLocationX, Top, Top + SoundSampleVPixelSize, 0xFF00FF00); // green
			Win32DebugDrawVertical( BackBuffer, OutputLocationX + BytesToWrite, Top, Top + SoundSampleVPixelSize, 0xFF00FF00); // green
			Win32DebugDrawHorizontal( BackBuffer, Top + (SoundSampleVPixelSize / 2), OutputLocationX, OutputLocationX + BytesToWrite, 0xFFFFFFFF);

			Top += SoundSampleVPixelSize;

			Win32DebugDrawVertical( BackBuffer, FlipPlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFFF00FF );
			Win32DebugDrawVertical( BackBuffer, FlipWriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00FFFF );
			Win32DebugDrawHorizontal( BackBuffer, Top + (SoundSampleVPixelSize / 2), FlipPlayCursorX, FlipWriteCursorX, 0xFFFFFFFF );

			
		}
	}
}

int CALLBACK WinMain(	HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow )
{
	LARGE_INTEGER PerfCounterFrequencyResult;
	::QueryPerformanceFrequency(&PerfCounterFrequencyResult);
	GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;
	bool32 SleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
	// desired windows scheduler granularity to 1 ms

    Win32LoadXInput();
    Win32ResizeDIBSection( &GlobalBackBuffer, 1280, 720 );
    
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = hInstance;
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    
	// TODO(nfauvet): how do we reliably query this on windows.
	const int32 MonitorRefreshHz = 60;
	const int32 GameUpdateHz = MonitorRefreshHz / 2; // 30 Hz because software rendering.
	real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    if ( RegisterClassA( &WindowClass ) ) {
        HWND hWindow = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            hInstance,
            0 
            );
        
        if ( hWindow ) {
            // Since we spcified CS_OWNDC, we can just get one 
            // device context and use it forever because we are
            // not sharing it with anyone.
            HDC dc = ::GetDC( hWindow );
            
            bool DoVibration = false;
            
            // sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof( int16 ) * 2; // [LEFT  RIGHT] LEFT RIGHT LEFT RIGHT
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.tSine = 0.0f;
			// TODO(nfauvet): get rid of LatencySampleCount;
			SoundOutput.LatencySampleCount = 3 * ( SoundOutput.SamplesPerSecond / GameUpdateHz );
			// TODO(nfauvet): compute the variance to find the lowest value.
			SoundOutput.SafetyBytes = ( ( SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz ) / 3;


            Win32InitDSound( hWindow, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize );
            Win32ClearBuffer( &SoundOutput );
            pGlobalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING );
            
#if 0		// This tests the PlayCursor/WriteCursor update frequency
			GlobalRunning = true;
			while ( GlobalRunning ) {
				DWORD PlayCursor;
				DWORD WriteCursor;
				pGlobalSecondaryBuffer->GetCurrentPosition( &PlayCursor, &WriteCursor );

				char text_buffer[512];
				_snprintf_s( text_buffer, sizeof( text_buffer ), "PC: %u WC: %u\n", PlayCursor, WriteCursor );
				::OutputDebugStringA( text_buffer );
			}
#endif

            // ALLOC sound buffer
            int16 *Samples = (int16 *)::VirtualAlloc( 0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            // ALLOC all game storage
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes( 64 );
            GameMemory.TransientStorageSize = Gigabytes( 1 );
            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = ::VirtualAlloc( BaseAddress, TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            GameMemory.TransientStorage = (void*)((uint8_t*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize );
            
            // quit if memory alloc failed.
            if ( Samples && GameMemory.PermanentStorage )
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];
                
                LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();
                uint64 LastCycleCount = __rdtsc();
                
				int DebugLastPlayCursorIndex = 0;
				//win32_debug_time_marker DebugLastPlayCursor[GameUpdateHz / 2] = { 0 };
				win32_debug_time_marker DebugLastPlayCursor[2 * GameUpdateHz] = { 0 };

				bool32 bSoundIsValid = false;
				DWORD AudioLatencyBytes = 0;
				real32 AudioLatencySeconds = 0;
				

                GlobalRunning = true;
                while ( GlobalRunning )
                {
                    game_controller_input *OldKeyboardController = GetController( OldInput, 0 );
                    game_controller_input *NewKeyboardController = GetController( NewInput, 0 );
                    
                    // TODO(nfauvet): Zeroing macro
                    // copy endeddown but reset transition.
                    game_controller_input ZeroKeyboard = {};
                    *NewKeyboardController = ZeroKeyboard;
                    NewKeyboardController->IsConnected = true;
                    for ( int ButtonIndex = 0; ButtonIndex < ArrayCount( NewKeyboardController->Buttons ); ++ButtonIndex ) {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages( NewKeyboardController );
                    
					if (!GlobalPause)
					{
						// xinput poll
						DWORD MaxControllerCount = XUSER_MAX_COUNT; // 4 ?
						if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) { // 4  > 5 -1 ?
							MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
						}

						// for each controller (not the keyboard)
						for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
						{
							XINPUT_STATE ControllerState;

							DWORD OurControllerIndex = ControllerIndex + 1;
							game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

							if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
							{
								XINPUT_GAMEPAD *pPad = &ControllerState.Gamepad;

								NewController->IsConnected = true;
								NewController->IsAnalog = false;

								NewController->StickAverageX = Win32ProcessXinputStickValue(pPad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageY = Win32ProcessXinputStickValue(pPad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

								// Analog only if we actually moved the sticks out of the deadzone.
								if ((NewController->StickAverageX != 0.0f)
									|| (NewController->StickAverageY != 0.0f)) {
									NewController->IsAnalog = true;
								}

								// Fake a StickX/Y full value if the DPAD buttons have been pushed.
								if (pPad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
									NewController->StickAverageY = 1.0f;
								}
								if (pPad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
									NewController->StickAverageY = -1.0f;
								}
								if (pPad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
									NewController->StickAverageX = 1.0f;
								}
								if (pPad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
									NewController->StickAverageX = -1.0f;
								}

								// Fake a "Move" buttons push with the StickX/Y if above a threshold
								real32 Threshold = 0.5f;
								Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0,
									&OldController->MoveLeft, 1,  // FAKE Bitfield that is going to be "&" with 1 or 0
									&NewController->MoveLeft);
								Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0,
									&OldController->MoveRight, 1,
									&NewController->MoveRight);
								Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0,
									&OldController->MoveDown, 1,
									&NewController->MoveDown);
								Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0,
									&OldController->MoveUp, 1,
									&NewController->MoveUp);

								//                            Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveUp, XINPUT_GAMEPAD_DPAD_UP, &NewController->MoveUp );
								//                            Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveDown, XINPUT_GAMEPAD_DPAD_DOWN, &NewController->MoveDown );
								//                            Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveRight, XINPUT_GAMEPAD_DPAD_LEFT, &NewController->MoveRight );
								//                            Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveLeft, XINPUT_GAMEPAD_DPAD_RIGHT, &NewController->MoveLeft );

								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);

								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
								Win32ProcessXInputDigitalButton(pPad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
							}
							else {
								NewController->IsConnected = false;
							}
						}

						if (DoVibration) {
							XINPUT_VIBRATION Vibration;
							Vibration.wLeftMotorSpeed = 60000;
							Vibration.wRightMotorSpeed = 60000;
							XInputSetState(0, &Vibration);
						}



						// GAME UPDATE + RENDER
						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBuffer.Memory;
						Buffer.Width = GlobalBackBuffer.Width;
						Buffer.Height = GlobalBackBuffer.Height;
						Buffer.Pitch = GlobalBackBuffer.Pitch;
						GameUpdateAndRender(&GameMemory, NewInput, &Buffer);

						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed( FlipWallClock, AudioWallClock );

						// SOUND
						DWORD PlayCursor;
						DWORD WriteCursor;
						if (DS_OK == pGlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))
						{
							/* NOTE(nfauvet):

								Here is how sound output computation works.

								We define a safety value that is te number of samples we think
								our game update loop may vary by(let's say up to 2ms).

								When we wake up to write audio, we will look and see what the play
								cursor position is and we will forecast ahead where we think the
								play cursor will be on the next frame boundary.

								We will then look to see if the write cursor is before that
								by at least our safety value. If it is, the target fill position
								is that frame boundary plus one frame. This gives us perfect
								audio sync in the case of a card that has low enough latency.

								If the write cursor is _after_ that safety margin, then we
								assume we can never sync the audio perfectly, so we will write
								one frame's worth of audio plus the safety margin's worth
								of guard samples.
								*/

							if (!bSoundIsValid) {
								// at startup, reset the running sample position to the writecursor.
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								bSoundIsValid = true;
							}

							// Lock wherever we were, running is infinite, wrap it up the circular buffer.
							DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

							DWORD ExpectedSoundBytesPerFrame = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
							real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor){
								SafeWriteCursor += SoundOutput.SecondaryBufferSize; // wrap/normalize relatively to the PlayCursor
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes; // accounting for the jitter

							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatency) {
								TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
							}
							else {
								TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
							}
							TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor) {
								// 2 sections to write to, 1 section after, and one at the beginning befiore the PlayCursor
								BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock + TargetCursor;
							}
							else {
								BytesToWrite = TargetCursor - ByteToLock;
							}

							// sound
							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;

							// Ask Game for sound samples
							GameGetSoundSamples(&GameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL
							win32_debug_time_marker *Marker = &DebugLastPlayCursor[DebugLastPlayCursorIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor) {
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = ((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond;

							char text_buffer[512];
							_snprintf_s(text_buffer, sizeof(text_buffer), "BTL: %u PC: %u WC: %u DELTA: %u (%.3fs)\n", ByteToLock, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
							::OutputDebugStringA(text_buffer);
#endif
							// Actually write the sound samples to the sound card.
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);

						}
						else {
							bSoundIsValid = false;
						}


						// timing
						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsedForWork = Win32GetSecondsElapsed(LastCounter, WorkCounter);
						real32 SecondsElapsedForFrame = WorkSecondsElapsedForWork;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
							while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
								if (SleepIsGranular) {
									DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
									if (SleepMS > 0) {
										::Sleep(SleepMS);
									}
								}
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else {
							// TODO: missed framerate !!!!
							// TODO: log it
						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						uint64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;

						// copy display buffer
						win32_window_dimension d = Win32GetWindowDimension(hWindow);
#if HANDMADE_INTERNAL
						Win32DebugSyncDisplay(&GlobalBackBuffer,
							ArrayCount(DebugLastPlayCursor), DebugLastPlayCursor, DebugLastPlayCursorIndex - 1,
							&SoundOutput, TargetSecondsPerFrame);
#endif
						Win32CopyBufferToWindow(dc, d.width, d.height, &GlobalBackBuffer);

						FlipWallClock = Win32GetWallClock();
#if HANDMADE_INTERNAL
						// debug sound
						{
							DWORD FlipPlayCursor;
							DWORD FlipWriteCursor;
							if (DS_OK == pGlobalSecondaryBuffer->GetCurrentPosition(&FlipPlayCursor, &FlipWriteCursor)) {
								Assert(DebugLastPlayCursorIndex < ArrayCount(DebugLastPlayCursor));
								win32_debug_time_marker *Marker = &DebugLastPlayCursor[DebugLastPlayCursorIndex];

								Marker->FlipPlayCursor = FlipPlayCursor;
								Marker->FlipWriteCursor = FlipWriteCursor;
							}
						}
#endif

						// swap
						game_input *tmp = NewInput;
						NewInput = OldInput;
						OldInput = tmp;

#if 0
						real32 fps = 0.0f; // (real32)(GlobalPerfCounterFrequency / (real32)CounterElapsed);

						char fps_buffer[512];
						_snprintf_s(fps_buffer, sizeof(fps_buffer), "ms/frame: %.02f, fps: %.02f, %I64u cycles/frame\n", MSPerFrame, fps, CyclesElapsed );
						::OutputDebugStringA( fps_buffer );
#endif

#if HANDMADE_INTERNAL
						++DebugLastPlayCursorIndex;
						if (DebugLastPlayCursorIndex == ArrayCount(DebugLastPlayCursor)) { // wrap
							DebugLastPlayCursorIndex = 0;
						}
#endif
					}
				}
			} else {
				// TODO: log, VirtualAlloc failed.
			}
		} else {
			// TODO: log
		}
	} else {
		// TODO: log
	}

	return(0);
}
