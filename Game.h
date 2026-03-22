#pragma once
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <SFML/Graphics.hpp>
#include "GameTypes.h"
#include "Camera.h"
#include "Map3D.h"
#include "Player3D.h"
#include "Renderer3D.h"
#include "WeaponModel.h"
#include "HUD3D.h"
#include "Menu3D.h"
#include "NetworkClient.h"
#include <unordered_map>
#include <memory>
namespace CS{
enum class Phase{MENU,CONNECTING,PLAYING};
class Game{
public:
    Game();~Game();int run();
private:
    GLFWwindow* m_glfw=nullptr;
    sf::RenderWindow m_sfmlWin;
    sf::Font m_font;
    int m_winW=1280,m_winH=720;
    std::unique_ptr<Renderer3D> m_renderer;
    std::unique_ptr<Map3D> m_map;
    std::unique_ptr<Menu3D> m_menu;
    std::unique_ptr<HUD3D> m_hud;
    std::unique_ptr<NetworkClient> m_net;
    Phase m_phase=Phase::MENU;
    Camera m_cam;
    Player3D* m_local=nullptr;
    WeaponAnimState m_weapAnim;
    GameInfo m_info;
    std::unordered_map<uint8_t,Player3D> m_players;
    InputState m_input;
    double m_lastMouseX=0,m_lastMouseY=0;
    bool m_mouseCaptured=false,m_scopeMode=false,m_tabDown=false;
    double m_lastTime=0;
    void handleGLFWEvents();
    void handleSFMLEvents();
    void update(float dt);
    void render();
    void updatePlaying(float dt);
    void processNetEvents();
    bool initGLFW();
    bool initSFML();
    bool loadResources();
    void captureMouse(bool capture);
    void connectToServer(const ConnectInfo& ci);
    std::vector<Player3D> remotePlayerList()const;
    static void cbResize(GLFWwindow*,int w,int h);
    static void cbKey(GLFWwindow*,int key,int,int action,int);
    static void cbMouseMove(GLFWwindow*,double x,double y);
    static Game* s_instance;
};}
