#include "nacre_dungeon_generator.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* All implementation details remain private to this translation unit. */
typedef struct NacreRng { uint32_t state; } NacreRng;
typedef struct NacreCandidate {
  uint32_t a, b;
  int gap;
  double order;
} NacreCandidate;
typedef struct NacreBorder {
  uint32_t inside, outside;
  int x, y, dx, dy;
  double score;
} NacreBorder;
typedef struct NacreExit {
  uint32_t inside, outside;
  double score;
} NacreExit;
typedef struct NacrePair {
  NacreExit a, b;
  double priority;
} NacrePair;
typedef struct NacreAstarNode { uint32_t tile; double g, f; } NacreAstarNode;
typedef struct NacreHeap { NacreAstarNode *items; size_t size, cap; } NacreHeap;
typedef struct NacreSpecial {
  uint32_t a, b;
  uint32_t ra, rb;
  double cx, cy, score;
  int detour;
} NacreSpecial;

typedef struct NacreBuild {
  NacreDungeonMap *map;
  NacreRng rng;
  uint32_t *room_capacity;
  uint32_t *exit_capacity;
  uint32_t link_capacity, edge_capacity;
  NacreSpecial *reserved;
  size_t reserved_count, reserved_capacity;
  bool oom;
} NacreBuild;

/*
 * JS v21 uses str.charCodeAt(i), i.e. UTF-16 code units, not raw UTF-8 bytes.
 * Decode UTF-8 input and hash UTF-16 units to preserve deterministic seeds
 * containing accented letters or non-BMP characters.
 */
