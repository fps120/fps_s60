#pragma once
#include "GameTypes.h"
#include <SFML/Graphics.hpp>
#include <deque>
#include <string>
#include <vector>

namespace CS {

struct KillEntry {
    std::string killer, victim;
    WeaponID    weapon;
    bool        headshot;
    float       timer = 5.f;
};

class HUD3D {
public:
    HUD3D(sf::Font& font) : m_font(font) {}

    void update(float dt);
    void draw(sf::RenderTarget& rt, const PlayerState& local,
              const GameInfo& info, bool scopeOn);

    void pushKill(const std::string& k, const std::string& v, WeaponID w, bool hs);
    void pushChat(const std::string& msg);
    void showMsg(const std::string& msg, float dur = 3.f);
    void showHit(bool hs);

    bool  m_chatOpen = false;
    std::string m_chatInput;

private:
    sf::Font& m_font;

    std::deque<KillEntry>   m_kills;
    std::deque<std::string> m_chat;

    std::string m_bigMsg;
    float       m_bigMsgTimer = 0.f;

    float       m_hitTimer = 0.f;
    bool        m_hitHS    = false;

    // helpers
    sf::Text txt(const std::string& s, unsigned sz,
                 sf::Color c = sf::Color::White) const {
        sf::Text t(s, m_font, sz);
        t.setFillColor(c);
        return t;
    }

    void drawCrosshair(sf::RenderTarget& rt, float cx, float cy,
                       float spread, bool crouching);
    void drawHP(sf::RenderTarget& rt, const PlayerState& ps,
                float sw, float sh);
    void drawAmmo(sf::RenderTarget& rt, const PlayerState& ps,
                  float sw, float sh);
    void drawTimer(sf::RenderTarget& rt, const GameInfo& info, float sw);
    void drawScore(sf::RenderTarget& rt, const GameInfo& info, float sw);
    void drawKillFeed(sf::RenderTarget& rt, float sw);
    void drawChat(sf::RenderTarget& rt, float sh);
    void drawBigMsg(sf::RenderTarget& rt, float sw, float sh);
    void drawHitMarker(sf::RenderTarget& rt, float cx, float cy);
    void drawScope(sf::RenderTarget& rt, float sw, float sh);
    void drawDeathScreen(sf::RenderTarget& rt, float sw, float sh);
};

} // namespace CS

// ── Implementation ─────────────────────────────────────────────────────────────
#include <cmath>
#include <algorithm>

