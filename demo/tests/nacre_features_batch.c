#include "../src/system/nacre_dungeon_generator.h"
#include "../src/system/nacre_dungeon_navigation.h"
#include "../src/system/nacre_dungeon_visibility.h"
#include "../src/system/nacre_dungeon_placement.h"
#include <stdio.h>
#include <string.h>
int main(void){
 unsigned success=0,reject=0,secrets=0,caves=0,pillars=0,cracked=0;
 for(unsigned iter=0;iter<1000;iter++){
  NacreDungeonConfig cfg; nacre_dungeon_generator_config_defaults(&cfg);
  snprintf(cfg.seed,sizeof(cfg.seed),"nacre-regression-%05u",iter);
  NacreDungeonMap map={0};
  NacreDungeonResult r=nacre_dungeon_generator_generate(&cfg,&map);
  if(r==NACRE_DUNGEON_NO_VALID_LAYOUT){reject++;continue;}
  if(r!=NACRE_DUNGEON_OK||!nacre_dungeon_generator_validate(&map)){
   printf("FAIL gen seed=%s result=%d\n",cfg.seed,r);return 1;
  }
  NacreDungeonVisibilityMap visibility={0};
  NacreDungeonPlacementList placement={0};
  if(!nacre_dungeon_visibility_build(&map,&visibility)||
     !nacre_dungeon_placements_build(&map,&visibility,true,true,&placement)){
   printf("FAIL placement seed=%s\n",cfg.seed);return 2;
  }
  for(uint32_t j=0;j<map.secret_room_count;j++){
   NacreDungeonSecretRoom *s=&map.secret_rooms[j];
   uint32_t a=s->entrance_outside,b=s->entrance_inside;
   int dx=(int)(b%map.width)-(int)(a%map.width);
   int dz=(int)(b/map.width)-(int)(a/map.width);
   if(nacre_dungeon_navigation_step(&map,a,dx,dz,NULL)==NACRE_DUNGEON_MOVE_ALLOWED){
    printf("FAIL secret gate %s\n",cfg.seed);return 3;
   }
  }
  /* Every cave portal is a real 1 m cracked-wall placement, including
   * cave-to-corridor edges where both room IDs equal -1. */
  for(uint32_t j=0;j<map.cave_count;j++){
   const NacreDungeonCave *cave=&map.caves[j];
   for(uint32_t k=0;k<2;k++){
    const NacreDungeonEdge *e=&cave->entrances[k];
    const int x0=(int)(e->a%map.width),z0=(int)(e->a/map.width);
    const int x1=(int)(e->b%map.width),z1=(int)(e->b/map.width);
    const int dx=x1-x0,dz=z1-z0;
    if(nacre_dungeon_navigation_step(&map,e->a,dx,dz,NULL)!=NACRE_DUNGEON_MOVE_CRACKED){
     printf("FAIL cave navigation seed=%s\n",cfg.seed);return 6;
    }
    const float x=dz ? (float)x0+.5f : (float)(x0>x1?x0:x1);
    const float z=dx ? (float)z0+.5f : (float)(z0>z1?z0:z1);
    const NacreDungeonPartKind expected=dz ?
        NACRE_DUNGEON_PART_CRACKED_X : NACRE_DUNGEON_PART_CRACKED_Z;
    bool found=false;
    for(uint32_t p=0;p<placement.count;p++){
     const NacreDungeonPlacement *part=&placement.items[p];
     if(part->kind==expected && part->x==x && part->z==z){found=true;break;}
    }
    if(!found){printf("FAIL cave wall placement seed=%s\n",cfg.seed);return 7;}
   }
  }
  for(uint32_t j=0;j<map.pillar_count;j++){
   uint32_t tile=map.pillars[j].tile;
   if(map.pillar_mask[tile]!=1){printf("FAIL pillar %s\n",cfg.seed);return 4;}
  }
  secrets+=map.secret_room_count;caves+=map.cave_count;pillars+=map.pillar_count;
  for(uint32_t j=0;j<map.edge_count;j++)cracked+=map.edges[j].type==NACRE_DUNGEON_EDGE_CRACKED;
  success++;
  nacre_dungeon_placements_release(&placement);
  nacre_dungeon_visibility_release(&visibility);
  nacre_dungeon_generator_release(&map);
 }
 printf("valid=%u rejected=%u secret=%u cave=%u pillars=%u cracked=%u\n",success,reject,secrets,caves,pillars,cracked);
 return 0;
}
