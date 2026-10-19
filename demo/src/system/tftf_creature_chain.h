#ifndef TFTF_CREATURE_CHAIN_H
#define TFTF_CREATURE_CHAIN_H

#include "../component/tftf_creature.h"

/* The chain is authored independently of its animated pose and mesh.
 * A node is a joint center; lengths and bend limits are structural data. */
typedef struct TFTFCreatureChain
{
  u32 count;
  Vec3 rest[TFTF_CREATURE_MAX_NODES];
  float length[TFTF_CREATURE_MAX_NODES - 1u];
  float bend_min[TFTF_CREATURE_MAX_NODES];
  float bend_max[TFTF_CREATURE_MAX_NODES];
} TFTFCreatureChain;

typedef struct TFTFCreaturePose
{
  Vec3 nodes[TFTF_CREATURE_MAX_NODES];
} TFTFCreaturePose;

void tftf_creature_chain_build(TFTFCreatureChain *chain,
    const TFTFCreatureNode *nodes, u32 count);
void tftf_creature_chain_rest_pose(const TFTFCreatureChain *chain,
    TFTFCreaturePose *pose);

/* Move only the first or last node; the opposite endpoint stays fixed.
 * Both FABRIK sweeps enforce distances and maximum/minimum joint angles.
 * Returns the distance from the requested goal to the solved endpoint.
 * The pose is preserved across calls to retain its preferred bend direction. */
float tftf_creature_chain_solve(const TFTFCreatureChain *chain,
    TFTFCreaturePose *pose, Vec3 target, u32 moving_endpoint);

#endif
