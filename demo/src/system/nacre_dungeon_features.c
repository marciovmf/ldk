/**
 * @file nacre_dungeon_features.c
 * @brief Optional v21 geometric features, independent of LDK and Comet.
 *
 * Phase order matters: secret rooms, cave tunnels, then blocking pillars.
 * None of these phases changes the ordinary room graph, selected endpoints,
 * or the main route. The C map's main room_count intentionally excludes
 * hidden rooms: a secret entrance is not needed to finish the dungeon.
 */
#include "nacre_dungeon_features.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

typedef struct NacreFeatRng { uint32_t state; } NacreFeatRng;
static double frand01(NacreFeatRng *r) {
  r->state+=UINT32_C(0x6d2b79f5);
  uint32_t t=r->state;
  t=(t^(t>>15))*(t|1u);
  t^=t+(t^(t>>7))*(t|61u);
  return (double)(t^(t>>14))/4294967296.0;
}
static int fri(NacreFeatRng *r,int lo,int hi) {
  return lo+(int)floor(frand01(r)*(double)(hi-lo+1));
}
/* Continue FNV-1a from the exact UTF-16 seed hash already computed by the
 * base generator. All suffixes below contain ASCII only. */
static uint32_t suffix_hash(uint32_t hash,const char *suffix) {
  for(const unsigned char *p=(const unsigned char*)suffix;*p;p++)
    hash=(hash^*p)*UINT32_C(16777619);
  return hash;
}
static uint32_t weighted_count(NacreFeatRng *r,const int *weights,int max) {
  int total=0;
  for(int i=0;i<=max;i++)total+=weights[i];
  double v=frand01(r)*total;
  for(int i=0;i<=max;i++)if((v-=weights[i])<0)return (uint32_t)i;
  return (uint32_t)max;
}
static int fx(const NacreDungeonMap *m,uint32_t t){return (int)(t%m->width);}
static int fy(const NacreDungeonMap *m,uint32_t t){return (int)(t/m->width);}
static uint32_t fi(const NacreDungeonMap *m,int x,int y){return (uint32_t)(y*(int)m->width+x);}
static int abs_i(int v){return v<0?-v:v;}
static int min_i(int a,int b){return a<b?a:b;}
static int max_i(int a,int b){return a>b?a:b;}
static uint32_t neighbors(const NacreDungeonMap *m,uint32_t tile,uint32_t n[4]){
  uint32_t x=tile%m->width,y=tile/m->width,count=0;
  if(x+1<m->width)n[count++]=tile+1;
  if(x)n[count++]=tile-1;
  if(y+1<m->height)n[count++]=tile+m->width;
  if(y)n[count++]=tile-m->width;
  return count;
}
static bool equal_edge(const NacreDungeonEdge *e,uint32_t a,uint32_t b){
  return (e->a==a&&e->b==b)||(e->a==b&&e->b==a);
}
static const NacreDungeonEdge *edge_find(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t k=0;k<m->edge_count;k++)if(equal_edge(&m->edges[k],a,b))return &m->edges[k];
  return NULL;
}
static bool edge_push(NacreDungeonMap *m,uint32_t a,uint32_t b,NacreDungeonEdgeType type){
  if(edge_find(m,a,b))return false;
  void *memory=realloc(m->edges,(size_t)(m->edge_count+1)*sizeof(*m->edges));
  if(!memory)return false;
  m->edges=memory;
  if(a>b){uint32_t t=a;a=b;b=t;}
  m->edges[m->edge_count++]=(NacreDungeonEdge){a,b,type};
  return true;
}
static bool can_cross(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  if(!m->cells[a]||!m->cells[b]||
     m->cells[a]==NACRE_DUNGEON_SECRET||m->cells[b]==NACRE_DUNGEON_SECRET||
     (m->pillar_mask&&m->pillar_mask[b]))return false;
  if(m->room_ids[a]==m->room_ids[b])return true;
  const NacreDungeonEdge *e=edge_find(m,a,b);
  return e&&(e->type==NACRE_DUNGEON_EDGE_OPEN||e->type==NACRE_DUNGEON_EDGE_HOLE);
}
static int bfs(const NacreDungeonMap *m,uint32_t start,uint32_t end,uint8_t *visited){
  uint32_t n=m->width*m->height;
  int32_t *dist=malloc((size_t)n*sizeof(*dist));
  uint32_t *queue=malloc((size_t)n*sizeof(*queue));
  if(!dist||!queue){free(dist);free(queue);return -2;}
  for(uint32_t i=0;i<n;i++)dist[i]=-1;
  uint32_t head=0,tail=0;
  dist[start]=0;queue[tail++]=start;
  while(head<tail){
    uint32_t at=queue[head++],ns[4];
    uint32_t count=neighbors(m,at,ns);
    for(uint32_t j=0;j<count;j++){
      uint32_t to=ns[j];
      if(dist[to]>=0||!can_cross(m,at,to))continue;
      dist[to]=dist[at]+1;queue[tail++]=to;
    }
  }
  if(visited)for(uint32_t i=0;i<n;i++)visited[i]=(uint8_t)(dist[i]>=0);
  int result=end<n?dist[end]:0;
  free(dist);free(queue);return result;
}

