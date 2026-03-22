#pragma once
#include "glad/glad.h"
#include "GameTypes.h"
#include "Camera.h"
#include "Map3D.h"
#include "Player3D.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
namespace CS{
class ShaderProgram{
public:
    GLuint id=0;
    bool load(const std::string& v,const std::string& f);
    void use()const{glUseProgram(id);}
    void stop()const{glUseProgram(0);}
    void setMat4(const char* n,const glm::mat4& m)const;
    void setVec3(const char* n,const glm::vec3& v)const;
    void setFloat(const char* n,float v)const;
    void setInt(const char* n,int v)const;
private:
    static GLuint compileShader(GLenum t,const std::string& s);
    static std::string readFile(const std::string& p);
};
struct Tracer3D{glm::vec3 start,end;float lifetime,maxLife;WeaponID weapon;bool hit;};
class Renderer3D{
public:
    bool init(int w,int h);
    void resize(int w,int h);
    void shutdown();
    void beginFrame();
    void renderWorld(const Map3D& map,const Camera& cam,int w,int h);
    void renderRemotePlayers(const std::vector<Player3D>& players,const Camera& cam,int w,int h);
    void renderWeaponModel(WeaponID weapon,bool reloading,float reloadT,float fireT,float bobT,const Camera& cam,int w,int h);
    void renderTracers(const Camera& cam,int w,int h);
    void endFrame();
    void addTracer(const BulletEvent& evt);
    void updateTracers(float dt);
    int width=0,height=0;
private:
    ShaderProgram m_worldShader,m_weaponShader;
    GLuint m_capsuleVAO=0,m_capsuleVBO=0,m_capsuleCount=0;
    struct WeaponMesh{GLuint vao=0,vbo=0,count=0;};
    WeaponMesh m_weaponMeshes[6];
    GLuint m_tracerVAO=0,m_tracerVBO=0;
    std::vector<Tracer3D> m_tracers;
    static constexpr size_t MAX_TRACERS=256;
    void buildCapsule();
    void buildWeaponMeshes();
    void buildWeaponMesh(int slot,WeaponID id);
    glm::vec3 m_sunDir=glm::normalize(glm::vec3(0.4f,0.8f,0.3f));
    glm::vec3 m_sunColor={1.0f,0.95f,0.85f};
    glm::vec3 m_ambient={0.35f,0.35f,0.40f};
    glm::vec3 m_fogColor={0.55f,0.55f,0.55f};
    float m_fogStart=40.f,m_fogEnd=120.f;
};}
