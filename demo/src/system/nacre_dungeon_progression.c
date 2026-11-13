/**
 * @file nacre_dungeon_progression.c
 * @brief Peripheral treasures, gates, color totems and optional exit goals.
 *
 * The order mirrors the HTML v21 design:
 *   1. Append one peripheral 3x3 or 4x4 room per requested color, each with
 *      a SINGLE authored edge to one existing non-entry/non-exit room.
 *   2. Assign 45/35/20 open/door/gate to ordinary openings, excluding holes.
 *      Reserve the treasure entrances as gates regardless of their draw.
 *   3. Simulate reachability and place totem anchors BEFORE each gate can be
 *      opened. Downgrade unsatisfiable optional gates to ordinary doors.
 *   4. Give each of the 1..3 colors a distinct password and totem. Extra gates
 *      share a color/password but have independent open state.
 *   5. Place N objectives off the union of shortest entry-to-exit routes.
 *
 * Cracked walls and cave shortcuts are considered accessible after a single
 * attack for reachability validation. Secret rooms NEVER count toward
 * progression. Ordinary doors may be opened without requiring a password.
 * No ECS, renderer, prefab or UI dependencies.
 */
#include "nacre_dungeon_progression.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct NacreProgressRng { uint32_t state; } NacreProgressRng;
static uint32_t rnd_uint(NacreProgressRng *r){
  r->state+=UINT32_C(0x6d2b79f5);
  uint32_t t=r->state;
  t=(t^(t>>15))*(t|1u);
  t^=t+((t^(t>>7))*(t|61u));
  return t^(t>>14);
}
static double rnd(NacreProgressRng *r){return (double)rnd_uint(r)/4294967296.0;}
static int randint(NacreProgressRng *r,int lo,int hi){return lo+(int)(rnd(r)*(hi-lo+1));}
static uint32_t suffix_hash(const NacreDungeonMap *m,const char *suffix){
  uint32_t hash=m->seed_hash;
  while(*suffix){hash^=(uint8_t)*suffix++;hash*=UINT32_C(16777619);}
  return hash;
}
static uint32_t abs_u(int a){return (uint32_t)(a<0?-a:a);}
static uint32_t min_u(uint32_t a,uint32_t b){return a<b?a:b;}
static int x_of(const NacreDungeonMap *m,uint32_t i){return (int)(i%m->width);}
static int y_of(const NacreDungeonMap *m,uint32_t i){return (int)(i/m->width);}
static uint32_t tile_of(const NacreDungeonMap *m,int x,int y){return (uint32_t)y*m->width+(uint32_t)x;}
static unsigned neighbors(const NacreDungeonMap *m,uint32_t i,uint32_t out[4]){
  unsigned n=0;int x=x_of(m,i),y=y_of(m,i);
  /* Same order as the v21 JS: RIGHT, LEFT, DOWN, UP. */
  if(x+1<(int)m->width)out[n++]=i+1;
  if(x>0)out[n++]=i-1;
  if(y+1<(int)m->height)out[n++]=i+m->width;
  if(y>0)out[n++]=i-m->width;
  return n;
}
static bool adjoining(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  return abs_u(x_of(m,a)-x_of(m,b))+abs_u(y_of(m,a)-y_of(m,b))==1u;
}
static bool edge_find(const NacreDungeonMap *m,uint32_t a,uint32_t b,
                      NacreDungeonEdgeType *type){
  if(a>b){uint32_t t=a;a=b;b=t;}
  for(uint32_t i=0;i<m->edge_count;i++){
    if(m->edges[i].a==a && m->edges[i].b==b){
      if(type)*type=m->edges[i].type;
      return true;
    }
  }
  return false;
}
static bool edge_push(NacreDungeonMap *m,uint32_t a,uint32_t b){
  if(a>b){uint32_t t=a;a=b;b=t;}
  if(edge_find(m,a,b,NULL))return false;
  NacreDungeonEdge *p=realloc(m->edges,(size_t)(m->edge_count+1)*sizeof(*p));
  if(!p)return false;
  m->edges=p;m->edges[m->edge_count++]=(NacreDungeonEdge){a,b,NACRE_DUNGEON_EDGE_OPEN};
  return true;
}
static NacreDungeonPassage *passage_find(const NacreDungeonMap *m,
                                        uint32_t a,uint32_t b){
  if(a>b){uint32_t t=a;a=b;b=t;}
  for(uint32_t i=0;i<m->passage_count;i++){
    if(m->passages[i].a==a&&m->passages[i].b==b)
      return &m->passages[i];
  }
  return NULL;
}
static bool traversable(const NacreDungeonMap *m,uint32_t a,uint32_t b,
                        bool cracks,bool all_gates){
  const uint32_t size=m->width*m->height;
  if(a>=size||b>=size||!adjoining(m,a,b)||
     !m->cells[a]||!m->cells[b]||
     m->cells[a]==NACRE_DUNGEON_SECRET||m->cells[b]==NACRE_DUNGEON_SECRET||
     (m->pillar_mask&&(m->pillar_mask[a]||m->pillar_mask[b])))return false;
  const bool transition=m->room_ids[a]!=m->room_ids[b]||
      ((m->cells[a]==NACRE_DUNGEON_CAVE)!=(m->cells[b]==NACRE_DUNGEON_CAVE));
  if(!transition)return true;
  NacreDungeonEdgeType type;
  if(!edge_find(m,a,b,&type)||type==NACRE_DUNGEON_EDGE_SECRET)return false;
  if(type==NACRE_DUNGEON_EDGE_CRACKED)return cracks;
  if(type==NACRE_DUNGEON_EDGE_HOLE)return true;
  if(type!=NACRE_DUNGEON_EDGE_OPEN)return false;
  const NacreDungeonPassage *p=passage_find(m,a,b);
  return !p || p->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS ||
      all_gates || p->opened;
}
bool nacre_dungeon_progression_can_step(const NacreDungeonMap *m,
    uint32_t from,uint32_t to,bool cracked,bool all_gates){
  if(!m||!m->cells||!m->room_ids)return false;
  return traversable(m,from,to,cracked,all_gates);
}
static int flood(const NacreDungeonMap *m,uint32_t from,
                 bool cracked,bool all_gates,uint8_t *seen){
  const uint32_t count=m->width*m->height;
  uint32_t *queue=malloc((size_t)count*sizeof(*queue));
  if(!queue)return -1;
  memset(seen,0,count);
  if(from>=count||!m->cells[from]){free(queue);return 0;}
  uint32_t start=0,end=1;queue[0]=from;seen[from]=1;
  while(start<end){
    uint32_t at=queue[start++],next[4];unsigned n=neighbors(m,at,next);
    for(unsigned j=0;j<n;j++)if(!seen[next[j]]&&
        traversable(m,at,next[j],cracked,all_gates)){
      seen[next[j]]=1;queue[end++]=next[j];
    }
  }
  free(queue);return (int)end;
}
static bool distances(const NacreDungeonMap *m,uint32_t from,int32_t *dist){
  uint32_t count=m->width*m->height,*queue=malloc((size_t)count*sizeof(*queue));
  if(!queue)return false;
  for(uint32_t i=0;i<count;i++)dist[i]=-1;
  uint32_t head=0,tail=1;queue[0]=from;dist[from]=0;
  while(head<tail){
    uint32_t at=queue[head++],ns[4];unsigned n=neighbors(m,at,ns);
    for(unsigned j=0;j<n;j++)if(dist[ns[j]]<0 &&
        traversable(m,at,ns[j],true,true)){
      dist[ns[j]]=dist[at]+1;queue[tail++]=ns[j];
    }
  }
  free(queue);return true;
}
typedef struct TreasureCandidate{
  int x,y,side,size;
  uint32_t parent_id,shared_count,inside,outside;
  double score;
} TreasureCandidate;
static int treas_cmp(const void *a,const void *b){
  const TreasureCandidate *u=a,*v=b;
  if(u->score>v->score)return -1;
  if(u->score<v->score)return 1;
  return u->parent_id<v->parent_id?-1:u->parent_id>v->parent_id?1:0;
}
static bool treasure_append(NacreDungeonMap *m, uint32_t parent_id,
                            const TreasureCandidate *c){
  const uint32_t id=m->room_count,n=(uint32_t)c->size*c->size;
  if(id>=NACRE_DUNGEON_MAX_ROOMS)return false;
  NacreDungeonRoom *r=&m->rooms[id];
  uint32_t *cells=malloc((size_t)n*sizeof(*cells));
  uint32_t *exits=malloc(sizeof(*exits));
  uint32_t *parent_exits=realloc(m->rooms[parent_id].exits,
      (size_t)(m->rooms[parent_id].exit_count+1)*sizeof(*parent_exits));
  if(!cells||!exits||!parent_exits){
    free(cells);free(exits);
    /* This may change the parent's allocation even if another allocation
       failed; the returned pointer must never be lost. */
    if(parent_exits)m->rooms[parent_id].exits=parent_exits;
    return false;
  }
  NacreDungeonLink *links=realloc(m->links,
      (size_t)(m->link_count+1)*sizeof(*links));
  if(!links){free(cells);free(exits);m->rooms[parent_id].exits=parent_exits;return false;}
  m->links=links;
  if(!edge_push(m,c->inside,c->outside)){
    free(cells);free(exits);m->rooms[parent_id].exits=parent_exits;return false;
  }
  for(int y=0;y<c->size;y++)for(int x=0;x<c->size;x++){
    uint32_t i=tile_of(m,c->x+x,c->y+y);
    m->cells[i]=NACRE_DUNGEON_ROOM;m->room_ids[i]=(int16_t)id;
    cells[y*c->size+x]=i;
  }
  *r=(NacreDungeonRoom){0};
  r->id=id;r->x=c->x;r->y=c->y;
  r->width=r->height=c->size;
  r->center_x=c->x+c->size/2;r->center_y=c->y+c->size/2;
  r->cells=cells;r->cell_count=n;
  r->exits=exits;r->exits[0]=c->inside;r->exit_count=1;
  r->peripheral_treasure=true;
  m->rooms[parent_id].exits=parent_exits;
  parent_exits[m->rooms[parent_id].exit_count++]=c->outside;
  NacreDungeonLink *link=&m->links[m->link_count++];
  memset(link,0,sizeof(*link));
  link->room_a=parent_id;link->room_b=id;link->direct=true;link->treasure=true;
  link->passage_count=1;link->passages[0]=(NacreDungeonEdge){
    c->inside<c->outside?c->inside:c->outside,
    c->inside<c->outside?c->outside:c->inside,
    NACRE_DUNGEON_EDGE_OPEN};
  m->treasures[m->treasure_count++]=(NacreDungeonTreasure){
    id,parent_id,link->passages[0].a,link->passages[0].b,0,
    NACRE_DUNGEON_COLOR_NONE};
  m->room_count++;
  return true;
}
static bool peripheral_treasures(const NacreDungeonConfig *cfg,NacreDungeonMap *m){
  (void)cfg;
  const uint32_t target=m->requested_totem_count;
  m->treasures=calloc(target,sizeof(*m->treasures));
  if(!m->treasures)return false;
  const uint32_t main_count=m->room_count;
  uint8_t parents_used[NACRE_DUNGEON_MAX_ROOMS]={0};
  NacreProgressRng rng={suffix_hash(m,"|peripheral-treasures-v16")};
  for(uint32_t r=0;r<target;r++){
    TreasureCandidate *candidates=NULL;uint32_t count=0,cap=0;
    for(uint32_t parent_id=0;parent_id<main_count;parent_id++){
      const NacreDungeonRoom *p=&m->rooms[parent_id];
      if(parent_id==(uint32_t)m->room_ids[m->entry]||
         parent_id==(uint32_t)m->room_ids[m->exit])continue;
      for(int side=0;side<4;side++)for(int size=3;size<=4;size++)
      for(int offset=-2;offset<=2;offset++){
        int x=0,y=0;
        if(side==0){x=p->x+p->width;y=p->center_y-size/2+offset;}
        else if(side==1){x=p->x-size;y=p->center_y-size/2+offset;}
        else if(side==2){x=p->center_x-size/2+offset;y=p->y+p->height;}
        else {x=p->center_x-size/2+offset;y=p->y-size;}
        if(x<2||y<2||x+size>(int)m->width-2||y+size>(int)m->height-2)continue;
        bool clear=true,other=false;
        uint32_t shared_count=0,best_in=0,best_out=0;
        int best_distance=INT32_MAX;
        for(int yy=y;yy<y+size&&clear;yy++)for(int xx=x;xx<x+size;xx++){
          if(m->cells[tile_of(m,xx,yy)]){clear=false;break;}
        }
        if(!clear)continue;
        /* Preserve JS iteration order: cells row-major; neighbors R,L,D,U. */
        for(int yy=y;yy<y+size;yy++)for(int xx=x;xx<x+size;xx++){
          uint32_t tile=tile_of(m,xx,yy),ns[4];unsigned n=neighbors(m,tile,ns);
          for(unsigned j=0;j<n;j++){
            uint32_t next=ns[j];int nx=x_of(m,next),ny=y_of(m,next);
            if(nx>=x&&nx<x+size&&ny>=y&&ny<y+size)continue;
            if(!m->cells[next])continue;
            if(m->room_ids[next]==(int16_t)parent_id&&
               m->cells[next]==NACRE_DUNGEON_ROOM){
              shared_count++;
              int d=(int)abs_u(xx-(x+size/2))+
                    (int)abs_u(yy-(y+size/2));
              if(d<best_distance){best_distance=d;best_in=tile;best_out=next;}
            }else other=true;
          }
        }
        if(other||shared_count<2)continue;
        if(count==cap){
          uint32_t next=cap?cap*2:128;
          TreasureCandidate *pnew=realloc(candidates,(size_t)next*sizeof(*pnew));
          if(!pnew){free(candidates);return false;}
          candidates=pnew;cap=next;
        }
        double parent_depth=fabs((double)p->center_x-(double)m->width*.5)+
            fabs((double)p->center_y-(double)m->height*.5);
        double score=parent_depth*.14+(double)min_u(shared_count,4u)*2+
            rnd(&rng)*4-(parents_used[parent_id]?30:0);
        candidates[count++]=(TreasureCandidate){
          x,y,side,size,parent_id,shared_count,best_in,best_out,score};
      }
    }
    if(!count){free(candidates);return false;}
    qsort(candidates,count,sizeof(*candidates),treas_cmp);
    TreasureCandidate choice=candidates[0];free(candidates);
    if(!treasure_append(m,choice.parent_id,&choice))return false;
    parents_used[choice.parent_id]=1;
  }
  /* Secret IDs were allocated immediately after MAIN rooms. The newly
   * appended real rooms occupy those IDs now, so shift hidden room IDs to
   * follow the FINAL ordinary room count. Otherwise visibility and room
   * validation disagree on ownership. */
  for(uint32_t s=0;s<m->secret_room_count;s++){
    NacreDungeonSecretRoom *secret=&m->secret_rooms[s];
    secret->id=main_count+target+s;
    for(uint32_t i=0;i<secret->cell_count;i++)
      m->room_ids[secret->cells[i]]=(int16_t)secret->id;
  }
  return m->room_count==m->requested_room_count&&m->treasure_count==target;
}