typedef struct SecretOption {
  int x,y,w,h,contact_count,sides,halls;
  double score;
  uint32_t ordinal;
} SecretOption;
static int secret_cmp(const void *a,const void *b){
  const SecretOption *x=a,*y=b;
  if(x->score>y->score)return -1;
  if(x->score<y->score)return 1;
  return x->ordinal<y->ordinal?-1:x->ordinal>y->ordinal?1:0;
}
typedef struct SecretContact {uint32_t inside,outside;bool hall;} SecretContact;
static bool place_secrets(NacreDungeonMap *m,NacreFeatRng *r,uint32_t desired){
  const int dims[6][2]={{2,2},{2,3},{3,2},{3,3},{2,4},{4,2}};
  if(!desired)return true;
  m->secret_rooms=calloc(desired,sizeof(*m->secret_rooms));
  if(!m->secret_rooms)return false;
  for(uint32_t iteration=0;iteration<desired;iteration++){
    SecretOption *options=NULL;uint32_t count=0,capacity=0;
    for(uint32_t shape=0;shape<6;shape++){
      int w=dims[shape][0],h=dims[shape][1];
      for(int y=2;y<=(int)m->height-h-3;y++)for(int x=2;x<=(int)m->width-w-3;x++){
        bool free_area=true,near_secret=false;
        int contacts=0,sides=0,hall_contacts=0;
        for(int yy=y;yy<y+h&&free_area;yy++)for(int xx=x;xx<x+w;xx++)
          if(m->cells[fi(m,xx,yy)]){free_area=false;break;}
        if(!free_area)continue;
        for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){
          static const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
          for(int d=0;d<4;d++){
            int nx=xx+dx[d],ny=yy+dy[d];
            if(nx>=x&&nx<x+w&&ny>=y&&ny<y+h)continue;
            uint8_t t=m->cells[fi(m,nx,ny)];
            if(t==NACRE_DUNGEON_SECRET){near_secret=true;continue;}
            if(t==NACRE_DUNGEON_ROOM||t==NACRE_DUNGEON_CORRIDOR){
              sides|=1<<d;contacts++;if(t==NACRE_DUNGEON_CORRIDOR)hall_contacts++;
            }
          }
        }
        if(near_secret||!contacts)continue;
        int side_count=0;
        for(int i=0;i<4;i++)if(sides&(1<<i))side_count++;
        double score=side_count*14+min_i(contacts,12)*2+min_i(hall_contacts,6)*1.5+
                     (w*h)*.3+frand01(r)*9;
        if(count==capacity){
          uint32_t next=capacity?capacity*2:128;
          void *p=realloc(options,(size_t)next*sizeof(*options));
          if(!p){free(options);return false;}
          options=p;capacity=next;
        }
        options[count]=(SecretOption){x,y,w,h,contacts,sides,hall_contacts,score,count};count++;
      }
    }
    if(!count){free(options);break;}
    qsort(options,count,sizeof(*options),secret_cmp);
    SecretOption opt=options[fri(r,0,min_i(14,(int)count-1))];free(options);
    SecretContact *choices=malloc((size_t)opt.contact_count*sizeof(*choices));
    if(!choices)return false;
    uint32_t k=0;
    for(int yy=opt.y;yy<opt.y+opt.h;yy++)for(int xx=opt.x;xx<opt.x+opt.w;xx++){
      static const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
      for(int d=0;d<4;d++){
        int nx=xx+dx[d],ny=yy+dy[d];
        if(nx>=opt.x&&nx<opt.x+opt.w&&ny>=opt.y&&ny<opt.y+opt.h)continue;
        uint32_t other=fi(m,nx,ny);uint8_t t=m->cells[other];
        if(t==NACRE_DUNGEON_ROOM||t==NACRE_DUNGEON_CORRIDOR)
          choices[k++]=(SecretContact){fi(m,xx,yy),other,t==NACRE_DUNGEON_CORRIDOR};
      }
    }
    for(uint32_t i=k;i>1;i--){uint32_t j=(uint32_t)fri(r,0,(int)i-1);
      SecretContact t=choices[j];choices[j]=choices[i-1];choices[i-1]=t;}
    /* Stable preference for corridor-facing openings after shuffle. */
    SecretContact entrance=choices[0];
    for(uint32_t i=0;i<k;i++)if(choices[i].hall){entrance=choices[i];break;}
    free(choices);
    NacreDungeonSecretRoom *secret=&m->secret_rooms[m->secret_room_count];
    secret->id=m->room_count+m->secret_room_count;
    secret->x=opt.x;secret->y=opt.y;secret->width=opt.w;secret->height=opt.h;
    secret->entrance_inside=entrance.inside;secret->entrance_outside=entrance.outside;
    secret->cell_count=(uint32_t)(opt.w*opt.h);
    secret->cells=malloc((size_t)secret->cell_count*sizeof(*secret->cells));
    if(!secret->cells)return false;
    uint32_t p=0;
    for(int yy=opt.y;yy<opt.y+opt.h;yy++)for(int xx=opt.x;xx<opt.x+opt.w;xx++){
      uint32_t t=fi(m,xx,yy);
      m->cells[t]=NACRE_DUNGEON_SECRET;m->room_ids[t]=(int16_t)secret->id;
      secret->cells[p++]=t;
    }
    m->secret_room_count++;
    if(!edge_push(m,entrance.inside,entrance.outside,NACRE_DUNGEON_EDGE_SECRET))return false;
  }
  return true;
}

