#include "Player3D.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace CS {

Player3D::Player3D(const PlayerState& st) : m_st(st) {}

// ─── HANDLE INPUT ─────────────────────────────────────────────────────────────
void Player3D::handleInput(const InputState& in,
                            const std::vector<AABB>& solids,
                            float dt)
{
    if (!m_st.alive) return;

    // ── view angles ───────────────────────────────────────────────────────────
    m_st.yaw   = in.yaw;
    m_st.pitch = in.pitch;

    // ── weapon slot ───────────────────────────────────────────────────────────
    static const WeaponID slots[]={WeaponID::KNIFE,WeaponID::GLOCK,WeaponID::MP5,
                                    WeaponID::AK47, WeaponID::AWP, WeaponID::M3};
    if (in.slot < 6) m_st.weapon = slots[in.slot];

    // ── crouch ────────────────────────────────────────────────────────────────
    m_st.crouching = in.crouch;

    // ── horizontal movement ───────────────────────────────────────────────────
    float yaw = m_st.yaw;
    glm::vec3 fwd  = glm::normalize(glm::vec3(sinf(yaw),  0, cosf(yaw)));
    glm::vec3 rgt  = glm::normalize(glm::vec3(cosf(yaw),  0,-sinf(yaw)));

    glm::vec3 wishDir{0,0,0};
    if (in.fwd)   wishDir += fwd;
    if (in.back)  wishDir -= fwd;
    if (in.right) wishDir += rgt;
    if (in.left)  wishDir -= rgt;

    float wlen = glm::length(wishDir);
    if (wlen > 0.001f) wishDir /= wlen;

    float speed = m_st.crouching ? CROUCH_SPEED
                 : /* run always  */ RUN_SPEED;
    speed *= WEAPONS[(int)m_st.weapon].speedMul;

    const float ACCEL   = 30.f;
    const float FRICTION= 18.f;

    // Accelerate towards wish direction
    m_velX += wishDir.x * ACCEL * dt;
    m_velZ += wishDir.z * ACCEL * dt;

    // Friction (only horizontal)
    float hspd = std::sqrtf(m_velX*m_velX + m_velZ*m_velZ);
    if (hspd > 0.001f) {
        float newSpd = std::max(0.f, hspd - FRICTION * dt);
        float clamp  = std::min(newSpd, speed);
        m_velX = m_velX / hspd * clamp;
        m_velZ = m_velZ / hspd * clamp;
    }

    // ── jump ──────────────────────────────────────────────────────────────────
    if (in.jump && m_grounded) {
        m_velY    = JUMP_FORCE;
        m_grounded = false;
    }

    // ── gravity ───────────────────────────────────────────────────────────────
    if (!m_grounded) m_velY -= GRAVITY * dt;

    // ── move & collide ────────────────────────────────────────────────────────
    moveAndCollide(m_velX*dt, m_velY*dt, m_velZ*dt, solids);

    // ── weapon timers ─────────────────────────────────────────────────────────
    updateWeapon(dt);

    // ── reload ────────────────────────────────────────────────────────────────
    if (in.reload && !m_st.reloading && m_st.ammoRes > 0
        && m_st.ammo < WEAPONS[(int)m_st.weapon].magSize)
        startReload();

    // ── head-bob ──────────────────────────────────────────────────────────────
    if (m_grounded && hspd > 0.5f)
        m_bobT += dt * hspd * 1.2f;
}

// ─── MOVE & COLLIDE (AABB sweep) ─────────────────────────────────────────────
void Player3D::moveAndCollide(float dx, float dy, float dz,
                               const std::vector<AABB>& solids)
{
    // Separate axes for sliding

    // ── X ────────────────────────────────────────────────────────────────────
    m_st.x += dx;
    AABB a = playerAABB();
    for (auto& s : solids) {
        if (!a.overlaps(s)) continue;
        // Push out on X
        float pushRight = s.max.x - a.min.x;
        float pushLeft  = a.max.x - s.min.x;
        if (pushRight < pushLeft) m_st.x += pushRight;
        else                      m_st.x -= pushLeft;
        m_velX = 0;
        a = playerAABB();
    }

    // ── Z ────────────────────────────────────────────────────────────────────
    m_st.z += dz;
    a = playerAABB();
    for (auto& s : solids) {
        if (!a.overlaps(s)) continue;
        float pushFar  = s.max.z - a.min.z;
        float pushNear = a.max.z - s.min.z;
        if (pushFar < pushNear) m_st.z += pushFar;
        else                    m_st.z -= pushNear;
        m_velZ = 0;
        a = playerAABB();
    }

    // ── Y (vertical) ─────────────────────────────────────────────────────────
    m_st.y += dy;
    a = playerAABB();
    m_grounded = false;

    for (auto& s : solids) {
        if (!a.overlaps(s)) continue;

        float pushUp   = s.max.y - a.min.y;  // solid top  - player bottom
        float pushDown = a.max.y - s.min.y;  // player top - solid bottom

        if (pushUp < pushDown) {
            // Landed on top
            m_st.y += pushUp;
            m_velY   = 0;
            m_grounded = true;
        } else {
            // Hit ceiling
            m_st.y -= pushDown;
            m_velY   = 0;
        }
        a = playerAABB();
    }

    // Floor at y=0
    if (m_st.y < 0.f) {
        m_st.y     = 0.f;
        m_velY     = 0.f;
        m_grounded = true;
    }
}