static uint32_t nacre_fnv_unit(uint32_t hash, uint16_t unit) {
  return (hash ^ (uint32_t)unit) * UINT32_C(16777619);
}
static uint32_t nacre_seed_hash(const char *str) {
  uint32_t h=UINT32_C(2166136261);
  const unsigned char *s=(const unsigned char *)str;
  while(*s){
    uint32_t cp=0;
    const uint8_t c=*s++;
    if(c<0x80)cp=c;
    else if((c&0xe0)==0xc0 && (s[0]&0xc0)==0x80){
      cp=((uint32_t)(c&0x1f)<<6)|(uint32_t)(s[0]&0x3f);s++;
      if(cp<0x80)cp=0xfffd;
    }else if((c&0xf0)==0xe0 && s[0] && s[1] &&
        (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80){
      cp=((uint32_t)(c&0xf)<<12)|((uint32_t)(s[0]&0x3f)<<6)|
         (uint32_t)(s[1]&0x3f);s+=2;
      if(cp<0x800 || (cp>=0xd800 && cp<=0xdfff))cp=0xfffd;
    }else if((c&0xf8)==0xf0 && s[0] && s[1] && s[2] &&
        (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80 && (s[2]&0xc0)==0x80){
      cp=((uint32_t)(c&7)<<18)|((uint32_t)(s[0]&0x3f)<<12)|
         ((uint32_t)(s[1]&0x3f)<<6)|(uint32_t)(s[2]&0x3f);s+=3;
      if(cp<0x10000||cp>0x10ffff)cp=0xfffd;
    }else cp=0xfffd;
    if(cp<=0xffff)h=nacre_fnv_unit(h,(uint16_t)cp);
    else {cp-=0x10000;
      h=nacre_fnv_unit(h,(uint16_t)(0xd800+(cp>>10)));
      h=nacre_fnv_unit(h,(uint16_t)(0xdc00+(cp&0x3ff)));
    }
  }
  return h;
}
static uint32_t nacre_rng_uint(NacreRng *rng) {
  uint32_t t;
  rng->state+=UINT32_C(0x6d2b79f5);
  t=rng->state;
  t=(t^(t>>15))*(t|1u);
  t^=t+( (t^(t>>7))*(t|61u) );
  return t^(t>>14);
}
static double nacre_rand(NacreRng *rng) {
  return (double)nacre_rng_uint(rng)/4294967296.0;
}
static int nacre_rand_int(NacreRng *rng,int a,int b) {
  return a+(int)floor(nacre_rand(rng)*(double)(b-a+1));
}
static uint32_t nacre_hash_suffix(const char *seed,const char *suffix) {
  /* FNV-1a continuation avoids concatenation and temporary buffers. */
  uint32_t h=nacre_seed_hash(seed);
  const unsigned char *s=(const unsigned char *)suffix;
  while(*s) {h^=(uint32_t)*s++;h*=UINT32_C(16777619);}
  return h;
}
static bool nacre_grow(void **data,uint32_t *capacity,uint32_t size,size_t stride) {
  if(*capacity>=size)return true;
  uint32_t next=*capacity?*capacity:8;
  while(next<size) {if(next>UINT32_MAX/2)return false;next*=2;}
  if(stride && (size_t)next>SIZE_MAX/stride)return false;
  void *p=realloc(*data,(size_t)next*stride);
  if(!p)return false;
  *data=p;*capacity=next;return true;
}
static int nacre_idx(const NacreDungeonMap *m,int x,int y) {return y*(int)m->width+x;}
static int nacre_x(const NacreDungeonMap *m,uint32_t i) {return (int)(i%m->width);}
static int nacre_y(const NacreDungeonMap *m,uint32_t i) {return (int)(i/m->width);}
static int nacre_abs(int x) {return x<0?-x:x;}
static int nacre_max(int a,int b) {return a>b?a:b;}
static int nacre_min(int a,int b) {return a<b?a:b;}
static int nacre_edge_order(const NacreDungeonEdge *e,uint32_t a,uint32_t b){
  return (e->a==a&&e->b==b)||(e->a==b&&e->b==a);
}
static bool nacre_edge_exists(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->edge_count;i++)if(nacre_edge_order(&m->edges[i],a,b) &&
      m->edges[i].type!=NACRE_DUNGEON_EDGE_CRACKED)return true;
  return false;
}
static bool nacre_edge_any(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->edge_count;i++)if(nacre_edge_order(&m->edges[i],a,b))return true;
  return false;
}
static bool nacre_edge_special(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->edge_count;i++)if(nacre_edge_order(&m->edges[i],a,b)&&
      m->edges[i].type!=NACRE_DUNGEON_EDGE_OPEN)return true;
  return false;
}
static bool nacre_add_edge(NacreBuild *s,uint32_t a,uint32_t b,NacreDungeonEdgeType type){
  NacreDungeonMap *m=s->map;
  if(nacre_edge_any(m,a,b))return true;
  if(!nacre_grow((void**)&m->edges,&s->edge_capacity,m->edge_count+1,sizeof(*m->edges)))
    return !(s->oom=true);
  if(a>b){uint32_t t=a;a=b;b=t;}
  m->edges[m->edge_count++]=(NacreDungeonEdge){a,b,type};return true;
}
static void nacre_remove_edge(NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->edge_count;i++)if(nacre_edge_order(&m->edges[i],a,b)){
    memmove(m->edges+i,m->edges+i+1,(m->edge_count-i-1)*sizeof(*m->edges));
    m->edge_count--;return;
  }
}
static bool nacre_add_room_cell(NacreBuild *s,uint32_t room_id,uint32_t tile){
  NacreDungeonRoom *r=&s->map->rooms[room_id];
  if(!nacre_grow((void**)&r->cells,&s->room_capacity[room_id],r->cell_count+1,sizeof(uint32_t)))
    return !(s->oom=true);
  r->cells[r->cell_count++]=tile;return true;
}
static bool nacre_add_exit(NacreBuild *s,uint32_t room_id,uint32_t tile){
  NacreDungeonRoom *r=&s->map->rooms[room_id];
  if(!nacre_grow((void**)&r->exits,&s->exit_capacity[room_id],r->exit_count+1,sizeof(uint32_t)))
    return !(s->oom=true);
  r->exits[r->exit_count++]=tile;return true;
}
static NacreDungeonLink *nacre_new_link(NacreBuild *s,uint32_t a,uint32_t b,bool extra,bool direct){
  NacreDungeonMap *m=s->map;
  if(!nacre_grow((void**)&m->links,&s->link_capacity,m->link_count+1,sizeof(*m->links))){
    s->oom=true;return NULL;
  }
  NacreDungeonLink *out=&m->links[m->link_count++];memset(out,0,sizeof(*out));
  out->room_a=a;out->room_b=b;out->extra=extra;out->direct=direct;
  return out;
}
static bool nacre_near_grid(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  return nacre_abs(nacre_x(m,a)-nacre_x(m,b))+nacre_abs(nacre_y(m,a)-nacre_y(m,b))==1;
}
static unsigned nacre_neighbors(const NacreDungeonMap *m,uint32_t i,uint32_t out[4]) {
  const int x=nacre_x(m,i),y=nacre_y(m,i);
  unsigned k=0;
  /* Same order as v21: right, left, down, up. */
  if(x+1<(int)m->width)out[k++]=i+1;
  if(x>0)out[k++]=i-1;
  if(y+1<(int)m->height)out[k++]=i+m->width;
  if(y>0)out[k++]=i-m->width;
  return k;
}
static bool nacre_in_bounds(const NacreDungeonMap *m,int x,int y){
  return x>=1&&y>=1&&x<(int)m->width-1&&y<(int)m->height-1;
}
static bool nacre_add_room(NacreBuild *s,const NacreDungeonConfig *cfg){
  NacreDungeonMap *m=s->map;NacreRng *rng=&s->rng;
  bool empty=m->room_count==0;
  for(int tries=0;tries<(empty?100:1800);tries++){
    const int w=nacre_rand_int(rng,4,(int)cfg->max_room_size);
    const int h=nacre_rand_int(rng,4,(int)cfg->max_room_size);
    if(w>(int)m->width-6||h>(int)m->height-6)continue;
    int x,y;
    if(empty){
      x=((int)m->width-w)/2+nacre_rand_int(rng,-2,2);
      y=((int)m->height-h)/2+nacre_rand_int(rng,-2,2);
    }else{
      NacreDungeonRoom *p=&m->rooms[nacre_rand_int(rng,0,(int)m->room_count-1)];
      int direction=nacre_rand_int(rng,0,3);
      int gap=nacre_rand(rng)<cfg->adjacent_rooms ? 0 :
        nacre_rand_int(rng,2,nacre_min(5,(int)cfg->max_corridor_tiles));
      if(direction<2){
        x=direction==0?p->x+p->width+gap:p->x-w-gap;
        y=p->y+nacre_rand_int(rng,-h+2,p->height-2);
      }else{
        y=direction==2?p->y+p->height+gap:p->y-h-gap;
        x=p->x+nacre_rand_int(rng,-w+2,p->width-2);
      }
    }
    if(x<2||y<2||x+w>(int)m->width-2||y+h>(int)m->height-2)continue;
    bool collision=false;
    for(uint32_t j=0;j<m->room_count;j++){
      const NacreDungeonRoom *p=&m->rooms[j];
      if(x<p->x+p->width&&x+w>p->x&&y<p->y+p->height&&y+h>p->y){collision=true;break;}
    }
    if(collision)continue;
    bool notch=w>=6&&h>=6&&nacre_rand(rng)<cfg->irregular_rooms;
    int corner=0,notch_w=0,notch_h=0;
    if(notch){corner=nacre_rand_int(rng,0,3);
      notch_w=nacre_rand_int(rng,2,nacre_max(2,(int)floor(w*.43)));
      notch_h=nacre_rand_int(rng,2,nacre_max(2,(int)floor(h*.43)));}
    uint32_t id=m->room_count;
    NacreDungeonRoom *r=&m->rooms[id];memset(r,0,sizeof(*r));
    r->id=id;r->x=x;r->y=y;r->width=w;r->height=h;
    r->center_x=x+w/2;r->center_y=y+h/2;
    m->room_count++;
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
      if(notch){bool right=corner%2==1,bottom=corner>=2;
        bool insideX=right?xx>=w-notch_w:xx<notch_w;
        bool insideY=bottom?yy>=h-notch_h:yy<notch_h;
        if(insideX&&insideY)continue;
      }
      uint32_t tile=(uint32_t)nacre_idx(m,x+xx,y+yy);
      if(!nacre_add_room_cell(s,id,tile))return false;
      m->cells[tile]=NACRE_DUNGEON_ROOM;m->room_ids[tile]=(int16_t)id;
    }
    return true;
  }
  return false;
}
static int nacre_room_gap(const NacreDungeonRoom *a,const NacreDungeonRoom *b){
  int dx=nacre_max(0,nacre_max(b->x-a->x-a->width,a->x-b->x-b->width));
  int dy=nacre_max(0,nacre_max(b->y-a->y-a->height,a->y-b->y-b->height));
  return dx+dy;
}
static int nacre_candidate_order(const void *pa,const void *pb){
  const NacreCandidate *a=pa,*b=pb;
  return a->order<b->order?-1:(a->order>b->order?1:0);
}
static int nacre_border_score(const NacreDungeonRoom *r,const NacreBorder *c){
  return nacre_min(nacre_min(c->x-r->x,r->x+r->width-1-c->x),
                   nacre_min(c->y-r->y,r->y+r->height-1-c->y));
}
static int nacre_exit_order(const void *pa,const void *pb){
  const NacreExit *a=pa,*b=pb;
  return a->score<b->score?-1:(a->score>b->score?1:0);
}
static int nacre_pair_order(const void *pa,const void *pb){
  const NacrePair *a=pa,*b=pb;
  return a->priority<b->priority?-1:(a->priority>b->priority?1:0);
}
static int nacre_special_order(const void *pa,const void *pb){
  const NacreSpecial *a=pa,*b=pb;
  return a->score>b->score?-1:(a->score<b->score?1:0);
}
static void nacre_stable_sort(void *buffer,size_t count,size_t size,int (*cmp)(const void*,const void*)) {
  /* Insertion sort: JS Array.sort is stable; our collections are small. */
  unsigned char *ptr=buffer;
  unsigned char tmp[sizeof(NacreSpecial) > sizeof(NacrePair) ?
                    sizeof(NacreSpecial) : sizeof(NacrePair)];
  for(size_t i=1;i<count;i++){
    memcpy(tmp,ptr+i*size,size);
    size_t j=i;
    while(j>0&&cmp(ptr+(j-1)*size,tmp)>0){
      memcpy(ptr+j*size,ptr+(j-1)*size,size);j--;
    }
    memcpy(ptr+j*size,tmp,size);
  }
}
static void nacre_shuffle_borders(NacreRng *rng,NacreBorder *arr,int count){
  for(int i=count-1;i>0;i--){int j=nacre_rand_int(rng,0,i);
    NacreBorder t=arr[i];arr[i]=arr[j];arr[j]=t;}
}
static int nacre_shared_borders(const NacreDungeonMap *m,const NacreDungeonRoom *a,
    const NacreDungeonRoom *b,NacreBorder *out,int limit) {
  int count=0;
  for(uint32_t k=0;k<a->cell_count;k++){
    uint32_t tile=a->cells[k];int x=nacre_x(m,tile),y=nacre_y(m,tile);
    static const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int d=0;d<4;d++){
      int nx=x+dirs[d][0],ny=y+dirs[d][1];
      if(nx<0||ny<0||nx>=(int)m->width||ny>=(int)m->height)continue;
      uint32_t n=(uint32_t)nacre_idx(m,nx,ny);
      if(m->room_ids[n]!=(int)b->id)continue;
      if(count<limit)out[count]=(NacreBorder){tile,n,x,y,dirs[d][0],dirs[d][1],0};
      count++;
    }
  }
  return count;
}
static bool nacre_far_from_special(const NacreBuild *s,uint32_t a,uint32_t b,double minimum){
  const NacreDungeonMap *m=s->map;
  double x=(nacre_x(m,a)+nacre_x(m,b))*.5,y=(nacre_y(m,a)+nacre_y(m,b))*.5;
  for(size_t i=0;i<s->reserved_count;i++){
    const NacreSpecial *sp=&s->reserved[i];
    double sx=(nacre_x(m,sp->a)+nacre_x(m,sp->b))*.5;
    double sy=(nacre_y(m,sp->a)+nacre_y(m,sp->b))*.5;
    if(fabs(x-sx)+fabs(y-sy)<minimum)return false;
  }
  return true;
}
static bool nacre_connect_adjacent(NacreBuild *s,uint32_t id_a,uint32_t id_b,bool extra,
    NacreBorder *borders,int count){
  NacreDungeonMap *m=s->map;
  const NacreDungeonRoom *a=&m->rooms[id_a];
  NacreBorder *candidates=malloc((size_t)count*sizeof(*candidates));
  if(!candidates){s->oom=true;return false;}
  int len=0;
  for(int j=0;j<count;j++){
    NacreBorder c=borders[j];
    if(!nacre_edge_exists(m,c.inside,c.outside)&&nacre_far_from_special(s,c.inside,c.outside,2.5))
      candidates[len++]=c;
  }
  if(!len){free(candidates);return false;}
  nacre_shuffle_borders(&s->rng,candidates,len);
  /* Stable sorting by first room's border distance, descending. */
  for(int i=1;i<len;i++){
    NacreBorder v=candidates[i];int j=i;
    while(j>0&&nacre_border_score(a,&candidates[j-1])<nacre_border_score(a,&v)){
      candidates[j]=candidates[j-1];j--;}
    candidates[j]=v;
  }
  NacreBorder selected[2]={candidates[0]};int sel=1;
  if(len>=4&&nacre_rand(&s->rng)<.55){
    int best=-1,dist=-1;
    for(int j=0;j<len;j++){
      int d=nacre_abs(candidates[j].x-selected[0].x)+nacre_abs(candidates[j].y-selected[0].y);
      if(d>=3&&d>dist){best=j;dist=d;}
    }
    if(best>=0)selected[sel++]=candidates[best];
  }
  NacreDungeonLink *link=nacre_new_link(s,id_a,id_b,extra,true);
  if(!link){free(candidates);return false;}
  for(int i=0;i<sel;i++){
    uint32_t u=selected[i].inside,v=selected[i].outside;
    if(!nacre_add_edge(s,u,v,NACRE_DUNGEON_EDGE_OPEN) ||
       !nacre_add_exit(s,id_a,u) || !nacre_add_exit(s,id_b,v)){
      free(candidates);return false;
    }
    link->passages[link->passage_count++]=(NacreDungeonEdge){u,v,NACRE_DUNGEON_EDGE_OPEN};
  }
  free(candidates);return true;
}
/* A* with the same noise, heuristic and priority queue as the JS baseline. */
static bool nacre_heap_push(NacreHeap *h,NacreAstarNode v){
  if(h->size==h->cap){size_t c=h->cap?h->cap*2:64;
    NacreAstarNode *p=realloc(h->items,c*sizeof(*p));
    if(!p)return false;
    h->items=p;h->cap=c;
  }
  size_t i=h->size++;while(i){size_t p=(i-1)>>1;
    if(h->items[p].f<=v.f)break;
    h->items[i]=h->items[p];i=p;}
  h->items[i]=v;return true;
}
static NacreAstarNode nacre_heap_pop(NacreHeap *h){
  NacreAstarNode root=h->items[0],last=h->items[--h->size];
  if(h->size){size_t i=0;
    while(true){size_t l=i*2+1;if(l>=h->size)break;
      size_t r=l+1,c=r<h->size&&h->items[r].f<h->items[l].f?r:l;
      if(h->items[c].f>=last.f)break;
      h->items[i]=h->items[c];i=c;
    }
    h->items[i]=last;
  }
  return root;
}
static int nacre_route_astar(NacreBuild *s,uint32_t start,uint32_t goal,
    uint32_t seed,uint32_t path[5]) {
  NacreDungeonMap *m=s->map;
  uint32_t n=m->width*m->height;
  double *best=malloc(n*sizeof(double));int32_t *prev=malloc(n*sizeof(int32_t));
  if(!best||!prev){free(best);free(prev);s->oom=true;return 0;}
  for(uint32_t i=0;i<n;i++){best[i]=INFINITY;prev[i]=-1;}
  uint32_t ne[4];unsigned ct=nacre_neighbors(m,start,ne);
  if(m->cells[start]||m->cells[goal]){free(best);free(prev);return 0;}
  for(unsigned j=0;j<ct;j++)if(m->cells[ne[j]]==NACRE_DUNGEON_CORRIDOR){
    free(best);free(prev);return 0;
  }
  NacreHeap heap={0};int len=0;
  int gx=nacre_x(m,goal),gy=nacre_y(m,goal);
  double distance=(double)(nacre_abs(nacre_x(m,start)-gx)+nacre_abs(nacre_y(m,start)-gy));
  best[start]=0;
  if(!nacre_heap_push(&heap,(NacreAstarNode){start,0,distance})){s->oom=true;goto done;}
  while(heap.size){
    NacreAstarNode cur=nacre_heap_pop(&heap);
    if(cur.g>best[cur.tile]+1e-8)continue;
    if(cur.tile==goal){
      uint32_t reversed[10000],qty=0;
      for(int32_t p=(int32_t)goal;p>=0;p=prev[p]){
        if(qty>=10000){qty=0;break;}
        reversed[qty++]=(uint32_t)p;
      }
      if(qty>=2&&qty<=5){len=(int)qty;
        for(uint32_t i=0;i<qty;i++)path[i]=reversed[qty-1-i];}
      break;
    }
    uint32_t around[4];unsigned around_count=nacre_neighbors(m,cur.tile,around);
    for(unsigned j=0;j<around_count;j++){
      uint32_t tile=around[j];int x=nacre_x(m,tile),y=nacre_y(m,tile);
      if(m->cells[tile]||!nacre_in_bounds(m,x,y))continue;
      uint32_t around2[4];unsigned c2=nacre_neighbors(m,tile,around2);
      bool neighboring_corridor=false;
      for(unsigned k=0;k<c2;k++)if(m->cells[around2[k]]==NACRE_DUNGEON_CORRIDOR){
        neighboring_corridor=true;break;
      }
      if(neighboring_corridor)continue;
      uint32_t noise=( (uint32_t)(x+37)*UINT32_C(73856093) ^
                       (uint32_t)(y+11)*UINT32_C(19349663) ^ seed );
      double g=cur.g+1.0+(double)(noise%17)/180.0;
      if(g<best[tile]-1e-8){
        best[tile]=g;prev[tile]=(int32_t)cur.tile;
        double d=nacre_abs(x-gx)+nacre_abs(y-gy);
        if(!nacre_heap_push(&heap,(NacreAstarNode){tile,g,g+d*.88})){
          s->oom=true;goto done;
        }
      }
    }
  }
done:
  free(heap.items);free(best);free(prev);return len;
}
static int nacre_exit_candidates(NacreBuild *s,const NacreDungeonRoom *room,
    const NacreDungeonRoom *target,NacreExit output[7]) {
  NacreDungeonMap *m=s->map;NacreExit candidates[128];int count=0;
  static const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  for(uint32_t k=0;k<room->cell_count;k++){
    uint32_t tile=room->cells[k];int x=nacre_x(m,tile),y=nacre_y(m,tile);
    for(int d=0;d<4;d++){
      int ex=x+dirs[d][0],ey=y+dirs[d][1];
      if(!nacre_in_bounds(m,ex,ey))continue;
      uint32_t ext=(uint32_t)nacre_idx(m,ex,ey);
      if(m->cells[ext]||!nacre_far_from_special(s,tile,ext,2.5))continue;
      double score=(double)(nacre_abs(ex-target->center_x)+nacre_abs(ey-target->center_y));
      if(dirs[d][0]&&((target->center_x-x)>0?1:((target->center_x-x)<0?-1:0))!=dirs[d][0])score+=4;
      if(dirs[d][1]&&((target->center_y-y)>0?1:((target->center_y-y)<0?-1:0))!=dirs[d][1])score+=4;
      for(uint32_t j=0;j<room->exit_count;j++)if(room->exits[j]==tile){score+=5;break;}
      score+=nacre_rand(&s->rng)*.7;
      if(count<128)candidates[count++]=(NacreExit){tile,ext,score};
    }
  }
  nacre_stable_sort(candidates,(size_t)count,sizeof(candidates[0]),nacre_exit_order);
  if(count>7)count=7;
  memcpy(output,candidates,(size_t)count*sizeof(*output));return count;
}
static bool nacre_connect_rooms(NacreBuild *s,uint32_t id_a,uint32_t id_b,bool extra,
    const NacreDungeonConfig *cfg) {
  NacreDungeonMap *m=s->map;
  NacreDungeonRoom *a=&m->rooms[id_a],*b=&m->rooms[id_b];
  NacreBorder borders[64];
  int shared=nacre_shared_borders(m,a,b,borders,64);
  if(shared>0)return nacre_connect_adjacent(s,id_a,id_b,extra,borders,nacre_min(shared,64));
  NacreExit ea[7],eb[7];
  int na=nacre_exit_candidates(s,a,b,ea),nb=nacre_exit_candidates(s,b,a,eb);
  NacrePair pairs[49];int count=0;
  for(int i=0;i<na;i++)for(int j=0;j<nb;j++){
    int dx=nacre_abs(nacre_x(m,ea[i].outside)-nacre_x(m,eb[j].outside));
    int dy=nacre_abs(nacre_y(m,ea[i].outside)-nacre_y(m,eb[j].outside));
    int dist=dx+dy;
    if(dist+1>(int)cfg->max_corridor_tiles)continue;
    bool same=nacre_edge_exists(m,ea[i].inside,ea[i].outside)||
       nacre_edge_exists(m,eb[j].inside,eb[j].outside);
    pairs[count++]=(NacrePair){ea[i],eb[j],dist+ea[i].score*.12+eb[j].score*.12+
      (same?7:0)+nacre_rand(&s->rng)*2};
  }
  nacre_stable_sort(pairs,(size_t)count,sizeof(pairs[0]),nacre_pair_order);
  int best_len=0;NacrePair best_pair={0};uint32_t best_path[5]={0};
  for(int j=0;j<nacre_min(24,count);j++){
    uint32_t path[5];int n=nacre_route_astar(s,pairs[j].a.outside,pairs[j].b.outside,
      m->seed_hash,path);
    if(s->oom)return false;
    if(n<2||n>(int)cfg->max_corridor_tiles)continue;
    if(!best_len||n<best_len){best_len=n;best_pair=pairs[j];memcpy(best_path,path,(size_t)n*sizeof(uint32_t));}
    if(j>=6&&best_len)break;
  }
  if(!best_len)return false;
  NacreDungeonLink *link=nacre_new_link(s,id_a,id_b,extra,false);
  if(!link)return false;
  for(int i=0;i<best_len;i++){m->cells[best_path[i]]=NACRE_DUNGEON_CORRIDOR;
    link->path[i]=best_path[i];}
  link->path_count=(uint32_t)best_len;
  NacreDungeonEdge ends[2]={{best_pair.a.inside,best_pair.a.outside,NACRE_DUNGEON_EDGE_OPEN},
    {best_pair.b.inside,best_pair.b.outside,NACRE_DUNGEON_EDGE_OPEN}};
  for(int k=0;k<2;k++){
    if(!nacre_edge_exists(m,ends[k].a,ends[k].b)){
      if(!nacre_add_edge(s,ends[k].a,ends[k].b,NACRE_DUNGEON_EDGE_OPEN))return false;
      link->passages[link->passage_count++]=ends[k];
    }
  }
  if(!nacre_add_exit(s,id_a,best_pair.a.inside)||!nacre_add_exit(s,id_b,best_pair.b.inside))
    return false;
  return true;
}
static int nacre_find_parent(uint32_t parent[NACRE_DUNGEON_MAX_ROOMS],int i){
  while(parent[i]!=(uint32_t)i){parent[i]=parent[parent[i]];i=(int)parent[i];}
  return i;
}
static bool nacre_traversable(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  if(!m->cells[a]||!m->cells[b])return false;
  int ra=m->room_ids[a],rb=m->room_ids[b];
  if(ra==rb)return true;
  if(ra==-1&&rb==-1)return true;
  return nacre_edge_exists(m,a,b);
}
static int nacre_bfs(const NacreDungeonMap *m,uint32_t start,int32_t *dist,int32_t *prev,
    uint32_t ignored_a,uint32_t ignored_b) {
  uint32_t n=m->width*m->height;
  for(uint32_t i=0;i<n;i++){dist[i]=-1;if(prev)prev[i]=-1;}
  uint32_t *queue=malloc((size_t)n*sizeof(uint32_t));if(!queue)return -1;
  uint32_t head=0,tail=0;queue[tail++]=start;dist[start]=0;
  while(head<tail){uint32_t tile=queue[head++];
    uint32_t adjacent[4];unsigned nn=nacre_neighbors(m,tile,adjacent);
    for(unsigned k=0;k<nn;k++){
      uint32_t v=adjacent[k];
      if(dist[v]>=0)continue;
      if(ignored_a!=UINT32_MAX &&
         ((tile==ignored_a&&v==ignored_b)||(tile==ignored_b&&v==ignored_a)))
        continue;
      if(!nacre_traversable(m,tile,v))continue;
      dist[v]=dist[tile]+1;if(prev)prev[v]=(int32_t)tile;queue[tail++]=v;
    }
  }
  free(queue);return (int)tail;
}
static int nacre_alt_distance(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  int32_t *dist=malloc((size_t)m->width*m->height*sizeof(int32_t));
  if(!dist)return -1;
  (void)nacre_bfs(m,a,dist,NULL,UINT32_MAX,UINT32_MAX);
  int result=dist[b];free(dist);return result;
}
static bool nacre_choose_endpoints(NacreBuild *s){
  NacreDungeonMap *m=s->map;uint32_t reps[NACRE_DUNGEON_MAX_ROOMS];
  uint32_t n=m->width*m->height;
  int32_t *dist=malloc(n*sizeof(int32_t)),*prev=malloc(n*sizeof(int32_t));
  if(!dist||!prev){free(dist);free(prev);s->oom=true;return false;}
  for(uint32_t r=0;r<m->room_count;r++){
    const NacreDungeonRoom *room=&m->rooms[r];
    int best_dist=INT_MAX;uint32_t best=UINT32_MAX;
    for(uint32_t k=0;k<room->cell_count;k++){
      uint32_t tile=room->cells[k];
      int d=nacre_abs(nacre_x(m,tile)-room->center_x)+nacre_abs(nacre_y(m,tile)-room->center_y);
      if(d<best_dist){best_dist=d;best=tile;}
    }
    reps[r]=best;
  }
  uint32_t far=0;
  for(int pass=0;pass<2;pass++){
    int reached=nacre_bfs(m,reps[far],dist,NULL,UINT32_MAX,UINT32_MAX);
    if(reached<0){s->oom=true;free(dist);free(prev);return false;}
    uint32_t current=far;
    for(uint32_t i=0;i<m->room_count;i++)if(dist[reps[i]]>dist[reps[current]])current=i;
    if(pass==0)m->entry=reps[current];
    far=current;
  }
  m->exit=reps[far];
  int reached=nacre_bfs(m,m->entry,dist,prev,UINT32_MAX,UINT32_MAX);
  if(reached<0){s->oom=true;free(dist);free(prev);return false;}
  if(dist[m->exit]<0){free(dist);free(prev);return false;}
  for(uint32_t i=0;i<m->room_count;i++){
    bool visit=false;
    for(uint32_t k=0;k<m->rooms[i].cell_count;k++)if(dist[m->rooms[i].cells[k]]>=0){visit=true;break;}
    if(!visit){free(dist);free(prev);return false;}
  }
  m->shortest_path_length=(uint32_t)dist[m->exit];
  m->shortest_route=malloc(((size_t)m->shortest_path_length+1)*sizeof(uint32_t));
  if(!m->shortest_route){s->oom=true;free(dist);free(prev);return false;}
  m->shortest_route_count=m->shortest_path_length+1;
  uint32_t tile=m->exit;
  for(int i=(int)m->shortest_path_length;i>=0;i--){
    m->shortest_route[i]=tile;if(prev[tile]>=0)tile=(uint32_t)prev[tile];
  }
  free(dist);free(prev);return true;
}
static void nacre_update_bounds(const NacreDungeonMap *m,NacreDungeonRoom *r){
  int x0=INT_MAX,y0=INT_MAX,x1=0,y1=0;
  for(uint32_t k=0;k<r->cell_count;k++){
    int x=nacre_x(m,r->cells[k]),y=nacre_y(m,r->cells[k]);
    x0=nacre_min(x0,x);y0=nacre_min(y0,y);
    x1=nacre_max(x1,x+1);y1=nacre_max(y1,y+1);
  }
  r->x=x0;r->y=y0;r->width=x1-x0;r->height=y1-y0;
}
static void nacre_normalize_micro_corridors(NacreBuild *s){
  NacreDungeonMap *m=s->map;
  for(uint32_t i=0;i<m->link_count;i++){
    NacreDungeonLink *link=&m->links[i];
    if(link->direct||link->passage_count!=2||link->path_count<1||link->path_count>2)continue;
    NacreDungeonRoom *a=&m->rooms[link->room_a],*b=&m->rooms[link->room_b];
    uint32_t first=link->path[0],last=link->path[link->path_count-1];
    uint32_t room_a=link->passages[0].a,room_b=link->passages[1].a;
    if(link->passages[0].b!=first||link->passages[1].b!=last||
       m->room_ids[room_a]!=(int)link->room_a||
       m->room_ids[room_b]!=(int)link->room_b||
       !nacre_edge_exists(m,room_a,first)||!nacre_edge_exists(m,room_b,last))continue;
    bool linear=true;
    for(uint32_t k=0;k<link->path_count;k++){
      uint32_t tile=link->path[k];
      uint32_t before=k?link->path[k-1]:room_a;
      uint32_t after=k+1<link->path_count?link->path[k+1]:room_b;
      if(m->cells[tile]!=NACRE_DUNGEON_CORRIDOR||!nacre_near_grid(m,tile,before)||
         !nacre_near_grid(m,tile,after)){linear=false;break;}
      uint32_t ns[4];unsigned nn=nacre_neighbors(m,tile,ns);
      for(unsigned j=0;j<nn;j++)if(m->cells[ns[j]]==NACRE_DUNGEON_CORRIDOR &&
         ns[j]!=before&&ns[j]!=after){linear=false;break;}
    }
    if(!linear || nacre_edge_special(m,room_a,first)||nacre_edge_special(m,room_b,last))continue;
    nacre_remove_edge(m,room_a,first);nacre_remove_edge(m,room_b,last);
    if(link->path_count==1){
      m->cells[first]=NACRE_DUNGEON_ROOM;m->room_ids[first]=(int16_t)link->room_a;
      if(!nacre_add_room_cell(s,link->room_a,first))return;
      (void)nacre_add_edge(s,first,room_b,NACRE_DUNGEON_EDGE_OPEN);
      (void)nacre_add_exit(s,link->room_a,first);
      (void)nacre_add_exit(s,link->room_b,room_b);
      link->passages[0]=(NacreDungeonEdge){first,room_b,NACRE_DUNGEON_EDGE_OPEN};
    }else{
      m->cells[first]=NACRE_DUNGEON_ROOM;m->room_ids[first]=(int16_t)link->room_a;
      m->cells[last]=NACRE_DUNGEON_ROOM;m->room_ids[last]=(int16_t)link->room_b;
      if(!nacre_add_room_cell(s,link->room_a,first)||!nacre_add_room_cell(s,link->room_b,last))return;
      (void)nacre_add_edge(s,first,last,NACRE_DUNGEON_EDGE_OPEN);
      (void)nacre_add_exit(s,link->room_a,first);
      (void)nacre_add_exit(s,link->room_b,last);
      link->passages[0]=(NacreDungeonEdge){first,last,NACRE_DUNGEON_EDGE_OPEN};
    }
    link->path_count=0;link->passage_count=1;link->direct=true;link->normalized_micro_corridor=true;
    nacre_update_bounds(m,a);nacre_update_bounds(m,b);
    m->normalized_micro_corridors++;
  }
}
/* Rest of phase: special walls and additional loops are added below. */
static bool nacre_reserved_append(NacreBuild *s,NacreSpecial special){
  if(s->reserved_count==s->reserved_capacity){
    size_t next=s->reserved_capacity?s->reserved_capacity*2:8;
    NacreSpecial *p=realloc(s->reserved,next*sizeof(*p));
    if(!p){s->oom=true;return false;}
    s->reserved=p;s->reserved_capacity=next;
  }
  s->reserved[s->reserved_count++]=special;return true;
}
static bool nacre_pair_has_link(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->link_count;i++)if(m->links[i].direct &&
      ((m->links[i].room_a==a&&m->links[i].room_b==b)||
       (m->links[i].room_a==b&&m->links[i].room_b==a)))return true;
  return false;
}
static bool nacre_pair_is_reserved(const NacreBuild *s,uint32_t a,uint32_t b){
  for(size_t i=0;i<s->reserved_count;i++){
    const NacreSpecial *r=&s->reserved[i];
    if((r->ra==a&&r->rb==b)||(r->ra==b&&r->rb==a))return true;
  }
  return false;
}
static bool nacre_specials_append(NacreSpecial **items,size_t *count,size_t *capacity,
    NacreSpecial value){
  if(*count==*capacity){
    size_t next=*capacity?*capacity*2:32;
    NacreSpecial *p=realloc(*items,next*sizeof(**items));
    if(!p)return false;
    *items=p;*capacity=next;
  }
  (*items)[(*count)++]=value;return true;
}
static bool nacre_add_special_walls(NacreBuild *s,int budget,double crack_fraction){
  if(budget<=0)return true;
  NacreDungeonMap *m=s->map;
  NacreSpecial *raw=NULL,*scored=NULL;size_t raw_n=0,raw_cap=0,scored_n=0,scored_cap=0;
  /* Raw boundaries in insertion order (y, x, right/down), as in v21. */
  for(int y=1;y<(int)m->height-1;y++)for(int x=1;x<(int)m->width-1;x++){
    uint32_t a=(uint32_t)nacre_idx(m,x,y);int ra=m->room_ids[a];
    if(ra<0||m->cells[a]!=NACRE_DUNGEON_ROOM)continue;
    const int dx[2]={1,0},dy[2]={0,1};
    for(int d=0;d<2;d++){
      uint32_t b=(uint32_t)nacre_idx(m,x+dx[d],y+dy[d]);int rb=m->room_ids[b];
      if(rb<0||ra==rb||m->cells[b]!=NACRE_DUNGEON_ROOM||nacre_edge_exists(m,a,b))continue;
      if(!nacre_specials_append(&raw,&raw_n,&raw_cap,
          (NacreSpecial){a,b,(uint32_t)ra,(uint32_t)rb,0,0,0,0})){
        s->oom=true;goto failure;
      }
    }
  }
  /* Map iteration order in JS: first appearance of each unordered room pair. */
  for(size_t i=0;i<raw_n;i++){
    const NacreSpecial *r=&raw[i];bool pair_seen=false;
    for(size_t j=0;j<i;j++)if((raw[j].ra==r->ra&&raw[j].rb==r->rb)||
       (raw[j].rb==r->ra&&raw[j].ra==r->rb)){pair_seen=true;break;}
    if(pair_seen)continue;
    if(nacre_pair_has_link(m,r->ra,r->rb))continue;
    for(size_t j=i;j<raw_n;j++){
      NacreSpecial c=raw[j];
      if(!((c.ra==r->ra&&c.rb==r->rb)||(c.rb==r->ra&&c.ra==r->rb)))continue;
      c.cx=(nacre_x(m,c.a)+nacre_x(m,c.b))*.5;
      c.cy=(nacre_y(m,c.a)+nacre_y(m,c.b))*.5;
      double clearance=INFINITY;
      for(uint32_t e=0;e<m->edge_count;e++){
        const NacreDungeonEdge *edge=&m->edges[e];
        if(edge->type==NACRE_DUNGEON_EDGE_CRACKED)continue;
        double cx=(nacre_x(m,edge->a)+nacre_x(m,edge->b))*.5;
        double cy=(nacre_y(m,edge->a)+nacre_y(m,edge->b))*.5;
        double d=fabs(c.cx-cx)+fabs(c.cy-cy);
        if(d<clearance)clearance=d;
      }
      if(clearance<2.5)continue;
      int detour=nacre_alt_distance(m,c.a,c.b);
      if(detour<8)continue;
      const NacreDungeonRoom *a=&m->rooms[c.ra],*b=&m->rooms[c.rb];
      int px=nacre_x(m,c.a),py=nacre_y(m,c.a);
      int qx=nacre_x(m,c.b),qy=nacre_y(m,c.b);
      int side_a=nacre_min(nacre_min(px-a->x,a->x+a->width-1-px),
         nacre_min(py-a->y,a->y+a->height-1-py));
      int side_b=nacre_min(nacre_min(qx-b->x,b->x+b->width-1-qx),
         nacre_min(qy-b->y,b->y+b->height-1-qy));
      c.score=fmin(detour,50)*.55+fmin(clearance,8)*3+
        nacre_min(side_a,side_b)*2+nacre_rand(&s->rng)*4;
      c.detour=detour;
      if(!nacre_specials_append(&scored,&scored_n,&scored_cap,c)){
        s->oom=true;goto failure;
      }
    }
  }
  nacre_stable_sort(scored,scored_n,sizeof(*scored),nacre_special_order);
  int cracks=0,holes=0,requested_cracks=(int)floor(budget*crack_fraction+.5);
  int requested_holes=budget-requested_cracks;
  for(size_t i=0;i<scored_n&&cracks+holes<budget;i++){
    const NacreSpecial c=scored[i];
    if(nacre_pair_is_reserved(s,c.ra,c.rb))continue;
    bool too_close=false;
    for(size_t j=0;j<s->reserved_count;j++){
      NacreSpecial v=s->reserved[j];
      if(fabs(v.cx-c.cx)+fabs(v.cy-c.cy)<4){too_close=true;break;}
    }
    if(too_close)continue;
    int detour=nacre_alt_distance(m,c.a,c.b);
    if(detour<8)continue;
    bool cracked=false;
    if(cracks<requested_cracks){
      cracked=holes>=requested_holes || nacre_rand(&s->rng)<.58;
    }
    if(cracked){
      if(!nacre_add_edge(s,c.a,c.b,NACRE_DUNGEON_EDGE_CRACKED))goto failure;
      cracks++;
    }else{
      if(!nacre_add_edge(s,c.a,c.b,NACRE_DUNGEON_EDGE_HOLE))goto failure;
      NacreDungeonLink *link=nacre_new_link(s,c.ra,c.rb,true,true);
      if(!link)goto failure;
      link->hole=true;
      link->passage_count=1;
      link->passages[0]=(NacreDungeonEdge){c.a,c.b,NACRE_DUNGEON_EDGE_HOLE};
      holes++;
    }
    NacreSpecial chosen=c;chosen.detour=detour;
    if(!nacre_reserved_append(s,chosen))goto failure;
  }
  free(raw);free(scored);return true;
failure:
  free(raw);free(scored);return false;
}