typedef struct CavePortal {uint32_t inside,outside;} CavePortal;
typedef struct CavePortals {CavePortal *items;uint32_t count,capacity;} CavePortals;
typedef struct CaveCandidate {
  uint32_t a,b;CavePortal u,v;int distance;
  double tie;bool cross_branch;
} CaveCandidate;
static int cave_cmp(const void *a,const void *b){
  const CaveCandidate *x=a,*y=b;
  if(x->distance!=y->distance)return x->distance-y->distance;
  return x->tie<y->tie?-1:x->tie>y->tie?1:0;
}
static bool add_portal(CavePortals *p,CavePortal portal){
  if(p->count==p->capacity){
    uint32_t next=p->capacity?p->capacity*2:32;
    void *v=realloc(p->items,(size_t)next*sizeof(*p->items));
    if(!v)return false;
    p->items=v;
    p->capacity=next;
  }
  p->items[p->count++]=portal;return true;
}
static bool connected_rooms(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  for(uint32_t i=0;i<m->link_count;i++){
    const NacreDungeonLink *l=&m->links[i];
    if((l->room_a==a&&l->room_b==b)||(l->room_a==b&&l->room_b==a))return true;
  }
  return false;
}
/* Same A* cost and local rectangle restrictions as the v21 cave router.
 * The heap is deliberately bounded by the rectangle's cell count. */
typedef struct CaveNode {uint32_t tile;double g,f;} CaveNode;
/* Exact binary-heap tie behavior of the JavaScript v21 Heap. Equal f values
 * preserve the parent's slot on insertion and choose the left child on pop. */
