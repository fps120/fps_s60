#include "Map3D.h"
#include "glad/glad.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

namespace CS {

// ─── DESTRUCTOR ───────────────────────────────────────────────────────────────
Map3D::~Map3D() {
    if (m_worldVAO) { glDeleteVertexArrays(1,&m_worldVAO); glDeleteBuffers(1,&m_worldVBO); }
    if (m_propVAO)  { glDeleteVertexArrays(1,&m_propVAO);  glDeleteBuffers(1,&m_propVBO);  }
}

// ─── LOAD MAP ─────────────────────────────────────────────────────────────────
bool Map3D::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[Map3D] Cannot open: " << path << "\n"; return false; }

    m_worldVerts.clear(); m_propVerts.clear();
    m_wallAABBs.clear();  m_props.clear();
    m_spawns.clear();     m_lights.clear();

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;

        if (cmd == "MAP_NAME") {
            ss >> m_name;

        } else if (cmd == "ROOM") {
            // ROOM cx cy cz  sx sy sz  r g b
            float cx,cy,cz,sx,sy,sz,r,g,b;
            ss>>cx>>cy>>cz>>sx>>sy>>sz>>r>>g>>b;
            glm::vec3 mn{cx-sx/2,cy,cz-sz/2};
            glm::vec3 mx{cx+sx/2,cy+sy,cz+sz/2};
            // Rooms rendered from INSIDE (ceiling, floor, walls inward)
            addBox(m_worldVerts, mn, mx, {r,g,b}, true, nullptr);
            // No wall AABB for rooms (player is inside)

        } else if (cmd == "WALL") {
            // WALL x1 z1 x2 z2  height  yFloor  r g b
            float x1,z1,x2,z2,height,yf,r,g,b;
            ss>>x1>>z1>>x2>>z2>>height>>yf>>r>>g>>b;
            float thick = 0.2f;
            float dx=x2-x1, dz=z2-z1;
            float len=std::sqrtf(dx*dx+dz*dz);
            float nx=dz/len, nz=-dx/len;
            glm::vec3 mn{std::min(x1,x2)-thick*std::abs(nx),
                         yf,
                         std::min(z1,z2)-thick*std::abs(nz)};
            glm::vec3 mx{std::max(x1,x2)+thick*std::abs(nx),
                         yf+height,
                         std::max(z1,z2)+thick*std::abs(nz)};
            addBox(m_worldVerts, mn, mx, {r,g,b}, false, &m_wallAABBs);

        } else if (cmd == "SOLIDBOX") {
            // SOLIDBOX cx cy cz  sx sy sz  r g b
            float cx,cy,cz,sx,sy,sz,r,g,b;
            ss>>cx>>cy>>cz>>sx>>sy>>sz>>r>>g>>b;
            glm::vec3 mn{cx-sx/2,cy,cz-sz/2};
            glm::vec3 mx{cx+sx/2,cy+sy,cz+sz/2};
            addBox(m_worldVerts, mn, mx, {r,g,b}, false, &m_wallAABBs);

        } else if (cmd == "PROP") {
            // PROP type cx cy cz yaw
            std::string typeStr; float cx,cy,cz,yaw=0;
            ss>>typeStr>>cx>>cy>>cz>>yaw;
            Prop p;
            p.pos = {cx,cy,cz};
            p.yaw = yaw;
            if      (typeStr=="CRATE_S") { p.type=PropType::CRATE_SMALL; p.color={0.55f,0.40f,0.20f}; }
            else if (typeStr=="CRATE_L") { p.type=PropType::CRATE_LARGE; p.color={0.45f,0.32f,0.15f}; }
            else if (typeStr=="BARREL")  { p.type=PropType::BARREL;      p.color={0.25f,0.45f,0.25f}; }
            else                         { p.type=PropType::WALL_THIN;   p.color={0.50f,0.50f,0.50f}; }

            glm::vec3 hs;
            switch(p.type) {
                case PropType::CRATE_SMALL: hs={0.25f,0.25f,0.25f}; break;
                case PropType::CRATE_LARGE: hs={0.35f,0.60f,0.35f}; break;
                case PropType::BARREL:      hs={0.20f,0.45f,0.20f}; break;
                default:                    hs={0.05f,0.80f,0.50f}; break;
            }
            p.aabb = { glm::vec3(cx,cy,cz)-hs, glm::vec3(cx,cy,cz)+hs };
            p.aabb.min.y = cy;
            p.aabb.max.y = cy + hs.y*2;

            addBox(m_propVerts, p.aabb.min, p.aabb.max, p.color, false, &m_wallAABBs);
            m_props.push_back(p);

        } else if (cmd == "SPAWN") {
            // SPAWN T/CT x y z yaw
            std::string ts; float x,y,z,yaw=0;
            ss>>ts>>x>>y>>z>>yaw;
            Team t = (ts=="T") ? Team::TERRORIST : Team::CT;
            m_spawns.push_back({x,y,z,t,glm::radians(yaw)});

        } else if (cmd == "LIGHT") {
            // LIGHT x y z r g b radius
            float x,y,z,r,g,b,rad;
            ss>>x>>y>>z>>r>>g>>b>>rad;
            m_lights.push_back({{x,y,z},{r,g,b},rad});
        }
    }

    m_loaded = true;
    std::cout << "[Map3D] '" << m_name << "' loaded  "
              << m_worldVerts.size()/3 << " world tris  "
              << m_props.size() << " props\n";
    return true;
}

