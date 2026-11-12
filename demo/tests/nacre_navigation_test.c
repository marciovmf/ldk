#include "../src/system/nacre_dungeon_navigation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nacre_test_tiny_map(void)
{
  uint8_t cells[9] = {1,1,1, 1,1,1, 0,1,1};
  int16_t ids[9] = {0,1,2, 0,0,2, -1,-1,2};
  NacreDungeonEdge edges[3] = {
    {0,1,NACRE_DUNGEON_EDGE_OPEN},
    {1,2,NACRE_DUNGEON_EDGE_CRACKED},
    {4,5,NACRE_DUNGEON_EDGE_HOLE}
  };
  NacreDungeonMap m = {0};
  m.width = m.height = 3;
  m.cells=cells; m.room_ids=ids; m.edges=edges; m.edge_count=3;
  uint32_t out=1234;
  assert(nacre_dungeon_navigation_step(&m,0,1,0,&out)==NACRE_DUNGEON_MOVE_ALLOWED && out==1);
  out=1234;
  assert(nacre_dungeon_navigation_step(&m,1,1,0,&out)==NACRE_DUNGEON_MOVE_CRACKED && out==1234);
  assert(nacre_dungeon_navigation_step(&m,0,0,1,&out)==NACRE_DUNGEON_MOVE_ALLOWED && out==3);
  assert(nacre_dungeon_navigation_step(&m,4,1,0,&out)==NACRE_DUNGEON_MOVE_ALLOWED && out==5);
  out=1234;
  assert(nacre_dungeon_navigation_step(&m,3,0,1,&out)==NACRE_DUNGEON_MOVE_VOID && out==1234);
  assert(nacre_dungeon_navigation_step(&m,0,-1,0,&out)==NACRE_DUNGEON_MOVE_OUTSIDE);
  assert(nacre_dungeon_navigation_step(&m,0,1,1,&out)==NACRE_DUNGEON_MOVE_OUTSIDE);
  assert(nacre_dungeon_navigation_step(&m,0,0,0,&out)==NACRE_DUNGEON_MOVE_OUTSIDE);
  /* Adjacent different regions without an explicit edge are solid walls. */
  assert(nacre_dungeon_navigation_step(&m,1,0,1,&out)==NACRE_DUNGEON_MOVE_WALL);
}

static void nacre_test_generated_map(void)
{
  NacreDungeonConfig c;
  nacre_dungeon_generator_config_defaults(&c);
  unsigned generated=0, traversable=0, blocked=0, cracked=0;
  for (unsigned seed=0; seed<200; ++seed)
  {
    snprintf(c.seed,sizeof(c.seed),"NacreCollision-%u",seed);
    NacreDungeonMap m={0};
    NacreDungeonResult r=nacre_dungeon_generator_generate(&c,&m);
    if (r == NACRE_DUNGEON_NO_VALID_LAYOUT) continue;
    assert(r == NACRE_DUNGEON_OK);
    assert(nacre_dungeon_generator_validate(&m));
    ++generated;
    for (uint32_t a=0; a<m.width*m.height; ++a)
    {
      if (!m.cells[a]) continue;
      const int ds[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
      for (unsigned d=0;d<4;d++)
      {
        uint32_t dest=UINT32_MAX;
        NacreDungeonMoveResult result=nacre_dungeon_navigation_step(
            &m,a,ds[d][0],ds[d][1],&dest);
        if(result==NACRE_DUNGEON_MOVE_ALLOWED)
        {
          assert(dest<m.width*m.height && m.cells[dest]);
          assert(abs((int)(dest%m.width)-(int)(a%m.width))+
              abs((int)(dest/m.width)-(int)(a/m.width))==1);
          ++traversable;
        }
        else
        {
          assert(dest==UINT32_MAX);
          ++blocked;
          if(result==NACRE_DUNGEON_MOVE_CRACKED) ++cracked;
        }
      }
    }
    nacre_dungeon_generator_release(&m);
  }
  assert(generated>150);
  assert(traversable>0 && blocked>0 && cracked>0);
  printf("OK: %u maps, %u traversable cardinal steps, %u blocked, %u cracked\n",
      generated,traversable,blocked,cracked);
}

int main(void)
{
  nacre_test_tiny_map();
  nacre_test_generated_map();
  return 0;
}