void nacre_dungeon_generator_config_defaults(NacreDungeonConfig *config){
  if(!config)return;
  memset(config,0,sizeof(*config));
  memcpy(config->seed,"LDK-001",8);
  config->width=50;config->height=50;
  config->rooms_min=18;config->rooms_max=22;
  config->max_room_size=6;config->max_corridor_tiles=5;
  config->extra_connections=.75;
  config->irregular_rooms=0.0;
  config->adjacent_rooms=.75;
  config->alternative_walls=.50;
  config->cracked_fraction=.50;
}
/* Share the exact validation rules with the LDK inspector adapter. Each
 * message describes one field; NULL means the configuration is valid. */
const char *nacre_dungeon_generator_config_error(const NacreDungeonConfig *cfg){
  if(!cfg)return "Null dungeon configuration";
  if(!memchr(cfg->seed,'\0',sizeof(cfg->seed)))return "Seed not terminated";
  if(!cfg->seed[0])return "Seed cannot be empty";
  if(cfg->width<24||cfg->width>100)return "Width must be 24..100";
  if(cfg->height<20||cfg->height>80)return "Height must be 20..80";
  if(cfg->rooms_min<8||cfg->rooms_min>32)return "Rooms Min must be 8..32";
  if(cfg->rooms_max<8||cfg->rooms_max>32)return "Rooms Max must be 8..32";
  if(cfg->rooms_min>cfg->rooms_max)return "Rooms Min exceeds Rooms Max";
  if(cfg->max_room_size<5||cfg->max_room_size>12)
    return "Max Room Size must be 5..12";
  if(cfg->max_corridor_tiles<2||cfg->max_corridor_tiles>5)
    return "Max Corridor Tiles must be 2..5";
  if(!(cfg->extra_connections>=0&&cfg->extra_connections<=.75))
    return "Extra Connections must be 0..0.75";
  if(!(cfg->irregular_rooms>=0&&cfg->irregular_rooms<=.6))
    return "Irregular Rooms must be 0..0.6";
  if(!(cfg->adjacent_rooms>=0&&cfg->adjacent_rooms<=1))
    return "Adjacent Rooms must be 0..1";
  if(!(cfg->alternative_walls>=0&&cfg->alternative_walls<=1))
    return "Alternative Walls must be 0..1";
  if(!(cfg->cracked_fraction>=0&&cfg->cracked_fraction<=1))
    return "Cracked Fraction must be 0..1";
  return NULL;
}

