#ifndef _HANDMADE_SIM_REGION_H_2017_04_29_
#define _HANDMADE_SIM_REGION_H_2017_04_29_

struct move_spec
{
	bool32 UnitMaxAccelVector;
	real32 Speed;
	real32 Drag;
};

enum entity_type
{
	EntityType_Null,
	EntityType_Hero,
	EntityType_Wall,
	EntityType_Familiar,
	EntityType_Monstar,
	EntityType_Sword
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	uint8 Flags;
	uint8 FilledAmount;
};

struct sim_entity;

union entity_reference
{
	sim_entity *Ptr;
	uint32 Index;
};

struct sim_entity
{
	uint32 StorageIndex;

	entity_type Type;

	v2 P; // NOTE(nfauvet): relative to the sim_region center.
	uint32 ChunkZ;
	
	real32 Z;
	real32 dZ;	

	v2 dP; // velocity (derivative of P)
	real32 Width, Height;

	uint32 FacingDirection;
	real32 tBob;

	bool32 Collides;
	int32 dAbsTileZ; // for "stairs"

	uint32 HighEntityIndex;

	uint32 HitPointMax;
	hit_point HitPoint[16];

	entity_reference Sword;
	real32 DistanceRemaining;
};

struct sim_entity_hash
{
	sim_entity *Ptr;
	uint32 Index;
};

struct sim_region
{
	// TODO(nfauvet): need a has table to map stored
	// entity indices to sim entities.

	world *World;
	world_position Origin;
	rectangle2 Bounds;

	uint32 MaxEntityCount;
	uint32 EntityCount;
	sim_entity *Entities;

	sim_entity_hash Hash[4096];
};

#endif //_HANDMADE_SIM_REGION_H_2017_04_29_
