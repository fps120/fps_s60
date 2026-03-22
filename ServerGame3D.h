#pragma once
#include "GameTypes.h"
#include <unordered_map>
#include <functional>
#include <random>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace CS {

struct SrvPlayer {
    PlayerState st;
    float vx=0,vy=0,vz=0;
    bool  grounded=false;
    float fireTimer=0, reloadTimer=0, spread=0;
    InputState input;
    bool  active=false;
};

class ServerGame3D {
public:
    using BulletCB = std::function<void(const BulletEvent&)>;
    using HitCB    = std::function<void(uint8_t,uint8_t,int,bool,bool)>;
    using RoundCB  = std::function<void(bool)>;

    ServerGame3D();
    bool   loadMap(const std::string& path);
    uint8_t addPlayer(const std::string& name);
    void   removePlayer(uint8_t id);
    void   setTeam(uint8_t id, Team t);
    void   applyInput(uint8_t id, const InputState& in);
    void   tick(float dt);

    const std::unordered_map<uint8_t,SrvPlayer>& players() const { return m_players; }
    const GameInfo& info() const { return m_info; }

    BulletCB onBullet;
    HitCB    onHit;
    RoundCB  onRound;

private:
    struct MapData {
        std::vector<AABB> solids;
        std::vector<SpawnPoint> spawns;
    } m_map;

    std::unordered_map<uint8_t,SrvPlayer> m_players;
    GameInfo  m_info;
    uint8_t   m_nextId=1;
    int       m_spawnT=0, m_spawnCT=0;
    std::mt19937 m_rng{42};

    void stepPlayer(uint8_t id, SrvPlayer& sp, float dt);
    void stepPhysicsY(SrvPlayer& sp, float dt);
    void moveCollide(SrvPlayer& sp, float dx, float dy, float dz);
    void tryShoot(uint8_t id, SrvPlayer& sp);
    void startRound();
    void endRound(Team winner);
    void spawnAll();
    void spawnPlayer(SrvPlayer& sp);
    void updateTimer(float dt);
    float rayVsAABB(glm::vec3 o, glm::vec3 d, const AABB& b, float maxD);
};

// ── Implementation ────────────────────────────────────────────────────────────
inline ServerGame3D::ServerGame3D(){
    m_info.state=RoundState::WARMUP; m_info.timeLeft=20.f;
}

inline bool ServerGame3D::loadMap(const std::string& path){
    std::ifstream f(path); if(!f) return false;
    std::string line;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#') continue;
        std::istringstream ss(line); std::string cmd; ss>>cmd;
        if(cmd=="SOLIDBOX"||cmd=="WALL"){
            float cx,cy,cz,sx,sy,sz,r,g,b;
            if(cmd=="SOLIDBOX"){ ss>>cx>>cy>>cz>>sx>>sy>>sz>>r>>g>>b;
                m_map.solids.push_back({{cx-sx/2,cy,cz-sz/2},{cx+sx/2,cy+sy,cz+sz/2}});
            }
        } else if(cmd=="PROP"){
            std::string t; float px,py,pz,yw=0; ss>>t>>px>>py>>pz>>yw;
            glm::vec3 hs;
            if(t=="CRATE_S") hs={0.25f,0.5f,0.25f};
            else if(t=="CRATE_L") hs={0.35f,1.2f,0.35f};
            else if(t=="BARREL") hs={0.2f,0.9f,0.2f};
            else hs={0.05f,1.6f,0.5f};
            m_map.solids.push_back({{px-hs.x,py,pz-hs.z},{px+hs.x,py+hs.y*2,pz+hs.z}});
        } else if(cmd=="SPAWN"){
            std::string ts; float sx,sy,sz,yw=0; ss>>ts>>sx>>sy>>sz>>yw;
            Team t=(ts=="T")?Team::TERRORIST:Team::CT;
            m_map.spawns.push_back({sx,sy,sz,t,glm::radians(yw)});
        }
    }
    std::cout<<"[ServerGame3D] Map loaded: "<<path
             <<"  solids="<<m_map.solids.size()
             <<"  spawns="<<m_map.spawns.size()<<"\n";
    return true;
}