void nacre_dungeon_generator_release(NacreDungeonMap *map){
  if(!map)return;
  if(map->rooms)for(uint32_t i=0;i<map->room_count;i++){
    free(map->rooms[i].cells);free(map->rooms[i].exits);
  }
  free(map->rooms);free(map->links);free(map->edges);
  free(map->cells);free(map->room_ids);free(map->shortest_route);
  memset(map,0,sizeof(*map));
}
NacreDungeonResult nacre_dungeon_generator_generate(
    const NacreDungeonConfig *cfg,NacreDungeonMap *out){
  if(!out)return NACRE_DUNGEON_INVALID_CONFIG;
  memset(out,0,sizeof(*out));
  if(nacre_dungeon_generator_config_error(cfg))return NACRE_DUNGEON_INVALID_CONFIG;
  const uint32_t n=cfg->width*cfg->height;
  NacreBuild s={0};s.map=out;
  out->width=cfg->width;out->height=cfg->height;
  out->seed_hash=nacre_seed_hash(cfg->seed);
  out->cells=calloc(n,1);out->room_ids=malloc(n*sizeof(int16_t));
  out->rooms=calloc(NACRE_DUNGEON_MAX_ROOMS,sizeof(*out->rooms));
  s.room_capacity=calloc(NACRE_DUNGEON_MAX_ROOMS,sizeof(uint32_t));
  s.exit_capacity=calloc(NACRE_DUNGEON_MAX_ROOMS,sizeof(uint32_t));
  if(!out->cells||!out->room_ids||!out->rooms||!s.room_capacity||!s.exit_capacity){
    s.oom=true;goto failed;
  }
  for(uint32_t i=0;i<n;i++)out->room_ids[i]=-1;
  s.rng.state=out->seed_hash;
  NacreRng count_rng={nacre_hash_suffix(cfg->seed,"|room-count-v16")};
  out->requested_room_count=(uint32_t)nacre_rand_int(&count_rng,
      (int)cfg->rooms_min,(int)cfg->rooms_max);
  count_rng.state=nacre_hash_suffix(cfg->seed,"|totem-count-v16");
  out->requested_totem_count=(uint32_t)nacre_rand_int(&count_rng,1,3);
  const uint32_t main_count=out->requested_room_count-out->requested_totem_count;
  for(uint32_t i=0;i<main_count;i++){
    if(!nacre_add_room(&s,cfg))goto failed;
  }
  if(out->room_count!=main_count||out->room_count<3)goto failed;
  size_t count=(size_t)out->room_count*(out->room_count-1)/2;
  NacreCandidate *candidates=malloc(count*sizeof(*candidates));
  if(!candidates){s.oom=true;goto failed;}
  uint32_t parents[NACRE_DUNGEON_MAX_ROOMS];
  for(uint32_t i=0;i<out->room_count;i++)parents[i]=i;
  size_t k=0;
  for(uint32_t i=0;i<out->room_count;i++)for(uint32_t j=i+1;j<out->room_count;j++){
    const NacreDungeonRoom *a=&out->rooms[i],*b=&out->rooms[j];
    int gap=nacre_room_gap(a,b);
    int center=nacre_abs(a->center_x-b->center_x)+nacre_abs(a->center_y-b->center_y);
    candidates[k++]=(NacreCandidate){i,j,gap,gap*20+center*.13+nacre_rand(&s.rng)*1.5};
  }
  nacre_stable_sort(candidates,count,sizeof(*candidates),nacre_candidate_order);
  bool used[NACRE_DUNGEON_MAX_ROOMS][NACRE_DUNGEON_MAX_ROOMS]={0};
  uint32_t linked=0;
  for(size_t i=0;i<count;i++){
    NacreCandidate e=candidates[i];
    int a=nacre_find_parent(parents,(int)e.a),b=nacre_find_parent(parents,(int)e.b);
    if(a==b)continue;
    if(!nacre_connect_rooms(&s,e.a,e.b,false,cfg)){
      if(s.oom)break;
      continue;
    }
    parents[a]=(uint32_t)b;used[e.a][e.b]=used[e.b][e.a]=true;
    if(++linked==out->room_count-1)break;
  }
  if(s.oom){free(candidates);goto failed;}
  if(linked!=out->room_count-1){free(candidates);goto failed;}
  const uint32_t corridor_limit=(uint32_t)nacre_max(2,(int)ceil(out->room_count*.30));
  bool compact=cfg->adjacent_rooms>=.65&&out->room_count>=12;
  uint32_t corridor_count=0;
  for(uint32_t i=0;i<out->link_count;i++)if(!out->links[i].direct)corridor_count++;
  if(compact&&corridor_count>corridor_limit){free(candidates);goto failed;}
  /* Same JS Math.round(x) semantics (nonnegative x). */
  int extra_budget=(int)floor(cfg->extra_connections*out->room_count*.56+.5);
  int specials_budget=(int)floor(extra_budget*cfg->alternative_walls+.5);
  if(!nacre_add_special_walls(&s,specials_budget,cfg->cracked_fraction)){
    free(candidates);goto failed;
  }
  int hole_count=0;
  for(uint32_t i=0;i<out->edge_count;i++)if(out->edges[i].type==NACRE_DUNGEON_EDGE_HOLE)hole_count++;
  int budget=extra_budget-hole_count;
  for(size_t i=0;i<count&&budget>0;i++){
    NacreCandidate e=candidates[i];
    if(used[e.a][e.b]||nacre_pair_is_reserved(&s,e.a,e.b))continue;
    if(e.gap>(int)cfg->max_corridor_tiles+2)continue;
    NacreBorder shared[64];
    int shared_n=nacre_shared_borders(out,&out->rooms[e.a],&out->rooms[e.b],shared,64);
    if(compact&&corridor_count>=corridor_limit&&!shared_n)continue;
    if(nacre_connect_rooms(&s,e.a,e.b,true,cfg)){
      used[e.a][e.b]=used[e.b][e.a]=true;budget--;
      if(!out->links[out->link_count-1].direct)corridor_count++;
    }
    if(s.oom)break;
  }
  free(candidates);
  if(s.oom)goto failed;
  nacre_normalize_micro_corridors(&s);
  if(s.oom||!nacre_choose_endpoints(&s))goto failed;
  free(s.room_capacity);free(s.exit_capacity);free(s.reserved);
  if(!nacre_dungeon_generator_validate(out)){
    nacre_dungeon_generator_release(out);return NACRE_DUNGEON_INVALID_LAYOUT;
  }
  return NACRE_DUNGEON_OK;
failed:
  free(s.room_capacity);free(s.exit_capacity);free(s.reserved);
  nacre_dungeon_generator_release(out);
  return s.oom?NACRE_DUNGEON_OUT_OF_MEMORY:NACRE_DUNGEON_NO_VALID_LAYOUT;
}
bool nacre_dungeon_generator_validate(const NacreDungeonMap *m){
  if(!m||!m->cells||!m->room_ids||!m->rooms||!m->links||!m->edges||
     m->room_count==0||m->room_count>NACRE_DUNGEON_MAX_ROOMS)return false;
  uint32_t n=m->width*m->height;
  uint8_t *seen=calloc(n,1);if(!seen)return false;
  bool valid=true;
  for(uint32_t r=0;r<m->room_count&&valid;r++){
    const NacreDungeonRoom *room=&m->rooms[r];
    for(uint32_t k=0;k<room->cell_count;k++){
      uint32_t i=room->cells[k];
      if(i>=n||seen[i]||m->cells[i]!=NACRE_DUNGEON_ROOM||m->room_ids[i]!=(int)r||
         nacre_x(m,i)<room->x||nacre_x(m,i)>=room->x+room->width||
         nacre_y(m,i)<room->y||nacre_y(m,i)>=room->y+room->height){valid=false;break;}
      seen[i]=1;
    }
  }
  for(uint32_t i=0;i<n&&valid;i++){
    if((m->cells[i]==NACRE_DUNGEON_ROOM)!=!!seen[i])valid=false;
  }
  for(uint32_t e=0;e<m->edge_count&&valid;e++){
    const NacreDungeonEdge *edge=&m->edges[e];
    if(edge->a>=n||edge->b>=n||!m->cells[edge->a]||!m->cells[edge->b]||
       !nacre_near_grid(m,edge->a,edge->b))valid=false;
    for(uint32_t j=0;j<e;j++)if(nacre_edge_order(&m->edges[j],edge->a,edge->b))valid=false;
  }
  for(uint32_t l=0;l<m->link_count&&valid;l++){
    const NacreDungeonLink *link=&m->links[l];
    if(link->room_a>=m->room_count||link->room_b>=m->room_count||
       link->passage_count==0||link->passage_count>2||
       (!link->direct&&(link->path_count<3||link->path_count>5)))valid=false;
    for(uint32_t p=0;p<link->passage_count&&valid;p++){
      NacreDungeonEdge e=link->passages[p];
      if(e.a>=n||e.b>=n||!nacre_edge_exists(m,e.a,e.b))valid=false;
    }
  }
  if(m->entry>=n||m->exit>=n||!m->shortest_route||!m->shortest_route_count||
     m->shortest_route_count>n||
     m->shortest_route[0]!=m->entry||
     m->shortest_route[m->shortest_route_count-1]!=m->exit||
     m->shortest_route_count!=m->shortest_path_length+1)valid=false;
  if(valid)for(uint32_t k=1;k<m->shortest_route_count;k++){
    uint32_t a=m->shortest_route[k-1],b=m->shortest_route[k];
    if(a>=n||b>=n||!nacre_near_grid(m,a,b)||!nacre_traversable(m,a,b)){
      valid=false;break;
    }
  }
  if(valid){
    int32_t *dist=malloc((size_t)n*sizeof(int32_t));
    if(!dist)valid=false;
    else {
      if(nacre_bfs(m,m->entry,dist,NULL,UINT32_MAX,UINT32_MAX)<0 ||
         dist[m->exit]!=(int32_t)m->shortest_path_length)valid=false;
      for(uint32_t r=0;r<m->room_count&&valid;r++){
        bool reachable=false;
        for(uint32_t k=0;k<m->rooms[r].cell_count;k++)
          if(dist[m->rooms[r].cells[k]]>=0){reachable=true;break;}
        if(!reachable)valid=false;
      }
      free(dist);
    }
  }
  free(seen);return valid;
}
