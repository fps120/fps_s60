#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <array>

// ── GLM forward (shared headers must compile without OpenGL) ──────────────────
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace CS {

// ─── PHYSICS ──────────────────────────────────────────────────────────────────
constexpr float GRAVITY        = 18.0f;   // m/s²
constexpr float JUMP_FORCE     = 6.8f;    // m/s initial vertical velocity
constexpr float WALK_SPEED     = 4.5f;    // m/s
constexpr float RUN_SPEED      = 7.0f;    // m/s (holding shift)
constexpr float CROUCH_SPEED   = 2.5f;    // m/s
constexpr float PLAYER_HEIGHT  = 1.75f;   // m
constexpr float PLAYER_EYE_H   = 1.62f;   // m  (camera height from feet)
constexpr float PLAYER_CROUCH_H= 1.2f;    // m
constexpr float PLAYER_RADIUS  = 0.3f;    // m  (capsule radius)
constexpr float STEP_HEIGHT    = 0.3f;    // m  (max step the player can climb)

// ─── LIMITS ───────────────────────────────────────────────────────────────────
constexpr int   MAX_PLAYERS    = 20;
constexpr float ROUND_TIME     = 115.0f;
constexpr float BUY_PHASE_TIME = 15.0f;
constexpr int   MAX_ROUNDS     = 30;
constexpr uint32_t TICK_RATE   = 64;

// ─── ENUMS ────────────────────────────────────────────────────────────────────
enum class Team : uint8_t  { UNASSIGNED=0, TERRORIST=1, CT=2, SPECTATOR=3 };
enum class WeaponID : uint8_t { KNIFE=0, GLOCK=1, MP5=2, AK47=3, AWP=4, M3=5 };
enum class RoundState : uint8_t { WARMUP=0, BUY=1, LIVE=2, END=3 };

// ─── WEAPON DATA ──────────────────────────────────────────────────────────────
struct WeaponData {
    const char* name;
    float damage;
    float headshotMul;
    float fireRate;       // shots/sec
    float reloadTime;     // sec
    int   magSize;
    float spread;         // base spread in degrees
    float range;          // max range in metres
    bool  automatic;
    int   cost;
    float speedMul;       // movement speed multiplier
    // Viewmodel colours (procedural model)
    glm::vec3 bodyCol;
    glm::vec3 slideCol;
};

inline const WeaponData WEAPONS[] = {
 // name       dmg  hs   fRate  rld   mag  spr  rng    auto  cost  spd   body                     slide
 {"Knife",     34,  2.5f, 0,    0,    0,   0,   2.5f, false,    0, 1.2f, {0.5f,0.5f,0.5f},       {0.7f,0.7f,0.7f}},
 {"Glock-18",  25,  4.0f, 4.0f, 1.8f, 20,  3.5f,50,  false,  200, 1.0f, {0.2f,0.2f,0.2f},       {0.3f,0.3f,0.3f}},
 {"MP5",       26,  3.5f,10.0f, 2.5f, 30,  2.5f,40,  true,  1500, 0.95f,{0.25f,0.25f,0.25f},    {0.4f,0.4f,0.4f}},
 {"AK-47",     36,  4.0f,10.0f, 2.8f, 30,  1.5f,80,  true,  2500, 0.88f,{0.5f,0.35f,0.1f},      {0.3f,0.3f,0.3f}},
 {"AWP",      115,  1.5f, 1.5f, 3.5f,  5,  0.3f,200, false, 4750, 0.75f,{0.6f,0.55f,0.45f},     {0.2f,0.2f,0.2f}},
 {"M3",        20,  2.0f, 1.0f, 3.2f,  8,  8.0f,15,  false, 1700, 0.9f, {0.35f,0.25f,0.15f},    {0.2f,0.2f,0.2f}},
};

// ─── PLAYER STATE (network-shared) ───────────────────────────────────────────
struct PlayerState {
    uint8_t  id         = 0;
    char     name[24]   = {};
    Team     team       = Team::UNASSIGNED;
    float    x=0,y=0,z=0;      // world position (feet)
    float    yaw=0,pitch=0;    // view angles (radians)
    int16_t  health     = 100;
    int16_t  armor      = 0;
    WeaponID weapon     = WeaponID::GLOCK;
    uint8_t  ammo       = 20;
    uint8_t  ammoRes    = 60;
    bool     alive      = true;
    bool     reloading  = false;
    bool     crouching  = false;
    uint16_t kills      = 0;
    uint16_t deaths     = 0;
    uint16_t money      = 800;
};

// ─── BULLET EVENT ─────────────────────────────────────────────────────────────
struct BulletEvent {
    uint8_t  shooterId;
    float    ox,oy,oz;    // origin
    float    dx,dy,dz;    // direction (normalised)
    float    dist;        // distance to impact
    WeaponID weapon;
    bool     hit;
};

// ─── GAME INFO ────────────────────────────────────────────────────────────────
struct GameInfo {
    RoundState state     = RoundState::WARMUP;
    uint8_t    tScore   = 0;
    uint8_t    ctScore  = 0;
    float      timeLeft = ROUND_TIME;
    uint16_t   round    = 1;
};

// ─── INPUT ────────────────────────────────────────────────────────────────────
struct InputState {
    bool  fwd=false, back=false, left=false, right=false;
    bool  jump=false, crouch=false;
    bool  shoot=false, reload=false;
    float yaw=0, pitch=0;
    uint8_t slot=1;
};

// ─── AABB (used for collision) ────────────────────────────────────────────────
struct AABB {
    glm::vec3 min, max;
    bool contains(glm::vec3 p) const {
        return p.x>=min.x&&p.x<=max.x&&p.y>=min.y&&p.y<=max.y&&p.z>=min.z&&p.z<=max.z;
    }
    bool overlaps(const AABB& o) const {
        return min.x<o.max.x&&max.x>o.min.x&&
               min.y<o.max.y&&max.y>o.min.y&&
               min.z<o.max.z&&max.z>o.min.z;
    }
    glm::vec3 centre() const { return (min+max)*0.5f; }
    glm::vec3 size()   const { return max-min; }
};

// ─── SPAWN POINT ──────────────────────────────────────────────────────────────
struct SpawnPoint { float x,y,z; Team team; float yaw; };

} // namespace CS