inline uint8_t ServerGame3D::addPlayer(const std::string& name){
    uint8_t id=m_nextId++;
    SrvPlayer sp; sp.active=true; sp.st.id=id;
    std::strncpy(sp.st.name,name.c_str(),23);
    sp.st.money=800; sp.st.alive=false;
    m_players[id]=sp;
    return id;
}

inline void ServerGame3D::removePlayer(uint8_t id){ m_players.erase(id); }

inline void ServerGame3D::setTeam(uint8_t id,Team t){
    auto it=m_players.find(id); if(it==m_players.end()) return;
    it->second.st.team=t;
    spawnPlayer(it->second);
}

inline void ServerGame3D::applyInput(uint8_t id,const InputState& in){
    auto it=m_players.find(id); if(it!=m_players.end()) it->second.input=in;
}

inline void ServerGame3D::tick(float dt){
    updateTimer(dt);
    for(auto& [id,sp]:m_players)
        if(sp.active&&sp.st.alive) stepPlayer(id,sp,dt);
}

inline void ServerGame3D::stepPlayer(uint8_t id,SrvPlayer& sp,float dt){
    auto& in=sp.input;
    sp.st.yaw=in.yaw; sp.st.pitch=in.pitch; sp.st.crouching=in.crouch;

    float yaw=sp.st.yaw;
    glm::vec3 fwd{sinf(yaw),0,cosf(yaw)};
    glm::vec3 rgt{cosf(yaw),0,-sinf(yaw)};
    glm::vec3 wish{0,0,0};
    if(in.fwd)  wish+=fwd; if(in.back)  wish-=fwd;
    if(in.right)wish+=rgt; if(in.left)  wish-=rgt;
    float wl=glm::length(wish); if(wl>0.001f) wish/=wl;

    float spd=sp.st.crouching?CROUCH_SPEED:RUN_SPEED;
    spd*=WEAPONS[(int)sp.st.weapon].speedMul;

    const float ACC=30.f,FRIC=18.f;
    sp.vx+=wish.x*ACC*dt; sp.vz+=wish.z*ACC*dt;
    float hs=std::sqrtf(sp.vx*sp.vx+sp.vz*sp.vz);
    if(hs>0.001f){float ns=std::max(0.f,hs-FRIC*dt); float c=std::min(ns,spd); sp.vx=sp.vx/hs*c; sp.vz=sp.vz/hs*c;}

    if(in.jump&&sp.grounded){sp.vy=JUMP_FORCE;sp.grounded=false;}
    if(!sp.grounded)sp.vy-=GRAVITY*dt;

    moveCollide(sp,sp.vx*dt,sp.vy*dt,sp.vz*dt);

    // Weapon
    if(sp.fireTimer>0)sp.fireTimer-=dt;
    if(sp.st.reloading){sp.reloadTimer-=dt;
        if(sp.reloadTimer<=0){sp.st.reloading=false;
            const auto& wd=WEAPONS[(int)sp.st.weapon];
            int need=wd.magSize-sp.st.ammo,given=std::min((int)sp.st.ammoRes,need);
            sp.st.ammo+=given;sp.st.ammoRes-=given;}}
    sp.spread=std::max(0.f,sp.spread-dt*10.f);
    if(in.reload&&!sp.st.reloading&&sp.st.ammoRes>0)
        {sp.st.reloading=true;sp.reloadTimer=WEAPONS[(int)sp.st.weapon].reloadTime;}
    if(in.shoot)tryShoot(id,sp);

    // Weapon slot
    static const WeaponID slots[]={WeaponID::KNIFE,WeaponID::GLOCK,WeaponID::MP5,WeaponID::AK47,WeaponID::AWP,WeaponID::M3};
    if(in.slot<6)sp.st.weapon=slots[in.slot];
}