static void cave_heap_push(CaveNode *items,uint32_t *length,CaveNode value){
  uint32_t i=(*length)++;
  while(i){
    uint32_t parent=(i-1u)>>1;
    if(items[parent].f<=value.f)break;
    items[i]=items[parent];i=parent;
  }
  items[i]=value;
}
static CaveNode cave_heap_pop(CaveNode *items,uint32_t *length){
  CaveNode result=items[0],last=items[--*length];
  if(*length){
    uint32_t i=0;
    while(true){
      uint32_t left=i*2u+1u;
      if(left>=*length)break;
      uint32_t right=left+1u;
      uint32_t child=(right<*length&&items[right].f<items[left].f)?right:left;
      if(items[child].f>=last.f)break;
      items[i]=items[child];i=child;
    }
    items[i]=last;
  }
  return result;
}
static bool route_cave(const NacreDungeonMap *m,uint32_t from,uint32_t to,
                       uint32_t seed,uint32_t **out_path,uint32_t *out_len){
  if(m->cells[from]||m->cells[to]||from==to)return false;
  int ax=fx(m,from),ay=fy(m,from),bx=fx(m,to),by=fy(m,to);
  int direct=abs_i(ax-bx)+abs_i(ay-by);
  if(direct<2||direct>12)return false;
  int max_path=min_i(16,(int)ceil(direct*1.4)+3);
  int minx=max_i(1,min_i(ax,bx)-2),maxx=min_i((int)m->width-2,max_i(ax,bx)+2);
  int miny=max_i(1,min_i(ay,by)-2),maxy=min_i((int)m->height-2,max_i(ay,by)+2);
  uint32_t n=m->width*m->height;
  double *dist=malloc((size_t)n*sizeof(*dist));
  int32_t *prev=malloc((size_t)n*sizeof(*prev));
  int16_t *steps=calloc(n,sizeof(*steps));
  CaveNode *heap=malloc((size_t)(maxx-minx+1)*(maxy-miny+1)*8*sizeof(*heap));
  if(!dist||!prev||!steps||!heap){free(dist);free(prev);free(steps);free(heap);return false;}
  for(uint32_t i=0;i<n;i++){dist[i]=INFINITY;prev[i]=-1;}
  uint32_t head=0,capacity=(uint32_t)((maxx-minx+1)*(maxy-miny+1)*8);
  dist[from]=0;steps[from]=1;
  cave_heap_push(heap,&head,(CaveNode){from,0,(double)direct});
  bool found=false;
  while(head){
    CaveNode cur=cave_heap_pop(heap,&head);
    if(cur.g>dist[cur.tile]+1.e-8)continue;
    if(cur.tile==to){found=true;break;}
    uint32_t ns[4],count=neighbors(m,cur.tile,ns);
    for(uint32_t i=0;i<count;i++){
      uint32_t next=ns[i];int x=fx(m,next),y=fy(m,next);
      if(x<minx||x>maxx||y<miny||y>maxy||m->cells[next]||steps[cur.tile]+1>max_path)continue;
      uint32_t noise=(((uint32_t)(x+17)*UINT32_C(73856093))^
                      ((uint32_t)(y+29)*UINT32_C(19349663))^seed)%13u;
      double g=cur.g+1+(double)noise/1000.0;
      if(g<dist[next]-1.e-8){
        dist[next]=g;prev[next]=(int32_t)cur.tile;steps[next]=steps[cur.tile]+1;
        if(head<capacity)cave_heap_push(heap,&head,
            (CaveNode){next,g,g+abs_i(x-bx)+abs_i(y-by)});
      }
    }
  }
  if(found){
    uint32_t length=0;
    for(int32_t t=(int32_t)to;t>=0;t=prev[t])length++;
    if(length<4||length>(uint32_t)max_path)found=false;
    else {
      uint32_t *path=malloc((size_t)length*sizeof(*path));
      if(!path)found=false;
      else {
        int32_t t=(int32_t)to;
        for(uint32_t k=length;k>0;k--){path[k-1]=(uint32_t)t;t=prev[t];}
        *out_path=path;*out_len=length;
      }
    }
  }
  free(dist);free(prev);free(steps);free(heap);
  return found;
}
/* Primary tree analysis for the v21 cross-branch annotation. */
static bool cross_branch(const NacreDungeonMap *m,uint32_t a,uint32_t b){
  uint32_t n=m->room_count;
  int16_t parent[NACRE_DUNGEON_MAX_ROOMS],depth[NACRE_DUNGEON_MAX_ROOMS];
  for(uint32_t i=0;i<n;i++){parent[i]=-1;depth[i]=0;}
  int root=m->room_ids[m->entry];if(root<0||root>=(int)n)return false;
  uint32_t q[NACRE_DUNGEON_MAX_ROOMS],head=0,tail=0;
  q[tail++]=(uint32_t)root;parent[root]=(int16_t)root;
  while(head<tail){uint32_t at=q[head++];
    for(uint32_t j=0;j<m->link_count;j++){
      const NacreDungeonLink *l=&m->links[j];if(l->extra)continue;
      int next=-1;
      if(l->room_a==at)next=(int)l->room_b;
      if(l->room_b==at)next=(int)l->room_a;
      if(next<0||parent[next]!=-1)continue;
      parent[next]=(int16_t)at;depth[next]=depth[at]+1;q[tail++]=(uint32_t)next;
    }
  }
  if(tail!=n)return false;
  int x=(int)a,y=(int)b;
  while(depth[x]>depth[y])x=parent[x];
  while(depth[y]>depth[x])y=parent[y];
  while(x!=y){x=parent[x];y=parent[y];}
  return x!=(int)a&&x!=(int)b;
}
static bool place_caves(NacreDungeonMap *m,uint32_t desired){
  if(!desired)return true;
  NacreFeatRng rng={suffix_hash(m->seed_hash,"|optional-cave-tunnels")};
  CavePortals *portals=calloc(m->room_count,sizeof(*portals));
  CaveCandidate *candidates=NULL;uint32_t count=0,cap=0;
  if(!portals)return false;
  bool okay=true;
  /* Portal proximity to existing openings (not merely nearby walls). */
  for(uint32_t r=0;r<m->room_count&&okay;r++){
    const NacreDungeonRoom *room=&m->rooms[r];
    for(uint32_t k=0;k<room->cell_count&&okay;k++){
      uint32_t tile=room->cells[k],ns[4],nc=neighbors(m,tile,ns);
      for(uint32_t j=0;j<nc;j++){
        uint32_t other=ns[j];
        if(m->cells[other])continue;
        int mx2=fx(m,tile)+fx(m,other),my2=fy(m,tile)+fy(m,other);
        bool near=false;
        for(uint32_t e=0;e<m->edge_count;e++){
          if(m->edges[e].type!=NACRE_DUNGEON_EDGE_OPEN&&
             m->edges[e].type!=NACRE_DUNGEON_EDGE_HOLE)continue;
          const NacreDungeonEdge *edge=&m->edges[e];
          int ox2=fx(m,edge->a)+fx(m,edge->b),oy2=fy(m,edge->a)+fy(m,edge->b);
          if((abs_i(mx2-ox2)+abs_i(my2-oy2))<5){near=true;break;}
        }
        if(!near&&!add_portal(&portals[r],(CavePortal){tile,other})){okay=false;break;}
      }
    }
  }
  /* Iterate room pairs and cap each pair's options at its 12 nearest. */
  for(uint32_t a=0;a<m->room_count&&okay;a++)for(uint32_t b=a+1;b<m->room_count&&okay;b++){
    if(!portals[a].count||!portals[b].count||connected_rooms(m,a,b))continue;
    CaveCandidate *local=NULL;uint32_t lc=0,lcap=0;
    bool cross=cross_branch(m,a,b);
    for(uint32_t u=0;u<portals[a].count&&okay;u++)for(uint32_t v=0;v<portals[b].count;v++){
      CavePortal pa=portals[a].items[u],pb=portals[b].items[v];
      int d=abs_i(fx(m,pa.outside)-fx(m,pb.outside))+
            abs_i(fy(m,pa.outside)-fy(m,pb.outside));
      if(d<2||d>12)continue;
      if(lc==lcap){uint32_t next=lcap?lcap*2:64;
        void *mem=realloc(local,(size_t)next*sizeof(*local));
        if(!mem){okay=false;break;}local=mem;lcap=next;}
      local[lc++]=(CaveCandidate){a,b,pa,pb,d,frand01(&rng),cross};
    }
    if(okay&&lc){
      qsort(local,lc,sizeof(*local),cave_cmp);
      uint32_t take=lc<12?lc:12;
      if(count+take>cap){uint32_t next=cap?cap*2:128;
        while(next<count+take)next*=2;
        void *mem=realloc(candidates,(size_t)next*sizeof(*candidates));
        if(!mem)okay=false;else {candidates=mem;cap=next;}}
      if(okay){memcpy(candidates+count,local,(size_t)take*sizeof(*local));count+=take;}
    }
    free(local);
  }
  if(okay)qsort(candidates,count,sizeof(*candidates),cave_cmp);
  m->caves=calloc(desired,sizeof(*m->caves));
  if(!m->caves)okay=false;
  uint8_t used_rooms[NACRE_DUNGEON_MAX_ROOMS]={0};
  while(okay&&m->cave_count<desired){
    int selected=-1;double best_score=INFINITY;
    uint32_t *best_path=NULL,best_length=0;
    for(uint32_t i=0;i<count;i++){
      CaveCandidate *c=&candidates[i];
      if(used_rooms[c->a]||used_rooms[c->b]||
         m->cells[c->u.outside]||m->cells[c->v.outside]||
         edge_find(m,c->u.inside,c->u.outside)||edge_find(m,c->v.inside,c->v.outside))continue;
      uint32_t *path=NULL,length=0;
      if(!route_cave(m,c->u.outside,c->v.outside,m->seed_hash,&path,&length))continue;
      bool touches=false;
      for(uint32_t k=0;k<length&&!touches;k++){
        uint32_t ns[4],nn=neighbors(m,path[k],ns);
        for(uint32_t j=0;j<nn;j++)if(m->cells[ns[j]]==NACRE_DUNGEON_CAVE){touches=true;break;}
      }
      if(touches){free(path);continue;}
      int detour=bfs(m,c->u.inside,c->v.inside,NULL);
      if(detour<0||detour-(int)(length+1)<5){free(path);continue;}
      int benefit=min_i(24,detour-(int)length);
      double score=fabs((double)length-7)*1.7+length*.22-benefit*.22+c->tie*.9;
      if(score<best_score){free(best_path);best_path=path;best_length=length;
        best_score=score;selected=(int)i;
      }else free(path);
    }
    if(selected<0){free(best_path);break;}
    CaveCandidate *c=&candidates[selected];
    NacreDungeonCave *cave=&m->caves[m->cave_count];
    cave->room_a=c->a;cave->room_b=c->b;cave->path=best_path;
    cave->path_count=best_length;cave->cross_branch=c->cross_branch;
    cave->entrances[0]=(NacreDungeonEdge){c->u.inside,c->u.outside,NACRE_DUNGEON_EDGE_CRACKED};
    cave->entrances[1]=(NacreDungeonEdge){c->v.inside,c->v.outside,NACRE_DUNGEON_EDGE_CRACKED};
    for(uint32_t j=0;j<best_length;j++)m->cells[best_path[j]]=NACRE_DUNGEON_CAVE;
    m->cave_count++;
    if(!edge_push(m,c->u.inside,c->u.outside,NACRE_DUNGEON_EDGE_CRACKED)||
       !edge_push(m,c->v.inside,c->v.outside,NACRE_DUNGEON_EDGE_CRACKED)){
      okay=false;break;
    }
    used_rooms[c->a]=used_rooms[c->b]=1;
  }
  for(uint32_t i=0;i<m->room_count;i++)free(portals[i].items);
  free(portals);free(candidates);
  return okay;
}

