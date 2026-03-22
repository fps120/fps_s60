#include "Renderer3D.h"
#include <GL/gl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace CS {

// ─── SHADER PROGRAM ───────────────────────────────────────────────────────────
std::string ShaderProgram::readFile(const std::string& p) {
    std::ifstream f(p);
    if (!f) { std::cerr << "[Shader] Cannot open: " << p << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

GLuint ShaderProgram::compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s,1,&c,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s,512,nullptr,log);
        std::cerr << "[Shader] Compile error:\n" << log << "\n";
    }
    return s;
}

bool ShaderProgram::load(const std::string& vp, const std::string& fp) {
    auto vs = compileShader(GL_VERTEX_SHADER,   readFile(vp));
    auto fs = compileShader(GL_FRAGMENT_SHADER, readFile(fp));
    id = glCreateProgram();
    glAttachShader(id,vs); glAttachShader(id,fs);
    glLinkProgram(id);
    GLint ok; glGetProgramiv(id,GL_LINK_STATUS,&ok);
    if (!ok) { char log[512]; glGetProgramInfoLog(id,512,nullptr,log); std::cerr<<"[Shader] Link:\n"<<log<<"\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return ok;
}

void ShaderProgram::setMat4(const char* n, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id,n),1,GL_FALSE,glm::value_ptr(m));
}
void ShaderProgram::setVec3(const char* n, const glm::vec3& v) const {
    glUniform3f(glGetUniformLocation(id,n),v.x,v.y,v.z);
}
void ShaderProgram::setFloat(const char* n, float v) const {
    glUniform1f(glGetUniformLocation(id,n),v);
}
void ShaderProgram::setInt(const char* n, int v) const {
    glUniform1i(glGetUniformLocation(id,n),v);
}

// ─── INIT ─────────────────────────────────────────────────────────────────────
bool Renderer3D::init(int w, int h) {
    width=w; height=h;

    if (!m_worldShader.load("shaders/world.vert",  "shaders/world.frag"))  return false;
    if (!m_weaponShader.load("shaders/weapon.vert", "shaders/weapon.frag")) return false;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);

    buildCapsule();
    buildWeaponMeshes();

    // Tracer VAO (dynamic, updated each frame)
    glGenVertexArrays(1,&m_tracerVAO);
    glGenBuffers(1,&m_tracerVBO);
    glBindVertexArray(m_tracerVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_tracerVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TRACERS*2*sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),(void*)0);
    glBindVertexArray(0);

    std::cout << "[Renderer3D] Initialized " << w << "x" << h << "\n";
    return true;
}

void Renderer3D::resize(int w, int h) {
    width=w; height=h;
    glViewport(0,0,w,h);
}

void Renderer3D::shutdown() {
    if (m_capsuleVAO) { glDeleteVertexArrays(1,&m_capsuleVAO); glDeleteBuffers(1,&m_capsuleVBO); }
    for (auto& wm : m_weaponMeshes)
        if (wm.vao) { glDeleteVertexArrays(1,&wm.vao); glDeleteBuffers(1,&wm.vbo); }
    if (m_tracerVAO) { glDeleteVertexArrays(1,&m_tracerVAO); glDeleteBuffers(1,&m_tracerVBO); }
}