inline void ServerGame3D::moveCollide(SrvPlayer& sp,float dx,float dy,float dz){
    float H=sp.st.crouching?PLAYER_CROUCH_H:PLAYER_HEIGHT;
    auto aabb=[&]()->AABB{
        return {{sp.st.x-PLAYER_RADIUS,sp.st.y,sp.st.z-PLAYER_RADIUS},
                {sp.st.x+PLAYER_RADIUS,sp.st.y+H,sp.st.z+PLAYER_RADIUS}};
    };
    sp.st.x+=dx;
    auto a=aabb();
    for(auto& s:m_map.solids){if(!a.overlaps(s))continue;
        float pr=s.max.x-a.min.x,pl=a.max.x-s.min.x;
        if(pr<pl)sp.st.x+=pr;else sp.st.x-=pl;sp.vx=0;a=aabb();}
    sp.st.z+=dz;
    a=aabb();
    for(auto& s:m_map.solids){if(!a.overlaps(s))continue;
        float pf=s.max.z-a.min.z,pb=a.max.z-s.min.z;
        if(pf<pb)sp.st.z+=pf;else sp.st.z-=pb;sp.vz=0;a=aabb();}
    sp.st.y+=dy; a=aabb(); sp.grounded=false;
    for(auto& s:m_map.solids){if(!a.overlaps(s))continue;
        float pu=s.max.y-a.min.y,pd=a.max.y-s.min.y;
        if(pu<pd){sp.st.y+=pu;sp.vy=0;sp.grounded=true;}
        else{sp.st.y-=pd;sp.vy=0;} a=aabb();}
    if(sp.st.y<0){sp.st.y=0;sp.vy=0;sp.grounded=true;}
}

inline void ServerGame3D::tryShoot(uint8_t id,SrvPlayer& sp){
    if(sp.st.weapon==WeaponID::KNIFE||sp.fireTimer>0||sp.st.ammo<=0||sp.st.reloading)return;
    const auto& wd=WEAPONS[(int)sp.st.weapon];
    glm::vec3 orig{sp.st.x,sp.st.y+PLAYER_EYE_H,sp.st.z};
    glm::vec3 dir{cosf(sp.st.pitch)*sinf(sp.st.yaw),sinf(sp.st.pitch),cosf(sp.st.pitch)*cosf(sp.st.yaw)};
    // Spread
    std::uniform_real_distribution<float> d(-1.f,1.f);
    float spr=glm::radians(wd.spread+sp.spread);
    glm::vec3 r{cosf(sp.st.yaw),0,-sinf(sp.st.yaw)};
    glm::vec3 u{0,1,0};
    dir+=r*d(m_rng)*spr+u*d(m_rng)*spr;
    dir=glm::normalize(dir);

    float wallD=wd.range;
    for(auto& s:m_map.solids){float t=rayVsAABB(orig,dir,s,wallD);if(t>0&&t<wallD)wallD=t;}

    float playerD=wallD; uint8_t hitId=255; bool hs=false;
    for(auto& [pid,other]:m_players){
        if(pid==id||!other.st.alive)continue;
        if(other.st.team==sp.st.team)continue;
        float H=other.st.crouching?PLAYER_CROUCH_H:PLAYER_HEIGHT;
        AABB pb{{other.st.x-PLAYER_RADIUS,other.st.y,other.st.z-PLAYER_RADIUS},
                {other.st.x+PLAYER_RADIUS,other.st.y+H,other.st.z+PLAYER_RADIUS}};
        float t=rayVsAABB(orig,dir,pb,wallD);
        if(t>0&&t<playerD){playerD=t;hitId=pid;
            glm::vec3 hp=orig+dir*t;
            hs=(hp.y>other.st.y+H*0.8f);}
    }

    BulletEvent evt;
    evt.shooterId=id;evt.ox=orig.x;evt.oy=orig.y;evt.oz=orig.z;
    evt.dx=dir.x;evt.dy=dir.y;evt.dz=dir.z;
    evt.dist=playerD;evt.weapon=sp.st.weapon;evt.hit=(hitId!=255);
    if(onBullet)onBullet(evt);

    if(hitId!=255){
        auto& target=m_players[hitId];
        float dmg=wd.damage*(hs?wd.headshotMul:1.f);
        int idmg=(int)dmg;
        if(target.st.armor>0){int a=std::min((int)target.st.armor,idmg/2);target.st.armor-=a;idmg-=a;}
        target.st.health-=idmg;
        bool killed=target.st.health<=0;
        if(killed){target.st.health=0;target.st.alive=false;sp.st.kills++;target.st.deaths++;
            sp.st.money=(uint16_t)std::min(16000,(int)sp.st.money+300);}
        if(onHit)onHit(id,hitId,idmg,hs,killed);
        if(killed){
            int ta=0,ca=0;
            for(auto& [pid,p]:m_players){if(!p.st.alive)continue;if(p.st.team==Team::TERRORIST)ta++;else ca++;}
            if(ta==0&&ca>0)endRound(Team::CT);
            else if(ca==0&&ta>0)endRound(Team::TERRORIST);
        }
    }
    sp.st.ammo--;sp.fireTimer=1.f/wd.fireRate;
    sp.spread=std::min(sp.spread+wd.spread*0.5f,wd.spread*4.f);
    if(sp.st.ammo==0&&sp.st.ammoRes>0){sp.st.reloading=true;sp.reloadTimer=wd.reloadTime;}
}

