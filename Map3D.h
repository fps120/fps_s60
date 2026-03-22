#pragma once
#include "GameTypes.h"
#include <vector>
#include <string>
typedef unsigned int GLuint;
namespace CS{
enum class PropType:uint8_t{CRATE_SMALL=0,CRATE_LARGE=1,BARREL=2,WALL_THIN=3};
struct Prop{PropType type;glm::vec3 pos;float yaw=0;AABB aabb;glm::vec3 color;};
struct Light{glm::vec3 pos,color;float radius;};
struct Vertex3D{glm::vec3 pos,normal,color;};
class Map3D{
public:
    ~Map3D();
    bool load(const std::string& path);
    bool isLoaded()const{return m_loaded;}
    void uploadGPU();
    void drawWorld()const;
    void drawProps()const;
    const std::vector<AABB>& getWallAABBs()const{return m_wallAABBs;}
    const std::vector<Prop>& getProps()const{return m_props;}
    const std::vector<SpawnPoint>& getSpawns()const{return m_spawns;}
    const std::vector<Light>& getLights()const{return m_lights;}
    const std::string& getName()const{return m_name;}
    std::vector<AABB> allSolidAABBs()const;
private:
    std::string m_name;bool m_loaded=false;
    std::vector<Vertex3D> m_worldVerts,m_propVerts;
    std::vector<AABB> m_wallAABBs;
    std::vector<Prop> m_props;
    std::vector<SpawnPoint> m_spawns;
    std::vector<Light> m_lights;
    GLuint m_worldVAO=0,m_worldVBO=0,m_worldCount=0;
    GLuint m_propVAO=0,m_propVBO=0,m_propCount=0;
    bool m_gpuReady=false;
    void addBox(std::vector<Vertex3D>& v,glm::vec3 mn,glm::vec3 mx,glm::vec3 col,bool inward=false,std::vector<AABB>* a=nullptr);
    void addQuad(std::vector<Vertex3D>& v,glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d,glm::vec3 n,glm::vec3 col);
    GLuint uploadMesh(const std::vector<Vertex3D>& v,GLuint& vao);
};}