typedef struct PillarSlot {
  uint32_t room;uint32_t index;uint32_t count;double score;
  double anchor_x,anchor_y;
} PillarSlot;
static int pillar_cmp(const void *a,const void *b){
  const PillarSlot *x=a,*y=b;
  if(x->score!=y->score)return x->score>y->score?-1:1;
  if(x->room!=y->room)return x->room<y->room?-1:1;
  return x->index<y->index?-1:x->index>y->index?1:0;
}
static bool place_pillars(const NacreDungeonConfig *cfg,NacreDungeonMap *m){
  if(!cfg->pillar_count)return true;
  uint32_t n=m->width*m->height;
  NacreFeatRng rng={suffix_hash(m->seed_hash,"|pillar-layout|0")};
  m->pillar_mask=calloc(n,1);
  m->pillars=calloc(cfg->pillar_count,sizeof(*m->pillars));
  uint8_t *forbidden=calloc(n,1),*baseline=malloc(n),*now=malloc(n);
  if(!m->pillar_mask||!m->pillars||!forbidden||!baseline||!now){
    free(forbidden);free(baseline);free(now);return false;
  }
  /* Every opening and its immediate neighborhood is reserved. */
  for(uint32_t e=0;e<m->edge_count;e++){
    uint32_t ends[2]={m->edges[e].a,m->edges[e].b};
    for(uint32_t k=0;k<2;k++){
      uint32_t tile=ends[k],ns[4],nc=neighbors(m,tile,ns);
      forbidden[tile]=1;
      for(uint32_t j=0;j<nc;j++)forbidden[ns[j]]=1;
    }
  }
  uint32_t ns[4],nc=neighbors(m,m->entry,ns);
  forbidden[m->entry]=forbidden[m->exit]=1;
  for(uint32_t j=0;j<nc;j++)forbidden[ns[j]]=1;
  nc=neighbors(m,m->exit,ns);
  for(uint32_t j=0;j<nc;j++)forbidden[ns[j]]=1;
  for(uint32_t i=0;i<m->shortest_route_count;i++)forbidden[m->shortest_route[i]]=1;
  /* Progression markers must remain reachable, never become a blocking
   * pillar. The HTML reserves the cells and their immediate neighborhood. */
  for(uint32_t t=0;t<m->totem_count;t++){
    uint32_t tile=m->totems[t].tile,ns[4],nc=neighbors(m,tile,ns);
    forbidden[tile]=1;
    for(uint32_t j=0;j<nc;j++)forbidden[ns[j]]=1;
  }
  for(uint32_t o=0;o<m->objective_count;o++){
    uint32_t tile=m->objectives[o].tile,ns[4],nc=neighbors(m,tile,ns);
    forbidden[tile]=1;
    for(uint32_t j=0;j<nc;j++)forbidden[ns[j]]=1;
  }

  PillarSlot slots[4*NACRE_DUNGEON_MAX_ROOMS];uint32_t slot_count=0;
  for(uint32_t rid=0;rid<m->room_count;rid++){
    NacreDungeonRoom *room=&m->rooms[rid];
    int area=(int)room->cell_count,short_side=min_i(room->width,room->height);
    if(room->peripheral_treasure||rid==(uint32_t)m->room_ids[m->entry]||
       rid==(uint32_t)m->room_ids[m->exit]||short_side<6)continue;
    double chance=0,extras[3]={0};int max=0;
    if(area<24){chance=.22;max=1;}
    else if(area<32){chance=.58;max=2;extras[0]=.30;}
    else if(area<42){chance=.82;max=3;extras[0]=.68;extras[1]=.28;}
    else {chance=.94;max=4;extras[0]=.82;extras[1]=.56;extras[2]=.24;}
    uint32_t available=0;
    for(uint32_t k=0;k<room->cell_count;k++){
      uint32_t t=room->cells[k];int x=fx(m,t),y=fy(m,t);
      int border=min_i(min_i(x-room->x,room->x+room->width-1-x),
                       min_i(y-room->y,room->y+room->height-1-y));
      if(!forbidden[t]&&border>=1)available++;
    }
    if(!available||frand01(&rng)>=chance)continue;
    int count=1;
    for(int j=0;j<max-1;j++){if(frand01(&rng)<extras[j])count++;else break;}
    count=min_i(count,min_i(max,(int)available));
    double ax[4],ay[4];
    if(count==1){ax[0]=.5;ay[0]=.5;}
    else if(count==2){
      if(room->width>=room->height){ax[0]=.3;ay[0]=.5;ax[1]=.7;ay[1]=.5;}
      else {ax[0]=.5;ay[0]=.3;ax[1]=.5;ay[1]=.7;}
    }else if(count==3){
      if(room->width>=room->height){ax[0]=.26;ay[0]=.32;ax[1]=.74;ay[1]=.32;ax[2]=.5;ay[2]=.68;}
      else {ax[0]=.32;ay[0]=.26;ax[1]=.68;ay[1]=.5;ax[2]=.32;ay[2]=.74;}
    }else {ax[0]=.30;ay[0]=.30;ax[1]=.70;ay[1]=.30;ax[2]=.30;ay[2]=.70;ax[3]=.70;ay[3]=.70;}
    bool flipx=frand01(&rng)<.5,flipy=frand01(&rng)<.5;
    for(int k=0;k<count;k++){
      double u=flipx?1-ax[k]:ax[k],v=flipy?1-ay[k]:ay[k];
      slots[slot_count++]=(PillarSlot){rid,(uint32_t)k,(uint32_t)count,
        sqrt((double)area)-k*.65+frand01(&rng)*.25,
        room->x+u*(room->width-1),room->y+v*(room->height-1)};
    }
  }
  qsort(slots,slot_count,sizeof(*slots),pillar_cmp);
  if(bfs(m,m->entry,UINT32_MAX,baseline)==-2){free(forbidden);free(baseline);free(now);return false;}
  for(uint32_t job=0;job<slot_count&&m->pillar_count<cfg->pillar_count;job++){
    const PillarSlot *slot=&slots[job];NacreDungeonRoom *room=&m->rooms[slot->room];
    uint32_t chosen=UINT32_MAX;double best=INFINITY;
    for(uint32_t k=0;k<room->cell_count;k++){
      uint32_t t=room->cells[k];int x=fx(m,t),y=fy(m,t);
      if(forbidden[t]||m->pillar_mask[t])continue;
      int border=min_i(min_i(x-room->x,room->x+room->width-1-x),
                       min_i(y-room->y,room->y+room->height-1-y));
      if(border<1)continue;
      bool crowded=false;
      for(uint32_t j=0;j<m->pillar_count;j++){
        uint32_t other=m->pillars[j].tile;
        if(abs_i(fx(m,other)-x)<3&&abs_i(fy(m,other)-y)<3){crowded=true;break;}
      }
      if(crowded)continue;
      double cx=room->x+(room->width-1)*.5,cy=room->y+(room->height-1)*.5;
      double score=(x-slot->anchor_x)*(x-slot->anchor_x)+
        (y-slot->anchor_y)*(y-slot->anchor_y)+
        ((x-cx)*(x-cx)+(y-cy)*(y-cy))*.12-border*.05;
      if(score>=best)continue;
      m->pillar_mask[t]=1;
      int rc=bfs(m,m->entry,UINT32_MAX,now);
      bool safe=rc!=-2;
      for(uint32_t i=0;i<n&&safe;i++)if(baseline[i]&&!m->pillar_mask[i]&&!now[i])safe=false;
      m->pillar_mask[t]=0;
      if(safe){best=score;chosen=t;}
    }
    if(chosen!=UINT32_MAX){
      m->pillar_mask[chosen]=1;
      m->pillars[m->pillar_count++]=(NacreDungeonPillar){chosen,slot->room,
        frand01(&rng)<cfg->pillar_broken_fraction};
    }
  }
  free(forbidden);free(baseline);free(now);return true;
}

