#pragma once
#include "GameTypes.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace CS {

struct Settings3D {
    float mouseSens   = 0.18f;
    float masterVol   = 80.f;
    float fov         = 90.f;
    bool  fullscreen  = false;
    int   resW        = 1280, resH = 720;
    std::string playerName = "Player";
    std::string lastIP     = "127.0.0.1";
    int         lastPort   = 27015;

    void save(const std::string& p) const;
    bool load(const std::string& p);
};

enum class MenuState { MAIN, CONNECT, SETTINGS, TEAM, SCOREBOARD };

struct ConnectInfo {
    std::string ip;
    int         port;
    std::string name;
    Team        team = Team::UNASSIGNED;
};

class Menu3D {
public:
    Menu3D(sf::RenderWindow& win, sf::Font& font);

    // Returns true once team selected and ready to connect
    bool update(float dt);
    void handleEvent(const sf::Event& ev);
    void draw();

    MenuState        getState()    const { return m_state;    }
    const ConnectInfo& getConn()   const { return m_conn;     }
    const Settings3D& getSettings()const { return m_cfg;      }
    Team             getTeam()     const { return m_team;     }
    void             setState(MenuState s){ m_state = s;       }

    void showMsg(const std::string& msg, float dur = 3.f);
    void updateScoreboard(const std::vector<PlayerState>& ps, const GameInfo& gi);
    bool readyToConnect = false;

private:
    sf::RenderWindow& m_win;
    sf::Font&         m_font;
    Settings3D        m_cfg;
    MenuState         m_state = MenuState::MAIN;
    Team              m_team  = Team::UNASSIGNED;
    ConnectInfo       m_conn;

    // Input fields
    std::string m_nameIn, m_ipIn, m_portIn;
    int         m_focus = 0;

    // Scoreboard data
    std::vector<PlayerState> m_sbPlayers;
    GameInfo                 m_sbInfo;

    std::string m_msg;
    float       m_msgTimer = 0.f;
    float       m_anim     = 0.f;

    void drawMain();
    void drawConnect();
    void drawSettings();
    void drawTeam();
    void drawScoreboard();
    void drawMsg();
    void drawBg();

    sf::Text makeText(const std::string& s, unsigned sz,
                      sf::Color c = sf::Color::White) const;
    bool button(float cx, float cy, float w, float h,
                const std::string& lbl, float mx, float my, bool clicked,
                sf::Color bg={25,35,70}, sf::Color hover={50,80,160});
    void panel(float x, float y, float w, float h);
};

} // namespace CS

