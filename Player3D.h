#pragma once
#include "GameTypes.h"
#include "Camera.h"
#include <vector>
#include <random>
typedef struct{glm::vec3 min,max;bool overlaps(const glm::vec3& o)const{return true;}} _unused;
namespace CS{
class Map3D;
class Player3D{
public:
    Player3D()=default;
    explicit Player3D(const PlayerState& st):m_st(st){}
    uint8_t getId()const{return m_st.id;}
    Team getTeam()const{return m_st.team;}
    bool isAlive()const{return m_st.alive;}
    bool isLocal()const{return m_local;}
    void setLocal(bool v){m_local=v;}
    const PlayerState& state()const{return m_st;}
    void setState(const PlayerState& s){m_st=s;}
    glm::vec3 eyePos()const{float h=m_st.crouching?1.1f:PLAYER_EYE_H;return{m_st.x,m_st.y+h,m_st.z};}
    glm::vec3 feetPos()const{return{m_st.x,m_st.y,m_st.z};}
    void handleInput(const InputState& in,const std::vector<AABB>& solids,float dt);
    bool tryShoot(const Camera& cam,const std::vector<AABB>& solids,const std::vector<Player3D>& others,BulletEvent& evt);
    void startReload();
    void applyDamage(int dmg,bool hs);
    void respawn(const SpawnPoint& sp);
    void reconcile(const PlayerState& auth);
    void updateWeapon(float dt);
    bool canShoot()const{return m_fireTimer<=0&&!m_st.reloading&&m_st.ammo>0;}
    AABB playerAABB()const{float h=m_st.crouching?PLAYER_CROUCH_H:PLAYER_HEIGHT;return{{m_st.x-PLAYER_RADIUS,m_st.y,m_st.z-PLAYER_RADIUS},{m_st.x+PLAYER_RADIUS,m_st.y+h,m_st.z+PLAYER_RADIUS}};}
    float m_velX=0,m_velY=0,m_velZ=0;bool m_grounded=false;
    float m_fireTimer=0,m_reloadTimer=0,m_spread=0,m_bobT=0;
private:
    PlayerState m_st;bool m_local=false;
    std::mt19937 m_rng{std::random_device{}()};
    void moveAndCollide(float dx,float dy,float dz,const std::vector<AABB>& solids);
    static float rayAABB(glm::vec3 o,glm::vec3 d,const AABB& b,float maxD);
};}