// ─── WEAPON UPDATE ────────────────────────────────────────────────────────────
void Player3D::updateWeapon(float dt) {
    if (m_fireTimer > 0) m_fireTimer -= dt;
    if (m_st.reloading) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0) {
            const auto& wd = WEAPONS[(int)m_st.weapon];
            m_st.reloading = false;
            int need  = wd.magSize - m_st.ammo;
            int given = std::min((int)m_st.ammoRes, need);
            m_st.ammo    += given;
            m_st.ammoRes -= given;
        }
    }
    m_spread = std::max(0.f, m_spread - dt * 10.f);
}

// ─── SHOOT ────────────────────────────────────────────────────────────────────
bool Player3D::tryShoot(const Camera& cam,
                         const std::vector<AABB>& solids,
                         const std::vector<Player3D>& others,
                         BulletEvent& evt)
{
    if (!canShoot()) return false;
    if (m_st.weapon == WeaponID::KNIFE) return false;

    const auto& wd = WEAPONS[(int)m_st.weapon];

    // Apply spread
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    float spreadRad = glm::radians(wd.spread + m_spread);
    glm::vec3 dir = cam.forward();
    // Perturb direction
    glm::vec3 r = cam.right(), u = cam.up();
    dir += r * dist(m_rng) * spreadRad + u * dist(m_rng) * spreadRad;
    dir = glm::normalize(dir);

    // Raycast vs walls
    float  closestWall = wd.range;
    for (auto& s : solids) {
        float t = rayAABB(cam.pos, dir, s, wd.range);
        if (t > 0 && t < closestWall) closestWall = t;
    }

    // Raycast vs players
    float closestPlayer = closestWall;
    uint8_t hitId = 255;
    bool    hitHS = false;
    for (auto& p : others) {
        if (!p.isAlive() || p.getId() == m_st.id) continue;
        if (p.getTeam() == m_st.team) continue; // FF off

        // Try body AABB
        float t = rayAABB(cam.pos, dir, p.playerAABB(), closestWall);
        if (t > 0 && t < closestPlayer) {
            closestPlayer = t;
            hitId = p.getId();
            // Head zone: top 20% of height
            float h = p.m_st.crouching ? PLAYER_CROUCH_H : PLAYER_HEIGHT;
            glm::vec3 hitPt = cam.pos + dir * t;
            hitHS = (hitPt.y > p.feetPos().y + h * 0.8f);
        }
    }

    evt.shooterId = m_st.id;
    evt.ox = cam.pos.x; evt.oy = cam.pos.y; evt.oz = cam.pos.z;
    evt.dx = dir.x;     evt.dy = dir.y;     evt.dz = dir.z;
    evt.dist   = closestPlayer;
    evt.weapon = m_st.weapon;
    evt.hit    = (hitId != 255);

    m_st.ammo--;
    m_fireTimer  = 1.f / wd.fireRate;
    m_spread     = std::min(m_spread + wd.spread * 0.5f, wd.spread * 4.f);

    if (m_st.ammo == 0 && m_st.ammoRes > 0) startReload();
    return true;
}

void Player3D::startReload() {
    if (m_st.reloading || m_st.weapon == WeaponID::KNIFE) return;
    m_st.reloading = true;
    m_reloadTimer  = WEAPONS[(int)m_st.weapon].reloadTime;
}

void Player3D::applyDamage(int dmg, bool hs) {
    if (!m_st.alive) return;
    int total = hs ? (int)(dmg * WEAPONS[(int)m_st.weapon].headshotMul) : dmg;
    if (m_st.armor > 0) {
        int abs = std::min((int)m_st.armor, total/2);
        m_st.armor -= abs; total -= abs;
    }
    m_st.health -= total;
    if (m_st.health <= 0) {
        m_st.health = 0;
        m_st.alive  = false;
        m_st.deaths++;
    }
}

void Player3D::respawn(const SpawnPoint& sp) {
    m_st.x = sp.x; m_st.y = sp.y; m_st.z = sp.z;
    m_st.yaw   = sp.yaw; m_st.pitch = 0;
    m_velX = m_velY = m_velZ = 0;
    m_grounded = false;
    m_st.health = 100; m_st.armor = 0;
    m_st.alive  = true; m_st.reloading = false;
    m_st.weapon = WeaponID::GLOCK;
    m_st.ammo   = WEAPONS[(int)WeaponID::GLOCK].magSize;
    m_st.ammoRes= 60;
    m_fireTimer = m_reloadTimer = m_spread = 0;
}

void Player3D::reconcile(const PlayerState& auth) {
    float dx = auth.x-m_st.x, dz = auth.z-m_st.z;
    if (dx*dx+dz*dz > 0.25f) { m_st.x=auth.x; m_st.z=auth.z; }
    m_st.y=auth.y; m_st.health=auth.health;
    m_st.armor=auth.armor; m_st.alive=auth.alive;
    m_st.ammo=auth.ammo; m_st.ammoRes=auth.ammoRes;
    m_st.kills=auth.kills; m_st.deaths=auth.deaths;
    m_st.money=auth.money;
}

// ─── RAY VS AABB ──────────────────────────────────────────────────────────────
float Player3D::rayAABB(glm::vec3 o, glm::vec3 d, const AABB& b, float maxD) {
    float tmin=0, tmax=maxD;
    for (int i=0;i<3;i++) {
        float di = (&d.x)[i];
        if (std::abs(di)<1e-6f) {
            if ((&o.x)[i]<(&b.min.x)[i]||(&o.x)[i]>(&b.max.x)[i]) return -1;
        } else {
            float t1=((&b.min.x)[i]-(&o.x)[i])/di;
            float t2=((&b.max.x)[i]-(&o.x)[i])/di;
            if(t1>t2)std::swap(t1,t2);
            tmin=std::max(tmin,t1);
            tmax=std::min(tmax,t2);
            if(tmin>tmax)return -1;
        }
    }
    return tmin>0?tmin:tmax;
}

} // namespace CS