static int passage_cmp(const void *a,const void *b){
  const NacreDungeonPassage *x=a,*y=b;
  if(x->a!=y->a)return x->a<y->a?-1:1;
  return x->b<y->b?-1:x->b>y->b?1:0;
}
static bool passage_build(NacreDungeonMap *m){
  uint32_t n=0;
  for(uint32_t e=0;e<m->edge_count;e++)
    n+=m->edges[e].type==NACRE_DUNGEON_EDGE_OPEN;
  m->passages=calloc(n?n:1,sizeof(*m->passages));
  if(!m->passages)return false;
  for(uint32_t e=0;e<m->edge_count;e++){
    const NacreDungeonEdge *edge=&m->edges[e];
    if(edge->type!=NACRE_DUNGEON_EDGE_OPEN)continue;
    m->passages[m->passage_count++]=(NacreDungeonPassage){
      0,edge->a,edge->b,NACRE_DUNGEON_PASSAGE_OPEN,true,
      NACRE_DUNGEON_COLOR_NONE,0,-1};
  }
  qsort(m->passages,m->passage_count,sizeof(*m->passages),passage_cmp);
  NacreProgressRng rng={suffix_hash(m,"|intermediate-passages-v11|0")};
  for(uint32_t p=0;p<m->passage_count;p++){
    NacreDungeonPassage *pass=&m->passages[p];
    pass->id=p+1;
    double r=rnd(&rng);
    pass->kind=r<.45?NACRE_DUNGEON_PASSAGE_OPEN:
        r<.8?NACRE_DUNGEON_PASSAGE_DOOR:NACRE_DUNGEON_PASSAGE_PORTCULLIS;
    pass->opened=pass->kind==NACRE_DUNGEON_PASSAGE_OPEN;
  }
  /* Treasure gates are reserved before processing any regular gate. */
  for(uint32_t t=0;t<m->treasure_count;t++){
    NacreDungeonTreasure *treasure=&m->treasures[t];
    NacreDungeonPassage *gate=passage_find(m,treasure->entrance_a,
        treasure->entrance_b);
    if(!gate)return false;
    gate->kind=NACRE_DUNGEON_PASSAGE_PORTCULLIS;
    gate->opened=false;
    gate->treasure_room_id=(int32_t)treasure->room_id;
    treasure->gate_id=gate->id;
  }
  /* Reserve treasure gate origins FIRST, regardless of numeric passage ID.
   * Next, keep at most one other portcullis on any room-pair/link. This is
   * important when the ordinary gate was sorted BEFORE the treasure gate. */
  for(uint32_t p=0;p<m->passage_count;p++){
    NacreDungeonPassage *gate=&m->passages[p];
    if(gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||
       gate->treasure_room_id>=0)continue;
    const NacreDungeonLink *link=NULL;
    for(uint32_t l=0;l<m->link_count&&!link;l++){
      const NacreDungeonLink *candidate=&m->links[l];
      for(uint32_t e=0;e<candidate->passage_count;e++)
        if(candidate->passages[e].a==gate->a&&
           candidate->passages[e].b==gate->b){link=candidate;break;}
    }
    bool duplicate=false;
    for(uint32_t q=0;q<m->passage_count;q++){
      const NacreDungeonPassage *other=&m->passages[q];
      if(q==p||other->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||
         (other->treasure_room_id<0&&q>p))continue;
      const NacreDungeonLink *other_link=NULL;
      for(uint32_t l=0;l<m->link_count&&!other_link;l++){
        const NacreDungeonLink *candidate=&m->links[l];
        for(uint32_t e=0;e<candidate->passage_count;e++)
          if(candidate->passages[e].a==other->a&&
             candidate->passages[e].b==other->b){other_link=candidate;break;}
      }
      if(link&&other_link&&
         ((link->room_a==other_link->room_a&&
           link->room_b==other_link->room_b)||
          (link->room_a==other_link->room_b&&
           link->room_b==other_link->room_a))) {duplicate=true;break;}
      if(!link&&!other_link&&gate->a==other->a&&gate->b==other->b){
        duplicate=true;break;
      }
    }
    if(duplicate){gate->kind=NACRE_DUNGEON_PASSAGE_DOOR;gate->opened=false;}
  }
  return true;
}

