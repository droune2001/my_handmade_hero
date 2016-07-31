#ifndef _WIN32_HANDMADE_H_
#define _WIN32_HANDMADE_H_

struct win32_window_dimension
{
	int width;
	int height;
};

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_sound_output {
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	DWORD SecondaryBufferSize;
	real32 tSine;
	int LatencySampleCount;
	DWORD SafetyBytes;
	// TODO(nfauvet): Add BytesPerSeconds
};

struct win32_debug_time_marker
{
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD ExpectedFlipPlayCursor;

	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};

#endif // _WIN32_HANDMADE_H_