bool nacre_dungeon_features_generate(const NacreDungeonConfig *cfg,
                                    NacreDungeonMap *m,uint32_t *rng_state){
  if(!cfg||!m||!rng_state)return false;
  NacreFeatRng rng={*rng_state};
  static const int secrets[4]={40,30,20,10},caves[4]={10,40,30,20};
  NacreFeatRng counts={suffix_hash(m->seed_hash,"|secret-room-count")};
  m->requested_secret_count=weighted_count(&counts,secrets,(int)cfg->secret_rooms_max);
  if(!place_secrets(m,&rng,m->requested_secret_count))return false;
  *rng_state=rng.state;
  counts.state=suffix_hash(m->seed_hash,"|cave-tunnel-count");
  m->requested_cave_count=weighted_count(&counts,caves,(int)cfg->cave_tunnels_max);
  if(!place_caves(m,m->requested_cave_count))return false;
  return true;
}
bool nacre_dungeon_features_place_pillars(const NacreDungeonConfig *cfg,
                                          NacreDungeonMap *m){
  return place_pillars(cfg,m);
}
void nacre_dungeon_features_release(NacreDungeonMap *m){
  if(!m)return;
  for(uint32_t i=0;i<m->secret_room_count;i++)free(m->secret_rooms[i].cells);
  for(uint32_t i=0;i<m->cave_count;i++)free(m->caves[i].path);
  free(m->secret_rooms);free(m->caves);free(m->pillars);free(m->pillar_mask);
  m->secret_rooms=NULL;m->secret_room_count=0;
  m->caves=NULL;m->cave_count=0;
  m->pillars=NULL;m->pillar_count=0;m->pillar_mask=NULL;
}