// ─── GPU UPLOAD ───────────────────────────────────────────────────────────────
void Map3D::uploadGPU() {
    if (!m_loaded || m_gpuReady) return;
    m_worldCount = (GLuint)m_worldVerts.size();
    m_propCount  = (GLuint)m_propVerts.size();
    uploadMesh(m_worldVerts, m_worldVAO);
    if (!m_propVerts.empty()) uploadMesh(m_propVerts, m_propVAO);
    m_gpuReady = true;
    std::cout << "[Map3D] GPU upload done\n";
}

GLuint Map3D::uploadMesh(const std::vector<Vertex3D>& verts, GLuint& vao) {
    GLuint vbo=0;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex3D), verts.data(), GL_STATIC_DRAW);
    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,pos));
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,normal));
    // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,color));
    glBindVertexArray(0);
    return vbo;
}

// ─── DRAW ─────────────────────────────────────────────────────────────────────
void Map3D::drawWorld() const {
    if (!m_gpuReady) return;
    glBindVertexArray(m_worldVAO);
    glDrawArrays(GL_TRIANGLES,0,m_worldCount);
    glBindVertexArray(0);
}

void Map3D::drawProps() const {
    if (!m_gpuReady || m_propCount==0) return;
    glBindVertexArray(m_propVAO);
    glDrawArrays(GL_TRIANGLES,0,m_propCount);
    glBindVertexArray(0);
}

std::vector<AABB> Map3D::allSolidAABBs() const {
    return m_wallAABBs;
}

// ─── GEOMETRY HELPERS ─────────────────────────────────────────────────────────
void Map3D::addQuad(std::vector<Vertex3D>& verts,
                    glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                    glm::vec3 normal, glm::vec3 color)
{
    // Two triangles: ABC + ACD
    verts.push_back({a,normal,color});
    verts.push_back({b,normal,color});
    verts.push_back({c,normal,color});
    verts.push_back({a,normal,color});
    verts.push_back({c,normal,color});
    verts.push_back({d,normal,color});
}

void Map3D::addBox(std::vector<Vertex3D>& verts,
                   glm::vec3 mn, glm::vec3 mx,
                   glm::vec3 color,
                   bool inward,
                   std::vector<AABB>* aabbs)
{
    if (aabbs) aabbs->push_back({mn,mx});
    float s = inward ? -1.f : 1.f;

    glm::vec3 p000=mn, p100={mx.x,mn.y,mn.z}, p110={mx.x,mn.y,mx.z}, p010={mn.x,mn.y,mx.z};
    glm::vec3 p001={mn.x,mx.y,mn.z}, p101={mx.x,mx.y,mn.z}, p111=mx, p011={mn.x,mx.y,mx.z};

    // Bottom (floor)  normal = -Y outward, +Y inward
    addQuad(verts, p000,p100,p110,p010, glm::vec3(0, -s, 0), color*0.8f);
    // Top (ceiling)
    addQuad(verts, p001,p011,p111,p101, glm::vec3(0,  s, 0), color*0.9f);
    // Front (-Z)
    addQuad(verts, p000,p001,p101,p100, glm::vec3(0, 0, -s), color);
    // Back  (+Z)
    addQuad(verts, p110,p111,p011,p010, glm::vec3(0, 0,  s), color);
    // Left  (-X)
    addQuad(verts, p010,p011,p001,p000, glm::vec3(-s,0,  0), color*0.95f);
    // Right (+X)
    addQuad(verts, p100,p101,p111,p110, glm::vec3( s,0,  0), color*0.95f);
}

} // namespace CS