// ─── FRAME ────────────────────────────────────────────────────────────────────
void Renderer3D::beginFrame() {
    glViewport(0,0,width,height);
    glClearColor(m_fogColor.r, m_fogColor.g, m_fogColor.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer3D::endFrame() {}

// ─── RENDER WORLD ─────────────────────────────────────────────────────────────
void Renderer3D::renderWorld(const Map3D& map, const Camera& cam,
                              int winW, int winH)
{
    if (!map.isLoaded()) return;
    float aspect = (float)winW / (float)winH;

    m_worldShader.use();
    m_worldShader.setMat4("uModel", glm::mat4(1.f));
    m_worldShader.setMat4("uView",  cam.viewMatrix());
    m_worldShader.setMat4("uProj",  cam.projMatrix(aspect));
    m_worldShader.setVec3("uLightDir",  m_sunDir);
    m_worldShader.setVec3("uLightColor",m_sunColor);
    m_worldShader.setVec3("uAmbient",   m_ambient);
    m_worldShader.setVec3("uCamPos",    cam.pos);
    m_worldShader.setVec3("uFogColor",  m_fogColor);
    m_worldShader.setFloat("uFogStart", m_fogStart);
    m_worldShader.setFloat("uFogEnd",   m_fogEnd);

    // Disable face culling for inward-facing rooms
    glDisable(GL_CULL_FACE);
    map.drawWorld();
    glEnable(GL_CULL_FACE);
    map.drawProps();

    m_worldShader.stop();
}

// ─── RENDER REMOTE PLAYERS (capsule proxies) ──────────────────────────────────
void Renderer3D::renderRemotePlayers(const std::vector<Player3D>& players,
                                      const Camera& cam, int winW, int winH)
{
    if (m_capsuleVAO == 0) return;
    float aspect = (float)winW / (float)winH;

    m_worldShader.use();
    m_worldShader.setMat4("uView", cam.viewMatrix());
    m_worldShader.setMat4("uProj", cam.projMatrix(aspect));
    m_worldShader.setVec3("uLightDir",  m_sunDir);
    m_worldShader.setVec3("uLightColor",m_sunColor);
    m_worldShader.setVec3("uAmbient",   m_ambient);
    m_worldShader.setVec3("uCamPos",    cam.pos);
    m_worldShader.setVec3("uFogColor",  m_fogColor);
    m_worldShader.setFloat("uFogStart", m_fogStart);
    m_worldShader.setFloat("uFogEnd",   m_fogEnd);

    glBindVertexArray(m_capsuleVAO);
    for (auto& p : players) {
        if (!p.isAlive()) continue;
        const auto& st = p.state();

        glm::mat4 model = glm::translate(glm::mat4(1.f), p.feetPos());
        model = glm::rotate(model, -st.yaw, glm::vec3(0,1,0));

        // T=red, CT=blue
        glm::vec3 col = (st.team==Team::TERRORIST)
            ? glm::vec3(0.8f,0.15f,0.15f)
            : glm::vec3(0.15f,0.35f,0.85f);

        m_worldShader.setMat4("uModel", model);
        // Tint via ambient override trick: set high ambient for player colour
        m_worldShader.setVec3("uAmbient", col * 0.5f);

        glDrawArrays(GL_TRIANGLES,0,m_capsuleCount);
    }
    m_worldShader.setVec3("uAmbient", m_ambient); // restore
    glBindVertexArray(0);
    m_worldShader.stop();
}

// ─── RENDER WEAPON MODEL ──────────────────────────────────────────────────────
void Renderer3D::renderWeaponModel(WeaponID weapon,
                                    bool reloading, float reloadT, float fireT,
                                    float bobT,
                                    const Camera& cam, int winW, int winH)
{
    int slot = (int)weapon;
    if (slot < 0 || slot >= 6) return;
    auto& wm = m_weaponMeshes[slot];
    if (wm.vao == 0) return;

    // Clear depth so weapon never clips through walls
    glClear(GL_DEPTH_BUFFER_BIT);

    float aspect = (float)winW / (float)winH;

    // Weapon uses a fixed view (identity view for viewspace rendering)
    glm::mat4 weapView = glm::mat4(1.f);

    // Bob + recoil animation
    float recoilZ = (fireT > 0) ? sinf(fireT * 20.f) * 0.04f : 0.f;
    float reloadY = reloading ? -sinf((1.f - reloadT) * glm::pi<float>()) * 0.12f : 0.f;
    float bobX    = sinf(bobT * 2.0f) * 0.006f;
    float bobY    = cosf(bobT * 4.0f) * 0.003f;

    // Position weapon in view space (right side, slightly down)
    glm::mat4 model = glm::translate(glm::mat4(1.f),
                       glm::vec3(0.14f + bobX, -0.12f + reloadY + bobY, -0.3f + recoilZ));
    model = glm::rotate(model, glm::radians(-5.f), glm::vec3(1,0,0));

    m_weaponShader.use();
    m_weaponShader.setMat4("uModel", model);
    m_weaponShader.setMat4("uView",  weapView);
    m_weaponShader.setMat4("uProj",  cam.weaponProjMatrix(aspect));
    m_weaponShader.setVec3("uLightDir", m_sunDir);

    glBindVertexArray(wm.vao);
    glDrawArrays(GL_TRIANGLES, 0, wm.count);
    glBindVertexArray(0);
    m_weaponShader.stop();
}

// ─── TRACERS ──────────────────────────────────────────────────────────────────
void Renderer3D::addTracer(const BulletEvent& evt) {
    if (m_tracers.size() >= MAX_TRACERS) m_tracers.erase(m_tracers.begin());
    Tracer3D t;
    t.start   = {evt.ox, evt.oy, evt.oz};
    t.end     = {evt.ox+evt.dx*evt.dist, evt.oy+evt.dy*evt.dist, evt.oz+evt.dz*evt.dist};
    t.weapon  = evt.weapon;
    t.hit     = evt.hit;
    float maxLife = (evt.weapon==WeaponID::AWP) ? 0.5f : 0.2f;
    t.maxLife = maxLife;
    t.lifetime= maxLife;
    m_tracers.push_back(t);

    // Hit spark
    if (evt.hit) {
        std::mt19937 rng{42};
        std::uniform_real_distribution<float> d(-1.f,1.f);
        for (int i=0;i<5;i++) {
            Spark s;
            s.pos   = t.end;
            s.vel   = glm::normalize(glm::vec3(d(rng),d(rng),d(rng)))*1.5f;
            s.life  = 0.3f;
            s.blood = evt.hit;
            m_sparks.push_back(s);
        }
    }
}

void Renderer3D::updateTracers(float dt) {
    for (auto& t : m_tracers) t.lifetime -= dt;
    m_tracers.erase(
        std::remove_if(m_tracers.begin(),m_tracers.end(),
            [](auto& t){return t.lifetime<=0;}),
        m_tracers.end());
    for (auto& s : m_sparks) { s.life-=dt; s.pos+=s.vel*dt; s.vel.y-=9.f*dt; }
    m_sparks.erase(
        std::remove_if(m_sparks.begin(),m_sparks.end(),
            [](auto& s){return s.life<=0;}),
        m_sparks.end());
}

void Renderer3D::renderTracers(const Camera& cam, int winW, int winH) {
    if (m_tracers.empty()) return;
    float aspect = (float)winW/(float)winH;

    // Build line buffer: each tracer = 2 vec3 points
    std::vector<glm::vec3> pts;
    pts.reserve(m_tracers.size()*2);
    for (auto& t : m_tracers) {
        pts.push_back(t.start);
        pts.push_back(t.end);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_tracerVBO);
    glBufferSubData(GL_ARRAY_BUFFER,0,pts.size()*sizeof(glm::vec3),pts.data());

    m_worldShader.use();
    m_worldShader.setMat4("uModel", glm::mat4(1.f));
    m_worldShader.setMat4("uView",  cam.viewMatrix());
    m_worldShader.setMat4("uProj",  cam.projMatrix(aspect));
    // White-ish tracers - max out ambient, zero diffuse
    m_worldShader.setVec3("uAmbient",    glm::vec3(1.f));
    m_worldShader.setVec3("uLightColor", glm::vec3(0.f));
    m_worldShader.setVec3("uFogColor",   m_fogColor);
    m_worldShader.setFloat("uFogStart",  m_fogStart);
    m_worldShader.setFloat("uFogEnd",    m_fogEnd);
    m_worldShader.setVec3("uCamPos",     cam.pos);

    glLineWidth(2.f);
    glBindVertexArray(m_tracerVAO);
    glDrawArrays(GL_LINES, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
    glLineWidth(1.f);
    m_worldShader.stop();
}

// ─── BUILD CAPSULE (remote player proxy) ─────────────────────────────────────
void Renderer3D::buildCapsule() {
    // Simple 8-sided cylinder as player proxy (0.6m wide, 1.75m tall)
    const int   SEGS = 8;
    const float R    = PLAYER_RADIUS;
    const float H    = PLAYER_HEIGHT;
    std::vector<Vertex3D> verts;

    for (int i=0;i<SEGS;i++) {
        float a0 = i     * glm::two_pi<float>() / SEGS;
        float a1 = (i+1) * glm::two_pi<float>() / SEGS;
        float x0=R*cosf(a0), z0=R*sinf(a0);
        float x1=R*cosf(a1), z1=R*sinf(a1);

        glm::vec3 n0=glm::normalize(glm::vec3(x0,0,z0));
        glm::vec3 n1=glm::normalize(glm::vec3(x1,0,z1));
        glm::vec3 col{0.6f,0.6f,0.6f};

        // Side quad (2 triangles)
        glm::vec3 bl{x0,0,z0}, br{x1,0,z1}, tr{x1,H,z1}, tl{x0,H,z0};
        verts.push_back({bl,n0,col}); verts.push_back({br,n1,col}); verts.push_back({tr,n1,col});
        verts.push_back({bl,n0,col}); verts.push_back({tr,n1,col}); verts.push_back({tl,n0,col});

        // Top cap triangle
        glm::vec3 top{0,H,0}; glm::vec3 nu{0,1,0};
        verts.push_back({{0,H,0},nu,col}); verts.push_back({tl,nu,col}); verts.push_back({tr,nu,col});
    }

    m_capsuleCount = (GLuint)verts.size();
    glGenVertexArrays(1,&m_capsuleVAO);
    glGenBuffers(1,&m_capsuleVBO);
    glBindVertexArray(m_capsuleVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_capsuleVBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(Vertex3D),verts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,color));
    glBindVertexArray(0);
}

// ─── BUILD WEAPON MESHES (procedural box models) ─────────────────────────────
void Renderer3D::buildWeaponMeshes() {
    for (int i=0;i<6;i++) buildWeaponMesh(i, (WeaponID)i);
}

void Renderer3D::buildWeaponMesh(int slot, WeaponID id) {
    const auto& wd = WEAPONS[(int)id];
    std::vector<Vertex3D> verts;

    auto box=[&](glm::vec3 mn, glm::vec3 mx, glm::vec3 col){
        // 6 faces × 2 triangles × 3 verts = 36
        auto quad=[&](glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d,glm::vec3 n){
            verts.push_back({a,n,col}); verts.push_back({b,n,col}); verts.push_back({c,n,col});
            verts.push_back({a,n,col}); verts.push_back({c,n,col}); verts.push_back({d,n,col});
        };
        quad({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},{mn.x,mn.y,mx.z},{0,-1,0});
        quad({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z},{0, 1,0});
        quad({mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mn.y,mn.z},{0,0,-1});
        quad({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},{0,0, 1});
        quad({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mx.y,mn.z},{-1,0,0});
        quad({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mx.x,mn.y,mx.z},{ 1,0,0});
    };

    glm::vec3 bc = wd.bodyCol, sc = wd.slideCol;

    switch(id) {
    case WeaponID::KNIFE:
        box({-0.01f,-0.01f,-0.15f},{0.01f,0.01f,0.05f}, bc);  // handle
        box({-0.004f,-0.003f,-0.30f},{0.004f,0.003f,-0.15f}, sc);  // blade
        break;
    case WeaponID::GLOCK:
        box({-0.025f,-0.07f,-0.18f},{0.025f,0.025f,0.02f}, bc); // body
        box({-0.015f,0.015f,-0.17f},{0.015f,0.025f,0.02f}, sc); // slide
        box({-0.006f,-0.07f,-0.05f},{0.006f,-0.01f,0.0f},  bc); // grip extend
        break;
    case WeaponID::MP5:
        box({-0.03f,-0.04f,-0.35f},{0.03f,0.06f,0.06f},  bc);
        box({-0.02f,0.03f,-0.34f}, {0.02f,0.06f,0.05f},  sc);
        box({-0.02f,-0.04f,-0.42f},{0.02f,-0.01f,-0.35f},sc*0.8f); // barrel ext
        break;
    case WeaponID::AK47:
        box({-0.03f,-0.04f,-0.42f},{0.03f,0.06f,0.05f},  bc);
        box({-0.018f,0.04f,-0.40f},{0.018f,0.06f,0.04f}, sc);
        box({-0.016f,-0.12f,-0.15f},{0.016f,-0.04f,0.0f},bc*0.9f); // mag
        box({-0.01f,-0.04f,-0.52f},{0.01f,0.01f,-0.42f}, bc*0.7f); // barrel
        break;
    case WeaponID::AWP:
        box({-0.03f,-0.05f,-0.55f},{0.03f,0.07f,0.08f},  bc);
        box({-0.018f,0.04f,-0.53f},{0.018f,0.07f,0.07f}, sc);
        box({-0.01f,-0.05f,-0.70f},{0.01f,0.015f,-0.55f},bc*0.7f); // barrel
        box({-0.025f,-0.05f,-0.35f},{0.025f,0.035f,-0.20f},sc*1.2f); // scope box
        break;
    case WeaponID::M3:
        box({-0.04f,-0.06f,-0.40f},{0.04f,0.07f,0.07f},  bc);
        box({-0.025f,0.04f,-0.38f},{0.025f,0.07f,0.06f}, sc);
        box({-0.015f,-0.06f,-0.48f},{0.015f,0.01f,-0.40f},bc*0.8f);
        break;
    }

    auto& wm = m_weaponMeshes[slot];
    wm.count = (GLuint)verts.size();
    glGenVertexArrays(1,&wm.vao);
    glGenBuffers(1,&wm.vbo);
    glBindVertexArray(wm.vao);
    glBindBuffer(GL_ARRAY_BUFFER,wm.vbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(Vertex3D),verts.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,color));
    glBindVertexArray(0);
}

} // namespace CS