/* Room score is intentionally a bounded heuristic. It prefers a previously
 * unused room, then a different branch and some distance from the gate.
 * Reachability is mandatory; heuristics never override it. */
static int totem_room_score(const NacreDungeonMap *m,
                            const NacreDungeonPassage *gate,
                            const NacreDungeonRoom *r,
                            const uint8_t *used_rooms){
  int score=used_rooms[r->id]?0:17;
  score+=r->id==(uint32_t)m->room_ids[m->entry]?-5:8;
  score+=r->id==(uint32_t)m->room_ids[m->exit]?-10:4;
  const int gax=x_of(m,gate->a),gay=y_of(m,gate->a);
  const int gbx=x_of(m,gate->b),gby=y_of(m,gate->b);
  int dist=(int)abs_u(r->center_x-gax)+(int)abs_u(r->center_y-gay)+
      (int)abs_u(r->center_x-gbx)+(int)abs_u(r->center_y-gby);
  score+=dist<60?dist/2:30;
  return score;
}
typedef struct GateAnchor {uint32_t gate_id,room_id,tile;} GateAnchor;
static bool totems_assign(NacreDungeonMap *m){
  uint32_t size=m->width*m->height;
  uint8_t *seen=calloc(size,1),*used_tiles=calloc(size,1);
  uint8_t used_rooms[NACRE_DUNGEON_MAX_ROOMS]={0};
  GateAnchor *anchors=calloc(m->passage_count?m->passage_count:1,sizeof(*anchors));
  if(!seen||!used_tiles||!anchors){free(seen);free(used_tiles);free(anchors);return false;}
  uint32_t anchor_count=0;
  NacreProgressRng rng={suffix_hash(m,"|intermediate-passages-v11|0")};
  /* The draws used when assigning passage types were in a separate RNG of
     the same suffix. Recreate its state to keep tie-breaking deterministic. */
  for(uint32_t i=0;i<m->passage_count;i++)(void)rnd(&rng);
  uint32_t guard=0;
  while(guard++ < m->passage_count+8){
    uint32_t gate_count=0;
    for(uint32_t g=0;g<m->passage_count;g++)
      gate_count+=m->passages[g].kind==NACRE_DUNGEON_PASSAGE_PORTCULLIS &&
          !m->passages[g].opened;
    if(!gate_count)break;
    if(flood(m,m->entry,true,false,seen)<0)goto failure;
    uint32_t chosen=UINT32_MAX;
    double best_priority=-INFINITY;
    uint32_t chosen_room=UINT32_MAX,chosen_tile=UINT32_MAX;
    for(uint32_t g=0;g<m->passage_count;g++){
      NacreDungeonPassage *gate=&m->passages[g];
      if(gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||gate->opened||
         (!seen[gate->a]&&!seen[gate->b]))continue;
      uint32_t best_room=UINT32_MAX,best_tile=UINT32_MAX;
      int room_score=INT32_MIN;
      for(uint32_t ri=0;ri<m->room_count;ri++){
        NacreDungeonRoom *r=&m->rooms[ri];
        if(r->peripheral_treasure ||
           ri==(uint32_t)m->room_ids[gate->a] ||
           ri==(uint32_t)m->room_ids[gate->b])continue;
        uint32_t tile=UINT32_MAX;
        int tile_score=INT32_MAX;
        for(uint32_t j=0;j<r->cell_count;j++){
          uint32_t t=r->cells[j];
          if(!seen[t]||used_tiles[t])continue;
          int d=(int)abs_u(x_of(m,t)-r->center_x)+
                (int)abs_u(y_of(m,t)-r->center_y);
          if(d<tile_score){tile=t;tile_score=d;}
        }
        if(tile==UINT32_MAX)continue;
        int score=totem_room_score(m,gate,r,used_rooms);
        if(score>room_score){room_score=score;best_room=ri;best_tile=tile;}
      }
      if(best_room==UINT32_MAX)continue;
      double priority=(gate->treasure_room_id>=0?130.0:0.0)+
          ((seen[gate->a]!=seen[gate->b])?45.0:0.0)+
          (double)room_score*.55+rnd(&rng)*3.0;
      if(priority>best_priority){
        best_priority=priority;chosen=g;chosen_room=best_room;
        chosen_tile=best_tile;
      }
    }
    if(chosen!=UINT32_MAX){
      NacreDungeonPassage *gate=&m->passages[chosen];
      anchors[anchor_count++]=(GateAnchor){gate->id,chosen_room,chosen_tile};
      used_tiles[chosen_tile]=1;used_rooms[chosen_room]=1;
      /* This is ONLY the simulated unlock: reset all gates before returning. */
      gate->opened=true;
    }else{
      /* An optional, inaccessible grade becomes a door. Treasure gates must
         remain; if one is inaccessible this layout is rejected. */
      bool converted=false;
      for(uint32_t p=0;p<m->passage_count;p++){
        NacreDungeonPassage *gate=&m->passages[p];
        if(gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS || gate->opened)continue;
        if(gate->treasure_room_id>=0)continue;
        gate->kind=NACRE_DUNGEON_PASSAGE_DOOR;
        gate->opened=false;
        converted=true;break;
      }
      if(!converted)goto failure;
    }
  }
  for(uint32_t p=0;p<m->passage_count;p++){
    NacreDungeonPassage *gate=&m->passages[p];
    if(gate->kind==NACRE_DUNGEON_PASSAGE_PORTCULLIS){
      if(!gate->opened)goto failure;
      gate->opened=false;
    }
  }
  /* Assign different colors to the treasure rooms, then share these colors
     with optional gates, exactly one totem/password per active color. */
  NacreProgressRng colors_rng={suffix_hash(m,"|totems-v13|0")};
  NacreDungeonTotemColor colors[3]={NACRE_DUNGEON_COLOR_RED,
       NACRE_DUNGEON_COLOR_GREEN,NACRE_DUNGEON_COLOR_BLUE};
  for(int i=2;i>0;i--){
    int j=randint(&colors_rng,0,i);
    NacreDungeonTotemColor t=colors[i];colors[i]=colors[j];colors[j]=t;
  }
  for(uint32_t t=0;t<m->treasure_count;t++){
    NacreDungeonTreasure *tr=&m->treasures[t];
    tr->color=colors[t];
    for(uint32_t p=0;p<m->passage_count;p++)if(m->passages[p].id==tr->gate_id)
      m->passages[p].color=colors[t];
  }
  for(uint32_t a=0;a<anchor_count;a++){
    NacreDungeonPassage *gate=NULL;
    for(uint32_t p=0;p<m->passage_count;p++)
      if(m->passages[p].id==anchors[a].gate_id)gate=&m->passages[p];
    if(!gate)return false;
    if(gate->color==NACRE_DUNGEON_COLOR_NONE)
      gate->color=colors[randint(&colors_rng,0,(int)m->treasure_count-1)];
  }
  m->totems=calloc(m->treasure_count,sizeof(*m->totems));
  if(!m->totems)goto failure;
  for(uint32_t a=0;a<anchor_count;a++){
    NacreDungeonPassage *gate=NULL;
    for(uint32_t p=0;p<m->passage_count;p++)
      if(m->passages[p].id==anchors[a].gate_id)gate=&m->passages[p];
    if(!gate)goto failure;
    bool exists=false;
    for(uint32_t t=0;t<m->totem_count;t++)if(m->totems[t].color==gate->color){
      gate->totem_id=m->totems[t].id;exists=true;break;
    }
    if(exists)continue;
    if(m->totem_count>=m->treasure_count)goto failure;
    NacreDungeonTotem *totem=&m->totems[m->totem_count++];
    totem->id=m->totem_count;totem->room_id=anchors[a].room_id;
    totem->tile=anchors[a].tile;totem->color=gate->color;
    bool unique=false;
    for(int guard_seq=0;guard_seq<2048 && !unique;guard_seq++){
      for(int j=0;j<4;j++)totem->sequence[j]=
          (NacreDungeonDirection)randint(&colors_rng,0,3);
      unique=true;
      for(uint32_t prev=0;prev+1<m->totem_count;prev++){
        bool same=true;
        for(int j=0;j<4;j++)if(totem->sequence[j]!=m->totems[prev].sequence[j])same=false;
        if(same){unique=false;break;}
      }
    }
    if(!unique)goto failure;
    gate->totem_id=totem->id;
  }
  bool valid=m->totem_count==m->treasure_count;
  free(seen);free(used_tiles);free(anchors);return valid;
failure:
  free(seen);free(used_tiles);free(anchors);return false;
}