inline void ServerGame3D::startRound(){
    m_info.state=RoundState::BUY;m_info.timeLeft=BUY_PHASE_TIME;m_info.round++;
    m_spawnT=m_spawnCT=0;spawnAll();if(onRound)onRound(true);
}

inline void ServerGame3D::endRound(Team w){
    if(m_info.state==RoundState::END)return;
    m_info.state=RoundState::END;m_info.timeLeft=5.f;
    if(w==Team::TERRORIST)m_info.tScore++;else m_info.ctScore++;
    for(auto& [id,sp]:m_players){
        if(sp.st.team==w&&sp.st.alive)sp.st.money=(uint16_t)std::min(16000,(int)sp.st.money+3250);
        else sp.st.money=(uint16_t)std::min(16000,(int)sp.st.money+1400);}
    if(onRound)onRound(false);
}

inline void ServerGame3D::spawnAll(){
    m_spawnT=m_spawnCT=0;
    for(auto& [id,sp]:m_players)if(sp.active)spawnPlayer(sp);
}

inline void ServerGame3D::spawnPlayer(SrvPlayer& sp){
    int& idx=(sp.st.team==Team::TERRORIST)?m_spawnT:m_spawnCT;
    std::vector<SpawnPoint> pts;
    for(auto& s:m_map.spawns)if(s.team==sp.st.team)pts.push_back(s);
    SpawnPoint pt={2.f,0.f,2.f,sp.st.team,0.f};
    if(!pts.empty())pt=pts[idx%(int)pts.size()]; idx++;
    sp.st.x=pt.x;sp.st.y=pt.y;sp.st.z=pt.z;
    sp.st.yaw=pt.yaw;sp.st.pitch=0;
    sp.vx=sp.vy=sp.vz=0;sp.grounded=false;
    sp.st.health=100;sp.st.armor=0;sp.st.alive=true;sp.st.reloading=false;
    sp.st.weapon=WeaponID::GLOCK;
    sp.st.ammo=WEAPONS[(int)WeaponID::GLOCK].magSize;sp.st.ammoRes=60;
    sp.fireTimer=sp.reloadTimer=sp.spread=0;
}

inline void ServerGame3D::updateTimer(float dt){
    m_info.timeLeft-=dt;
    switch(m_info.state){
    case RoundState::WARMUP: if(m_info.timeLeft<=0)startRound(); break;
    case RoundState::BUY:    if(m_info.timeLeft<=0){m_info.state=RoundState::LIVE;m_info.timeLeft=ROUND_TIME;} break;
    case RoundState::LIVE:   if(m_info.timeLeft<=0)endRound(Team::CT); break;
    case RoundState::END:    if(m_info.timeLeft<=0)startRound(); break;
    }
}

inline float ServerGame3D::rayVsAABB(glm::vec3 o,glm::vec3 d,const AABB& b,float maxD){
    float tmin=0,tmax=maxD;
    for(int i=0;i<3;i++){
        float di=(&d.x)[i];
        if(std::abs(di)<1e-6f){if((&o.x)[i]<(&b.min.x)[i]||(&o.x)[i]>(&b.max.x)[i])return -1;}
        else{float t1=((&b.min.x)[i]-(&o.x)[i])/di,t2=((&b.max.x)[i]-(&o.x)[i])/di;
            if(t1>t2)std::swap(t1,t2);tmin=std::max(tmin,t1);tmax=std::min(tmax,t2);if(tmin>tmax)return -1;}}
    return tmin>0?tmin:tmax;
}

} // namespace CS
