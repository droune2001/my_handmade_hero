#ifndef _HANDMADE_SIM_REGION_H_2017_04_29_
#define _HANDMADE_SIM_REGION_H_2017_04_29_

struct sim_entity
{
	uint32 StorageIndex;

	v2 P; // NOTE(nfauvet): relative to the camera.
	uint32 ChunkZ;
	
	real32 dZ;
	real32 Z;
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
};

#endif //_HANDMADE_SIM_REGION_H_2017_04_29_