static bool objectives_assign(const NacreDungeonConfig *cfg,NacreDungeonMap *m){
  m->exit_requirement=cfg->exit_requirement;
  if(cfg->exit_requirement==0)return true;
  uint32_t count=cfg->objective_count;
  uint32_t size=m->width*m->height;
  int32_t *start=malloc((size_t)size*sizeof(*start));
  int32_t *finish=malloc((size_t)size*sizeof(*finish));
  if(!start||!finish){free(start);free(finish);return false;}
  bool valid=distances(m,m->entry,start)&&distances(m,m->exit,finish);
  if(!valid){free(start);free(finish);return false;}
  uint32_t eligible[NACRE_DUNGEON_MAX_ROOMS],n=0;
  for(uint32_t i=0;i<m->room_count;i++){
    const NacreDungeonRoom *r=&m->rooms[i];
    if(r->peripheral_treasure||i==(uint32_t)m->room_ids[m->entry]||
       i==(uint32_t)m->room_ids[m->exit])continue;
    bool on_shortest=false,accessible=false;
    for(uint32_t j=0;j<r->cell_count;j++){
      uint32_t tile=r->cells[j];
      if(start[tile]>=0){accessible=true;
        if(finish[tile]>=0&&start[tile]+finish[tile]==start[m->exit])
          on_shortest=true;
      }
    }
    if(accessible&&!on_shortest)eligible[n++]=i;
  }
  free(start);free(finish);
  if(n<count)return false; /* Never silently lower required objective count. */
  NacreProgressRng rng={suffix_hash(m,"|objective-placement|0")};
  for(uint32_t i=n;i>1;i--){
    uint32_t j=(uint32_t)randint(&rng,0,(int)i-1);
    uint32_t k=eligible[i-1];eligible[i-1]=eligible[j];eligible[j]=k;
  }
  m->objectives=calloc(count,sizeof(*m->objectives));
  if(!m->objectives)return false;
  for(uint32_t o=0;o<count;o++){
    const NacreDungeonRoom *r=&m->rooms[eligible[o]];
    uint32_t tile=UINT32_MAX;int best=INT32_MAX;
    for(uint32_t j=0;j<r->cell_count;j++){
      uint32_t t=r->cells[j];
      bool occupied=false;
      for(uint32_t k=0;k<m->totem_count;k++)if(m->totems[k].tile==t)occupied=true;
      if(occupied)continue;
      int d=(int)abs_u(x_of(m,t)-r->center_x)+
            (int)abs_u(y_of(m,t)-r->center_y);
      if(d<best){best=d;tile=t;}
    }
    if(tile==UINT32_MAX)return false;
    m->objectives[m->objective_count++]=(NacreDungeonObjective){o+1,r->id,tile,false};
  }
  return true;
}

