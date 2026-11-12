/* Standalone test/smoke runner: cc -std=c11 -Wall -Wextra -pedantic ... -lm */
#include "nacre_dungeon_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv){
  NacreDungeonConfig cfg;nacre_dungeon_generator_config_defaults(&cfg);
  if(argc>1){
    if(strlen(argv[1])>NACRE_DUNGEON_MAX_SEED_LENGTH)return 2;
    strcpy(cfg.seed,argv[1]);
  }
  NacreDungeonMap m={0};
  NacreDungeonResult result=nacre_dungeon_generator_generate(&cfg,&m);
  if(result!=NACRE_DUNGEON_OK){
    fprintf(stderr,"seed=%s result=%d\n",cfg.seed,(int)result);
    return result==NACRE_DUNGEON_NO_VALID_LAYOUT?3:1;
  }
  if(!nacre_dungeon_generator_validate(&m))return 4;
  printf("{\"seed\":\"%s\",\"requested_room_count\":%u,\"requested_totems\":%u,",
      cfg.seed,m.requested_room_count,m.requested_totem_count);
  printf("\"room_count\":%u,\"link_count\":%u,\"entry\":%u,\"exit\":%u,",
      m.room_count,m.link_count,m.entry,m.exit);
  printf("\"micro_corridors\":%u,\"rooms\":[",m.normalized_micro_corridors);
  for(uint32_t i=0;i<m.room_count;i++){
    const NacreDungeonRoom *r=&m.rooms[i];
    printf("%s[%d,%d,%d,%d,%d,%d]",i?",":"",r->x,r->y,r->width,r->height,
        r->center_x,r->center_y);
  }
  printf("],\"links\":[");
  for(uint32_t i=0;i<m.link_count;i++){
    const NacreDungeonLink *l=&m.links[i];
    printf("%s[%u,%u,%u,%u,%u,%u]",i?",":"",l->room_a,l->room_b,
        l->extra?1:0,l->direct?1:0,l->normalized_micro_corridor?1:0,l->path_count);
  }
  printf("],\"cells\":[");
  for(uint32_t i=0;i<m.width*m.height;i++)printf("%s%u",i?",":"",m.cells[i]);
  printf("],\"openEdges\":[");
  int first=1;
  for(uint32_t i=0;i<m.edge_count;i++)if(m.edges[i].type!=NACRE_DUNGEON_EDGE_CRACKED){
    printf("%s[%u,%u]",first?"":",",m.edges[i].a,m.edges[i].b);first=0;
  }
  printf("],\"holeEdges\":[");first=1;
  for(uint32_t i=0;i<m.edge_count;i++)if(m.edges[i].type==NACRE_DUNGEON_EDGE_HOLE){
    printf("%s[%u,%u]",first?"":",",m.edges[i].a,m.edges[i].b);first=0;
  }
  printf("],\"crackedEdges\":[");first=1;
  for(uint32_t i=0;i<m.edge_count;i++)if(m.edges[i].type==NACRE_DUNGEON_EDGE_CRACKED){
    printf("%s[%u,%u]",first?"":",",m.edges[i].a,m.edges[i].b);first=0;
  }
  printf("]}\n");
  nacre_dungeon_generator_release(&m);
  return 0;
}
