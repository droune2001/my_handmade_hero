#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

struct v2
{
	union
	{
		struct 
		{
			real32 X, Y;
		};
		real32 E[2];
	};
	
	inline real32 &operator[]( int Index );
};

// seriously... why not make a constructor?!!
inline v2 V2( real32 A, real32 B )
{
	v2 Result;

	Result.X = A;
	Result.Y = B;

	return Result;
}

inline real32 &v2::operator[]( int Index ) 
{
	return(( &X )[Index]);
}

inline v2 operator*( real32 A, v2 B )
{
	v2 Result;

	Result.X = A * B.X;
	Result.Y = A * B.Y;

	return Result;
}

inline v2 operator*( v2 B, real32 A )
{
	v2 Result = A * B;
	return Result;
}

inline v2 operator-( v2 A )
{
	v2 Result;

	Result.X = -A.X;
	Result.Y = -A.Y;

	return Result;
}

inline v2 operator+( v2 A, v2 B )
{
	v2 Result;

	Result.X = A.X + B.X;
	Result.Y = A.Y + B.Y;

	return Result;
}

inline v2 operator-( v2 A, v2 B )
{
	v2 Result;

	Result.X = A.X - B.X;
	Result.Y = A.Y - B.Y;

	return Result;
}

inline v2 & operator*=( v2 &A, real32 B )
{
	A = B * A;
	return A;
}

inline v2 & operator+=( v2 &A, v2 B )
{
	A = B + A;
	return A;
}

#endif // HANDMADE_MATH_H