namespace CS {

void HUD3D::update(float dt) {
    for (auto& k : m_kills) k.timer -= dt;
    m_kills.erase(std::remove_if(m_kills.begin(), m_kills.end(),
        [](auto& k){ return k.timer <= 0; }), m_kills.end());
    if (m_bigMsgTimer > 0) m_bigMsgTimer -= dt;
    if (m_hitTimer    > 0) m_hitTimer    -= dt;
}

void HUD3D::draw(sf::RenderTarget& rt, const PlayerState& local,
                  const GameInfo& info, bool scopeOn)
{
    float sw = (float)rt.getSize().x;
    float sh = (float)rt.getSize().y;
    float cx = sw / 2.f, cy = sh / 2.f;

    if (!local.alive) {
        drawDeathScreen(rt, sw, sh);
        drawScore(rt, info, sw);
        drawKillFeed(rt, sw);
        return;
    }

    if (scopeOn) {
        drawScope(rt, sw, sh);
    } else {
        drawCrosshair(rt, cx, cy, 0.f, local.crouching);
    }

    drawHP(rt, local, sw, sh);
    drawAmmo(rt, local, sw, sh);
    drawTimer(rt, info, sw);
    drawScore(rt, info, sw);
    drawKillFeed(rt, sw);
    drawChat(rt, sh);
    drawBigMsg(rt, sw, sh);
    drawHitMarker(rt, cx, cy);

    // Chat input
    if (m_chatOpen) {
        sf::RectangleShape bg({480.f, 28.f});
        bg.setPosition({cx - 240.f, sh - 90.f});
        bg.setFillColor({0,0,0,160});
        rt.draw(bg);
        sf::Text ct("Say: " + m_chatInput + "|", m_font, 15);
        ct.setFillColor(sf::Color::White);
        ct.setPosition({cx - 230.f, sh - 87.f});
        rt.draw(ct);
    }
}

void HUD3D::drawCrosshair(sf::RenderTarget& rt, float cx, float cy,
                            float spread, bool crouching)
{
    float base = crouching ? 4.f : 6.f;
    float gap  = base + spread * 0.3f;
    float len  = 10.f;
    sf::Color col(50, 240, 50, 210);

    auto line = [&](float x1,float y1,float x2,float y2) {
        sf::Vertex v[2] = {{ {x1,y1}, col }, { {x2,y2}, col }};
        rt.draw(v, 2, sf::PrimitiveType::Lines);
    };
    line(cx - gap - len, cy, cx - gap, cy);
    line(cx + gap, cy, cx + gap + len, cy);
    line(cx, cy - gap - len, cx, cy - gap);
    line(cx, cy + gap, cx, cy + gap + len);

    sf::CircleShape dot(1.5f);
    dot.setOrigin({1.5f, 1.5f});
    dot.setPosition({cx, cy});
    dot.setFillColor(col);
    rt.draw(dot);
}

void HUD3D::drawScope(sf::RenderTarget& rt, float sw, float sh) {
    // Black overlay with circular hole
    float cx = sw/2.f, cy = sh/2.f;
    float r  = std::min(sw, sh) * 0.38f;

    // Overlay (4 rectangles around the circle)
    sf::RectangleShape ovTop({sw, cy-r});
    ovTop.setPosition({0, 0});
    ovTop.setFillColor(sf::Color::Black);
    rt.draw(ovTop);

    sf::RectangleShape ovBot({sw, sh-(cy+r)});
    ovBot.setPosition({0, cy+r});
    ovBot.setFillColor(sf::Color::Black);
    rt.draw(ovBot);

    sf::RectangleShape ovL({cx-r, 2*r});
    ovL.setPosition({0, cy-r});
    ovL.setFillColor(sf::Color::Black);
    rt.draw(ovL);

    sf::RectangleShape ovR({sw-(cx+r), 2*r});
    ovR.setPosition({cx+r, cy-r});
    ovR.setFillColor(sf::Color::Black);
    rt.draw(ovR);

    // Scope reticle
    sf::Color rc(40, 210, 40, 200);
    auto line = [&](float x1,float y1,float x2,float y2) {
        sf::Vertex v[2] = {{ {x1,y1}, rc }, { {x2,y2}, rc }};
        rt.draw(v, 2, sf::PrimitiveType::Lines);
    };
    line(cx-r, cy, cx+r, cy);     // horizontal
    line(cx, cy-r, cx, cy+r);     // vertical
    line(cx-r*0.5f, cy+r*0.15f, cx+r*0.5f, cy+r*0.15f); // lower mil
}

void HUD3D::drawHP(sf::RenderTarget& rt, const PlayerState& ps,
                    float sw, float sh)
{
    float x = 30.f, y = sh - 68.f;

    sf::Text lbl = txt("HP", 13, {160,160,160});
    lbl.setPosition({x, y - 18.f});
    rt.draw(lbl);

    // Bar background
    sf::RectangleShape bg({130.f, 12.f}); bg.setPosition({x, y}); bg.setFillColor({30,30,30}); rt.draw(bg);

    float frac = std::clamp(ps.health / 100.f, 0.f, 1.f);
    sf::Color hcol = ps.health > 60 ? sf::Color{50,220,80}
                   : ps.health > 30 ? sf::Color{230,180,30}
                   :                  sf::Color{220,50,50};
    sf::RectangleShape bar({130.f * frac, 12.f}); bar.setPosition({x, y}); bar.setFillColor(hcol); rt.draw(bar);

    sf::Text num = txt(std::to_string(ps.health), 22,
                       ps.health > 30 ? sf::Color::White : sf::Color{255,100,100});
    num.setPosition({x + 140.f, y - 4.f});
    rt.draw(num);

    // Armor
    if (ps.armor > 0) {
        sf::RectangleShape abg({80.f, 6.f}); abg.setPosition({x, y+18.f}); abg.setFillColor({20,20,50}); rt.draw(abg);
        sf::RectangleShape abar({80.f*ps.armor/100.f, 6.f}); abar.setPosition({x, y+18.f}); abar.setFillColor({80,130,230}); rt.draw(abar);
        sf::Text albl = txt("ARMOR", 11, {100,140,200}); albl.setPosition({x+85.f, y+15.f}); rt.draw(albl);
    }

    // Money
    sf::Text money = txt("$" + std::to_string(ps.money), 16, {80,200,80});
    money.setPosition({x, sh - 32.f});
    rt.draw(money);
}

void HUD3D::drawAmmo(sf::RenderTarget& rt, const PlayerState& ps,
                      float sw, float sh)
{
    float ax = sw - 180.f, ay = sh - 68.f;

    sf::Text wname = txt(WEAPONS[(int)ps.weapon].name, 14, {160,185,220});
    wname.setPosition({ax, ay - 20.f});
    rt.draw(wname);

    if (ps.weapon == WeaponID::KNIFE) return;

    if (ps.reloading) {
        sf::Text rl = txt("RELOADING...", 17, {255,200,50});
        rl.setPosition({ax - 30.f, ay});
        rt.draw(rl);
        return;
    }

    sf::Color ac = ps.ammo == 0      ? sf::Color{220,60,60}
                 : ps.ammo <= 5      ? sf::Color{230,180,30}
                 :                     sf::Color::White;
    sf::Text ammo = txt(std::to_string(ps.ammo) + " / " + std::to_string(ps.ammoRes), 22, ac);
    ammo.setPosition({ax, ay - 2.f});
    rt.draw(ammo);
}

void HUD3D::drawTimer(sf::RenderTarget& rt, const GameInfo& info, float sw) {
    int s = (int)info.timeLeft;
    char buf[16]; std::snprintf(buf,sizeof(buf),"%d:%02d",s/60,s%60);
    sf::Color tc = info.timeLeft < 20.f ? sf::Color{255,80,80} : sf::Color::White;
    sf::Text t = txt(buf, 26, tc);
    t.setPosition({sw/2.f - t.getLocalBounds().size.x/2.f, 8.f});
    rt.draw(t);

    if (info.state == RoundState::BUY) {
        sf::Text buy = txt("BUY PHASE", 14, {220,200,80});
        buy.setPosition({sw/2.f - buy.getLocalBounds().size.x/2.f, 38.f});
        rt.draw(buy);
    }
}

void HUD3D::drawScore(sf::RenderTarget& rt, const GameInfo& info, float sw) {
    std::string s = "T " + std::to_string(info.tScore)
                  + "   :   " + std::to_string(info.ctScore) + " CT";
    sf::Text t = txt(s, 20, sf::Color::White);
    t.setPosition({sw/2.f - t.getLocalBounds().size.x/2.f, 56.f});
    rt.draw(t);
}

void HUD3D::drawKillFeed(sf::RenderTarget& rt, float sw) {
    float ky = 80.f;
    for (auto it = m_kills.rbegin(); it != m_kills.rend(); ++it) {
        float alpha = std::min(1.f, it->timer);
        std::string s = it->killer + (it->headshot?" [HS] ":" [") +
                        WEAPONS[(int)it->weapon].name + "] " + it->victim;
        sf::Text t = txt(s, 13, {255,230,180,(uint8_t)(200*alpha)});
        t.setPosition({sw - t.getLocalBounds().size.x - 10.f, ky});
        rt.draw(t);
        ky += 20.f;
    }
}

void HUD3D::drawChat(sf::RenderTarget& rt, float sh) {
    float cy = sh - 120.f;
    for (auto it = m_chat.rbegin(); it != m_chat.rend(); ++it) {
        sf::Text t = txt(*it, 13, {200,220,200,200});
        t.setPosition({10.f, cy});
        rt.draw(t);
        cy -= 18.f;
    }
}

void HUD3D::drawBigMsg(sf::RenderTarget& rt, float sw, float sh) {
    if (m_bigMsgTimer <= 0) return;
    float alpha = std::min(1.f, m_bigMsgTimer);
    sf::Text t = txt(m_bigMsg, 38, {255,230,80,(uint8_t)(240*alpha)});
    t.setPosition({sw/2.f - t.getLocalBounds().size.x/2.f, sh/3.f});
    rt.draw(t);
}

void HUD3D::drawHitMarker(sf::RenderTarget& rt, float cx, float cy) {
    if (m_hitTimer <= 0) return;
    float a = m_hitTimer / 0.25f;
    sf::Color col = m_hitHS ? sf::Color{255,60,60,(uint8_t)(220*a)}
                             : sf::Color{255,255,60,(uint8_t)(180*a)};
    float sz = 11.f;
    auto line = [&](float x1,float y1,float x2,float y2){
        sf::Vertex v[2]={{{x1,y1},col},{{x2,y2},col}};
        rt.draw(v,2,sf::PrimitiveType::Lines);
    };
    line(cx-sz,cy-sz, cx-4.f,cy-4.f);
    line(cx+4.f,cy-4.f, cx+sz,cy-sz);
    line(cx-sz,cy+sz, cx-4.f,cy+4.f);
    line(cx+4.f,cy+4.f, cx+sz,cy+sz);
}

void HUD3D::drawDeathScreen(sf::RenderTarget& rt, float sw, float sh) {
    sf::RectangleShape ov({sw,sh});
    ov.setFillColor({120,0,0,60});
    rt.draw(ov);

    sf::Text t = txt("YOU DIED", 52, {220,50,50,200});
    t.setPosition({sw/2.f - t.getLocalBounds().size.x/2.f, sh/2.5f});
    rt.draw(t);

    sf::Text sub = txt("Waiting for next round...", 20, {180,180,180});
    sub.setPosition({sw/2.f - sub.getLocalBounds().size.x/2.f, sh/2.5f + 70.f});
    rt.draw(sub);
}

void HUD3D::pushKill(const std::string& k, const std::string& v,
                      WeaponID w, bool hs)
{
    while (m_kills.size() >= 6) m_kills.pop_front();
    m_kills.push_back({k,v,w,hs,5.f});
}

void HUD3D::pushChat(const std::string& msg) {
    while (m_chat.size() >= 6) m_chat.pop_front();
    m_chat.push_back(msg);
}

void HUD3D::showMsg(const std::string& msg, float dur) {
    m_bigMsg = msg; m_bigMsgTimer = dur;
}

void HUD3D::showHit(bool hs) {
    m_hitTimer = 0.25f; m_hitHS = hs;
}

} // namespace CS