// ── Implementation ──────────────────────────────────────────────────────────
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace CS {

// Settings I/O
void Settings3D::save(const std::string& p) const {
    std::ofstream f(p);
    if (!f) return;
    f<<"sens "<<mouseSens<<"\nvol "<<masterVol<<"\nfov "<<fov
     <<"\nname "<<playerName<<"\nlastip "<<lastIP
     <<"\nlastport "<<lastPort<<"\nresw "<<resW<<"\nresh "<<resH<<"\n";
}
bool Settings3D::load(const std::string& p) {
    std::ifstream f(p); if(!f) return false;
    std::string k;
    while(f>>k){
        if(k=="sens")   f>>mouseSens;
        else if(k=="vol")  f>>masterVol;
        else if(k=="fov")  f>>fov;
        else if(k=="name") f>>playerName;
        else if(k=="lastip")   f>>lastIP;
        else if(k=="lastport") f>>lastPort;
        else if(k=="resw") f>>resW;
        else if(k=="resh") f>>resH;
    }
    return true;
}

Menu3D::Menu3D(sf::RenderWindow& win, sf::Font& font)
    : m_win(win), m_font(font)
{
    m_cfg.load("config.cfg");
    m_nameIn = m_cfg.playerName;
    m_ipIn   = m_cfg.lastIP;
    m_portIn = std::to_string(m_cfg.lastPort);
}

bool Menu3D::update(float dt) {
    m_anim += dt * 0.35f;
    if (m_msgTimer > 0) m_msgTimer -= dt;
    return false;
}

void Menu3D::handleEvent(const sf::Event& ev) {
    if (m_state != MenuState::CONNECT && m_state != MenuState::SETTINGS) return;
    if (true) return;
    uint32_t c = 0;
    std::string* field = (m_focus==0)?&m_nameIn:(m_focus==1)?&m_ipIn:&m_portIn;
    if (c==8 && !field->empty()) field->pop_back();
    else if (c>=32 && c<127 && field->size()<40) *field+=(char)c;
}

void Menu3D::draw() {
    drawBg();
    switch(m_state){
        case MenuState::MAIN:        drawMain();       break;
        case MenuState::CONNECT:     drawConnect();    break;
        case MenuState::SETTINGS:    drawSettings();   break;
        case MenuState::TEAM:        drawTeam();       break;
        case MenuState::SCOREBOARD:  drawScoreboard(); break;
    }
    drawMsg();
}

void Menu3D::drawBg() {
    auto sz = m_win.getSize();
    sf::RectangleShape bg({(float)sz.x,(float)sz.y});
    bg.setFillColor({8,10,16});
    m_win.draw(bg);
    // Moving diagonal grid
    sf::Color lc{20,30,55,70};
    float sp = 70.f, off = std::fmod(m_anim*22.f, sp);
    for(float x=-off; x<sz.x+sp; x+=sp){
        sf::Vertex l[2]={{{x,0},lc},{{x+(float)sz.y*0.4f,(float)sz.y},lc}};
        m_win.draw(l,2,sf::PrimitiveType::Lines);
    }
}

sf::Text Menu3D::makeText(const std::string& s,unsigned sz,sf::Color c) const {
    sf::Text t(s,m_font,sz); t.setFillColor(c); return t;
}

void Menu3D::panel(float x,float y,float w,float h){
    sf::RectangleShape r({w,h}); r.setPosition({x, y});
    r.setFillColor({18,22,35,230});
    r.setOutlineThickness(1.5f); r.setOutlineColor({60,80,130});
    m_win.draw(r);
}

bool Menu3D::button(float cx,float cy,float w,float h,
                     const std::string& lbl,
                     float mx,float my,bool clicked,
                     sf::Color bg,sf::Color hover)
{
    bool hov = mx>cx-w/2&&mx<cx+w/2&&my>cy-h/2&&my<cy+h/2;
    sf::RectangleShape r({w,h}); r.setPosition({cx-w/2, cy-h/2});
    r.setFillColor(hov?hover:bg);
    r.setOutlineThickness(1.f);
    r.setOutlineColor(hov?sf::Color{100,150,255}:sf::Color{45,60,110});
    m_win.draw(r);
    sf::Text t=makeText(lbl,18,hov?sf::Color::White:sf::Color{160,185,230});
    t.setPosition({cx-t.getLocalBounds().size.x/2.f, cy-10.f});
    m_win.draw(t);
    return hov&&clicked;
}

void Menu3D::drawMain(){
    auto sz=m_win.getSize();
    float cx=sz.x/2.f, cy=sz.y/2.f;

    sf::Text title=makeText("CS 1.6  |  3D",52,{190,210,255});
    title.setPosition({cx-title.getLocalBounds().size.x/2.f, 50.f});
    m_win.draw(title);
    sf::Text sub=makeText("Counter-Strike  First Person",16,{80,110,170});
    sub.setPosition({cx-sub.getLocalBounds().size.x/2.f, 110.f});
    m_win.draw(sub);

    panel(cx-160.f,cy-110.f,320.f,280.f);

    sf::Vector2i mp=sf::Mouse::getPosition(m_win);
    float mx=(float)mp.x,my=(float)mp.y;
    static bool wd=false;
    bool lmb=sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool clicked=lmb&&!wd; wd=lmb;

    if(button(cx,cy-70.f,260.f,46.f,"PLAY / CONNECT",mx,my,clicked)) m_state=MenuState::CONNECT;
    if(button(cx,cy,      260.f,46.f,"SETTINGS",       mx,my,clicked)) m_state=MenuState::SETTINGS;
    if(button(cx,cy+70.f, 260.f,46.f,"QUIT",           mx,my,clicked,{60,15,15},{140,30,30})) m_win.close();

    sf::Text ver=makeText("v1.0  OpenGL 3.3  SFML 2.5",11,{40,55,80});
    ver.setPosition({8.f, (float})sz.y-20.f);
    m_win.draw(ver);
}

void Menu3D::drawConnect(){
    auto sz=m_win.getSize();
    float cx=sz.x/2.f,cy=sz.y/2.f;
    panel(cx-240.f,cy-200.f,480.f,400.f);

    sf::Text hdr=makeText("CONNECT TO SERVER",26,{180,205,255});
    hdr.setPosition({cx-hdr.getLocalBounds().size.x/2.f, cy-185.f});
    m_win.draw(hdr);

    sf::Vector2i mp=sf::Mouse::getPosition(m_win);
    float mx=(float)mp.x,my=(float)mp.y;
    static bool wd=false;
    bool lmb=sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool clicked=lmb&&!wd; wd=lmb;

    struct F{std::string lbl;std::string* val;int id;};
    F fields[]={{"Name:",  &m_nameIn,0},{"Server IP:", &m_ipIn,1},{"Port:", &m_portIn,2}};
    float fy=cy-120.f;
    for(auto& f:fields){
        sf::Text lbl=makeText(f.lbl,14,{140,165,210});
        lbl.setPosition({cx-200.f, fy}); m_win.draw(lbl);
        bool foc=(m_focus==f.id);
        sf::RectangleShape box({300.f,30.f}); box.setPosition({cx-150.f, fy+18.f});
        box.setFillColor({12,16,32}); box.setOutlineThickness(1.5f);
        box.setOutlineColor(foc?sf::Color{100,155,255}:sf::Color{50,65,110});
        m_win.draw(box);
        std::string disp=*f.val+(foc&&(int)(m_anim*2)%2==0?"|":"");
        sf::Text vt=makeText(disp,15,sf::Color::White);
        vt.setPosition({cx-142.f, fy+20.f}); m_win.draw(vt);
        if(clicked&&box.getGlobalBounds().contains({mx, my})) m_focus=f.id;
        fy+=72.f;
    }

    if(button(cx+60.f,cy+130.f,160.f,44.f,"CONNECT",mx,my,clicked,{15,70,35},{30,150,70})){
        m_conn.ip   = m_ipIn;
        m_conn.port = m_portIn.empty()?27015:std::stoi(m_portIn);
        m_conn.name = m_nameIn;
        m_cfg.lastIP=m_ipIn; m_cfg.lastPort=m_conn.port; m_cfg.playerName=m_nameIn;
        m_cfg.save("config.cfg");
        m_state = MenuState::TEAM;
    }
    if(button(cx-160.f,cy+130.f,100.f,36.f,"< BACK",mx,my,clicked,{0,0,0,0},{0,0,0,0})){
        sf::Text bt=makeText("< BACK",14,{100,130,180});
        bt.setPosition({cx-210.f, cy+121.f}); m_win.draw(bt);
        m_state=MenuState::MAIN;
    }
}

void Menu3D::drawSettings(){
    auto sz=m_win.getSize();
    float cx=sz.x/2.f,cy=sz.y/2.f;
    panel(cx-280.f,cy-220.f,560.f,440.f);

    sf::Text hdr=makeText("SETTINGS",26,{180,205,255});
    hdr.setPosition({cx-hdr.getLocalBounds().size.x/2.f, cy-205.f});
    m_win.draw(hdr);

    sf::Vector2i mp=sf::Mouse::getPosition(m_win);
    bool lmb=sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    static bool wd=false; bool clicked=lmb&&!wd; wd=lmb;
    float mx=(float)mp.x,my=(float)mp.y;

    auto slider=[&](const char* lbl,float& val,float mn,float mx2,float fy){
        sf::Text l=makeText(std::string(lbl)+": "+std::to_string((int)val),14,{140,165,210});
        l.setPosition({cx-240.f, fy}); m_win.draw(l);
        float slX=cx-80.f,slW=220.f;
        sf::RectangleShape tr({slW,4.f}); tr.setPosition({slX, fy+22.f}); tr.setFillColor({35,45,80}); m_win.draw(tr);
        float frac=(val-mn)/(mx2-mn);
        float kx=slX+frac*slW;
        sf::CircleShape k(7.f); k.setOrigin({7.f, 7.f}); k.setPosition({kx, fy+24.f});
        bool kh=k.getGlobalBounds().contains({mx, my});
        k.setFillColor(kh?sf::Color{100,160,255}:sf::Color{60,100,200});
        m_win.draw(k);
        static bool drag=false;
        if(kh&&clicked) drag=true; if(!lmb) drag=false;
        if(drag){ frac=std::clamp((mx-slX)/slW,0.f,1.f); val=mn+frac*(mx2-mn); }
    };

    float fy=cy-140.f;
    slider("Mouse Sensitivity (×100)",m_cfg.mouseSens,0.01f,1.0f,fy); fy+=65.f;
    slider("Volume",         m_cfg.masterVol, 0.f,100.f, fy); fy+=65.f;
    slider("FOV",            m_cfg.fov,       60.f,110.f,fy); fy+=65.f;

    if(button(cx+70.f,cy+150.f,140.f,42.f,"SAVE",mx,my,clicked,{15,60,30},{30,130,65})){
        m_cfg.save("config.cfg"); showMsg("Settings saved!"); m_state=MenuState::MAIN;
    }
    if(button(cx-160.f,cy+150.f,100.f,36.f,"< BACK",mx,my,clicked,{0,0,0,0},{0,0,0,0}))
        m_state=MenuState::MAIN;
}

void Menu3D::drawTeam(){
    auto sz=m_win.getSize();
    float cx=sz.x/2.f,cy=sz.y/2.f;
    panel(cx-340.f,cy-190.f,680.f,380.f);

    sf::Text hdr=makeText("CHOOSE YOUR SIDE",28,{200,220,255});
    hdr.setPosition({cx-hdr.getLocalBounds().size.x/2.f, cy-175.f});
    m_win.draw(hdr);

    sf::Vector2i mp=sf::Mouse::getPosition(m_win);
    bool lmb=sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    static bool wd=false; bool clicked=lmb&&!wd; wd=lmb;
    float mx=(float)mp.x,my=(float)mp.y;

    // T Panel
    {
        bool hov=mx>cx-320.f&&mx<cx-10.f&&my>cy-110.f&&my<cy+90.f;
        sf::RectangleShape p({300.f,200.f}); p.setPosition({cx-320.f, cy-110.f});
        p.setFillColor(hov?sf::Color{70,15,15,200}:sf::Color{40,10,10,180});
        p.setOutlineThickness(2.f);
        p.setOutlineColor(m_team==Team::TERRORIST?sf::Color{255,80,80}:sf::Color{110,35,35});
        m_win.draw(p);
        sf::Text t=makeText("TERRORIST",22,{255,90,90}); t.setPosition({cx-300.f, cy-95.f}); m_win.draw(t);
        sf::Text d=makeText("AK-47  |  Offensive\nPlant the bomb\nDefault pistol: Glock",13,{200,150,150});
        d.setPosition({cx-310.f, cy-55.f}); m_win.draw(d);
        if(hov&&clicked) m_team=Team::TERRORIST;
    }

    // CT Panel
    {
        bool hov=mx>cx+10.f&&mx<cx+320.f&&my>cy-110.f&&my<cy+90.f;
        sf::RectangleShape p({300.f,200.f}); p.setPosition({cx+10.f, cy-110.f});
        p.setFillColor(hov?sf::Color{15,15,70,200}:sf::Color{10,10,40,180});
        p.setOutlineThickness(2.f);
        p.setOutlineColor(m_team==Team::CT?sf::Color{80,140,255}:sf::Color{35,55,130});
        m_win.draw(p);
        sf::Text t=makeText("COUNTER-TERRORIST",18,{90,160,255}); t.setPosition({cx+18.f, cy-95.f}); m_win.draw(t);
        sf::Text d=makeText("M4A1 / AWP  |  Defensive\nDefuse the bomb\nDefault pistol: Glock",13,{150,165,210});
        d.setPosition({cx+20.f, cy-55.f}); m_win.draw(d);
        if(hov&&clicked) m_team=Team::CT;
    }

    if(m_team!=Team::UNASSIGNED){
        if(button(cx,cy+130.f,200.f,44.f,"CONFIRM",mx,my,clicked,{20,80,40},{40,160,75})){
            m_conn.team    = m_team;
            readyToConnect = true;
        }
    }
    sf::Text back=makeText("< BACK",14,{100,130,180}); back.setPosition({cx-320.f, cy+130.f});
    m_win.draw(back);
    sf::FloatRect br(cx-320.f,cy+122.f,80.f,22.f);
    if(br.contains({mx, my})){back.setFillColor(sf::Color::White); if(clicked)m_state=MenuState::CONNECT;}
}

void Menu3D::drawScoreboard(){
    auto sz=m_win.getSize();
    float cx=sz.x/2.f,cy=sz.y/2.f;
    panel(cx-360.f,cy-240.f,720.f,480.f);

    sf::Text hdr=makeText("SCOREBOARD",24,{190,210,255});
    hdr.setPosition({cx-hdr.getLocalBounds().size.x/2.f, cy-225.f});
    m_win.draw(hdr);

    std::string sc="T  "+std::to_string(m_sbInfo.tScore)+"   :   "+std::to_string(m_sbInfo.ctScore)+"  CT";
    sf::Text st=makeText(sc,26,sf::Color::White);
    st.setPosition({cx-st.getLocalBounds().size.x/2.f, cy-185.f});
    m_win.draw(st);

    float py=cy-145.f;
    sf::Text col1=makeText("PLAYER",12,{110,140,200}); col1.setPosition({cx-320.f, py}); m_win.draw(col1);
    sf::Text col2=makeText("K",12,{110,140,200}); col2.setPosition({cx+100.f, py}); m_win.draw(col2);
    sf::Text col3=makeText("D",12,{110,140,200}); col3.setPosition({cx+170.f, py}); m_win.draw(col3);
    sf::Text col4=makeText("HP",12,{110,140,200}); col4.setPosition({cx+240.f, py}); m_win.draw(col4);

    sf::RectangleShape sep({700.f,1.f}); sep.setPosition({cx-350.f, py+18.f}); sep.setFillColor({50,65,110}); m_win.draw(sep);
    py+=28.f;

    for(auto& ps:m_sbPlayers){
        sf::Color tc=ps.team==Team::TERRORIST?sf::Color{220,80,80}:sf::Color{80,140,240};
        sf::Text nm=makeText(ps.name,14,tc); nm.setPosition({cx-320.f, py}); m_win.draw(nm);
        auto n=[&](int v,float x,sf::Color c){sf::Text t=makeText(std::to_string(v),14,c); t.setPosition({x, py}); m_win.draw(t);};
        n(ps.kills, cx+105.f,{190,230,190});
        n(ps.deaths,cx+175.f,{230,170,170});
        n(ps.health,cx+245.f,ps.alive?sf::Color{80,230,100}:sf::Color{110,110,110});
        py+=26.f;
    }
}

void Menu3D::drawMsg(){
    if(m_msgTimer<=0) return;
    auto sz=m_win.getSize();
    sf::Text t=makeText(m_msg,16,{255,230,100});
    t.setPosition({(float)sz.x/2.f-t.getLocalBounds().size.x/2.f, (float})sz.y-44.f);
    m_win.draw(t);
}

void Menu3D::showMsg(const std::string& m,float d){m_msg=m;m_msgTimer=d;}
void Menu3D::updateScoreboard(const std::vector<PlayerState>& ps,const GameInfo& gi){m_sbPlayers=ps;m_sbInfo=gi;}

} // namespace CS