bool nacre_dungeon_progression_can_exit(const NacreDungeonMap *m){
  if(!m||!m->progression_valid)return false;
  for(uint32_t i=0;i<m->objective_count;i++)
    if(!m->objectives[i].completed)return false;
  return true;
}

bool nacre_dungeon_progression_validate(const NacreDungeonMap *m){
  if(!m||!m->cells||!m->rooms||!m->passages||!m->treasures||
     m->requested_totem_count<1||m->requested_totem_count>3||
     m->room_count!=m->requested_room_count||
     m->treasure_count!=m->requested_totem_count||
     m->totem_count!=m->treasure_count||
     (m->exit_requirement>2)||
     (m->exit_requirement==0&&m->objective_count!=0))return false;
  const uint32_t size=m->width*m->height;
  uint8_t *seen=calloc(size,1);
  if(!seen)return false;
  bool valid=true;
  for(uint32_t t=0;t<m->treasure_count&&valid;t++){
    const NacreDungeonTreasure *tr=&m->treasures[t];
    if(tr->room_id>=m->room_count||!m->rooms[tr->room_id].peripheral_treasure||
       tr->parent_room_id>=m->room_count||
       (m->room_ids[tr->entrance_a]!=(int16_t)tr->room_id &&
       m->room_ids[tr->entrance_b]!=(int16_t)tr->room_id)){valid=false;break;}
    const NacreDungeonPassage *gate=passage_find(m,tr->entrance_a,tr->entrance_b);
    if(!gate||gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||
       gate->id!=tr->gate_id||gate->color!=tr->color){valid=false;break;}
    uint32_t accesses=0;
    const NacreDungeonRoom *r=&m->rooms[tr->room_id];
    for(uint32_t j=0;j<r->cell_count;j++){
      uint32_t ns[4];unsigned n=neighbors(m,r->cells[j],ns);
      for(unsigned k=0;k<n;k++)if(m->cells[ns[k]]&&
          m->room_ids[ns[k]]!=(int16_t)tr->room_id&&
          edge_find(m,r->cells[j],ns[k],NULL))accesses++;
    }
    if(accesses!=1)valid=false;
    for(uint32_t u=0;u<t;u++)if(m->treasures[u].color==tr->color)valid=false;
  }
  for(uint32_t t=0;t<m->totem_count&&valid;t++){
    const NacreDungeonTotem *totem=&m->totems[t];
    if(totem->id!=t+1 || totem->room_id>=m->room_count||
       m->rooms[totem->room_id].peripheral_treasure ||
       totem->tile>=size||m->room_ids[totem->tile]!=(int16_t)totem->room_id){
      valid=false;break;
    }
    for(uint32_t prev=0;prev<t;prev++){
      if(m->totems[prev].color==totem->color)valid=false;
      bool same=true;
      for(int j=0;j<4;j++)if(m->totems[prev].sequence[j]!=totem->sequence[j])same=false;
      if(same)valid=false;
    }
  }
  for(uint32_t o=0;o<m->objective_count&&valid;o++){
    const NacreDungeonObjective *obj=&m->objectives[o];
    if(obj->id!=o+1||obj->tile>=size||m->cells[obj->tile]!=NACRE_DUNGEON_ROOM||
       m->room_ids[obj->tile]!=(int16_t)obj->room_id||
       obj->room_id>=(uint32_t)m->room_count||
       m->rooms[obj->room_id].peripheral_treasure){valid=false;break;}
    for(uint32_t prev=0;prev<o;prev++)if(m->objectives[prev].room_id==obj->room_id)
      valid=false;
  }
  if(valid){
    /* Closed gates should not prevent all progress: iteratively discover
     accessible totems, open gates of known colors, then flood again. */
    uint8_t *known=calloc(m->totem_count?m->totem_count:1,1);
    uint8_t *opened=calloc(m->passage_count?m->passage_count:1,1);
    if(!known||!opened)valid=false;
    else {
      uint32_t iterations=0;bool changed=true;
      while(changed&&iterations++<m->passage_count+m->totem_count+2){
        changed=false;
        /* Temporarily set opened in a shallow copy of the passage list. */
        NacreDungeonMap view=*m;
        NacreDungeonPassage *tmp=malloc((size_t)m->passage_count*sizeof(*tmp));
        if(!tmp){valid=false;break;}
        memcpy(tmp,m->passages,(size_t)m->passage_count*sizeof(*tmp));
        for(uint32_t i=0;i<m->passage_count;i++)
          if(opened[i])tmp[i].opened=true;
        view.passages=tmp;
        if(flood(&view,m->entry,true,false,seen)<0){free(tmp);valid=false;break;}
        free(tmp);
        for(uint32_t t=0;t<m->totem_count;t++)
          if(!known[t] && seen[m->totems[t].tile]){known[t]=1;changed=true;}
        for(uint32_t p=0;p<m->passage_count;p++){
          const NacreDungeonPassage *g=&m->passages[p];
          if(g->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||opened[p]||
             (!seen[g->a]&&!seen[g->b]))continue;
          for(uint32_t t=0;t<m->totem_count;t++)
            if(known[t]&&m->totems[t].color==g->color){
              opened[p]=1;changed=true;break;
            }
        }
      }
      if(valid){
        for(uint32_t p=0;p<m->passage_count;p++)
          if(m->passages[p].kind==NACRE_DUNGEON_PASSAGE_PORTCULLIS&&
             !opened[p])valid=false;
        for(uint32_t t=0;t<m->totem_count;t++)if(!known[t])valid=false;
        /* Fully unlocked traversal must reach all rooms and all objectives. */
        if(flood(m,m->entry,true,true,seen)<0)valid=false;
        if(valid)for(uint32_t r=0;r<m->room_count;r++){
          bool reached=false;
          for(uint32_t j=0;j<m->rooms[r].cell_count;j++)
            if(seen[m->rooms[r].cells[j]]){reached=true;break;}
          if(!reached){valid=false;break;}
        }
        if(valid)for(uint32_t o=0;o<m->objective_count;o++)
          if(!seen[m->objectives[o].tile])valid=false;
        if(valid && !seen[m->exit])valid=false;
      }
    }
    free(known);free(opened);
  }
  free(seen);return valid;
}

bool nacre_dungeon_progression_generate(const NacreDungeonConfig *cfg,
                                        NacreDungeonMap *m){
  if(!cfg||!m||!m->cells||!m->room_ids||!m->rooms||
     !peripheral_treasures(cfg,m)||!passage_build(m)||
     !totems_assign(m)||!objectives_assign(cfg,m))return false;
  m->progression_valid=true;
  return nacre_dungeon_progression_validate(m);
}
void nacre_dungeon_progression_release(NacreDungeonMap *m){
  if(!m)return;
  free(m->passages);free(m->treasures);free(m->totems);free(m->objectives);
  m->passages=NULL;m->treasures=NULL;m->totems=NULL;m->objectives=NULL;
  m->passage_count=m->treasure_count=m->totem_count=m->objective_count=0;
  m->progression_valid=false;
}
