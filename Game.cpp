#include "Game.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace CS {

Game* Game::s_instance = nullptr;

// ─── CONSTRUCTOR ──────────────────────────────────────────────────────────────
Game::Game() {
    s_instance = this;
    m_renderer = std::make_unique<Renderer3D>();
    m_map      = std::make_unique<Map3D>();
    m_net      = std::make_unique<NetworkClient>();
}

Game::~Game() {
    if (m_net) m_net->disconnect();
    if (m_renderer) m_renderer->shutdown();
    if (m_glfw) { glfwDestroyWindow(m_glfw); glfwTerminate(); }
}

// ─── RUN ──────────────────────────────────────────────────────────────────────
int Game::run() {
    if (!initGLFW()) return 1;
    if (!initSFML()) return 1;
    if (!loadResources()) return 1;

    m_menu = std::make_unique<Menu3D>(m_sfmlWin, m_font);
    m_hud  = std::make_unique<HUD3D>(m_font);

    m_lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(m_glfw)) {
        double now = glfwGetTime();
        float  dt  = std::min((float)(now - m_lastTime), 0.05f);
        m_lastTime = now;

        handleGLFWEvents();
        handleSFMLEvents();
        update(dt);
        render();
    }
    return 0;
}

// ─── INIT GLFW (OpenGL window) ────────────────────────────────────────────────
bool Game::initGLFW() {
    if (!glfwInit()) { std::cerr << "[GLFW] Init failed\n"; return false; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA 4×

    m_glfw = glfwCreateWindow(m_winW, m_winH, "CS 1.6 3D Clone", nullptr, nullptr);
    if (!m_glfw) { std::cerr << "[GLFW] Window failed\n"; glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_glfw);
    glfwSwapInterval(1);  // VSync

    // Set callbacks
    glfwSetWindowSizeCallback(m_glfw, cbResize);
    glfwSetKeyCallback(m_glfw, cbKey);
    glfwSetCursorPosCallback(m_glfw, cbMouseMove);

    if (!gladLoadGL()) { std::cerr << "[GLAD] Failed to load OpenGL\n"; return false; }

    if (!m_renderer->init(m_winW, m_winH)) return false;
    std::cout << "[Game] OpenGL " << glGetString(GL_VERSION) << "\n";
    return true;
}

// ─── INIT SFML (overlay window — invisible, shares input via GLFW) ────────────
bool Game::initSFML() {
    // SFML used only as a render target for HUD/menus — we draw to a texture
    // then blit it over the GLFW window using a full-screen quad.
    // Simpler: create a hidden SFML window of same size for text rendering.
    m_sfmlWin.create(sf::VideoMode({(unsigned)m_winW, (unsigned)m_winH}), "cs_hud",
                     sf::Style::None);  // borderless, hidden
    m_sfmlWin.setVisible(false);
    return true;
}

bool Game::loadResources() {
    if (!m_font.openFromFile("assets/fonts/mono.ttf")) {
        if (!m_font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")) {
            std::cerr << "[Game] Warning: font not found\n";
        }
    }
    return true;
}

// ─── HANDLE GLFW EVENTS ───────────────────────────────────────────────────────
void Game::handleGLFWEvents() {
    glfwPollEvents();

    if (m_phase != Phase::PLAYING) return;

    // WASD
    m_input.fwd   = glfwGetKey(m_glfw, GLFW_KEY_W) == GLFW_PRESS;
    m_input.back  = glfwGetKey(m_glfw, GLFW_KEY_S) == GLFW_PRESS;
    m_input.left  = glfwGetKey(m_glfw, GLFW_KEY_A) == GLFW_PRESS;
    m_input.right = glfwGetKey(m_glfw, GLFW_KEY_D) == GLFW_PRESS;
    m_input.jump  = glfwGetKey(m_glfw, GLFW_KEY_SPACE) == GLFW_PRESS;
    m_input.crouch= glfwGetKey(m_glfw, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    m_input.reload= glfwGetKey(m_glfw, GLFW_KEY_R) == GLFW_PRESS;

    // Weapon slots
    static const int keys[] = {GLFW_KEY_1,GLFW_KEY_2,GLFW_KEY_3,
                                GLFW_KEY_4,GLFW_KEY_5,GLFW_KEY_6};
    for (int i=0;i<6;i++)
        if (glfwGetKey(m_glfw,keys[i])==GLFW_PRESS) m_input.slot=(uint8_t)i;

    // Mouse shoot
    m_input.shoot = glfwGetMouseButton(m_glfw, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Scope (right mouse)
    m_scopeMode = (m_local && m_local->state().weapon == WeaponID::AWP)
               && (glfwGetMouseButton(m_glfw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    // Aim angles from camera
    m_input.yaw   = m_cam.yaw;
    m_input.pitch = m_cam.pitch;

    // Tab scoreboard
    bool tabNow = glfwGetKey(m_glfw, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tabNow && !m_tabDown) {
        m_menu->setState(MenuState::SCOREBOARD);
    } else if (!tabNow && m_tabDown) {
        m_menu->setState(MenuState::MAIN);
    }
    m_tabDown = tabNow;
}

// ─── HANDLE SFML EVENTS ───────────────────────────────────────────────────────
void Game::handleSFMLEvents() {
    while (m_sfmlWin.pollEvent()) {}

    // Route keyboard to menu
    if (m_phase == Phase::MENU || m_phase == Phase::CONNECTING) {
        // Synthesise SFML events from GLFW key state for menu text input
        // (Menu3D handles sf::Event::TextEntered — we poll GLFW chars separately)
    }

    if (m_phase == Phase::PLAYING && glfwGetKey(m_glfw, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        static bool escDown = false;
        if (!escDown) {
            captureMouse(false);
            m_net->disconnect();
            m_phase = Phase::MENU;
            m_players.clear();
            m_local = nullptr;
            m_menu->setState(MenuState::MAIN);
        }
        escDown = true;
    } else {
        static bool escDown = false;
        escDown = false;
    }
}

// ─── UPDATE ───────────────────────────────────────────────────────────────────
void Game::update(float dt) {
    switch (m_phase) {
    case Phase::MENU:
    case Phase::CONNECTING:
        m_menu->update(dt);
        if (m_menu->readyToConnect) {
            m_menu->readyToConnect = false;
            connectToServer(m_menu->getConn());
        }
        m_net->update();
        processNetEvents();
        break;
    case Phase::PLAYING:
        updatePlaying(dt);
        break;
    }
}

// ─── UPDATE PLAYING ───────────────────────────────────────────────────────────
void Game::updatePlaying(float dt) {
    m_net->update();
    processNetEvents();

    if (!m_local) return;

    auto solids = m_map->allSolidAABBs();
    auto others = remotePlayerList();

    // Local player physics
    m_local->handleInput(m_input, solids, dt);

    // Sync camera to eye position
    m_cam.pos   = m_local->eyePos();
    m_cam.yaw   = m_local->state().yaw;
    m_cam.pitch = m_local->state().pitch;
    m_cam.fovY  = m_scopeMode ? 20.f : m_menu->getSettings().fov;

    // Shooting (client-side prediction)
    if (m_input.shoot) {
        BulletEvent evt;
        if (m_local->tryShoot(m_cam, solids, others, evt)) {
            m_renderer->addTracer(evt);
            m_weapAnim.onShoot();
        }
    }

    // Weapon animation state
    m_weapAnim.update(m_local->state(), dt,
        std::sqrtf(m_input.fwd||m_input.back||m_input.left||m_input.right ? 5.f : 0.f));

    m_renderer->updateTracers(dt);
    m_hud->update(dt);

    // Send input to server every frame
    m_net->sendInput(m_input);

    // Scoreboard
    if (m_menu->getState() == MenuState::SCOREBOARD) {
        std::vector<PlayerState> states;
        for (auto& [id,p] : m_players) states.push_back(p.state());
        m_menu->updateScoreboard(states, m_info);
    }
}

// ─── PROCESS NETWORK EVENTS ───────────────────────────────────────────────────
void Game::processNetEvents() {
    NetEvent ev;
    while (m_net->pollEvent(ev)) {
        switch (ev.type) {

        case NetEvent::Type::CONNECTED:
            std::cout << "[Game] Connected (id=" << (int)m_net->myId() << ")\n";
            m_net->sendTeam(m_menu->getTeam());
            m_phase = Phase::PLAYING;
            m_map->load("maps/de_dust2_3d.map");
            m_map->uploadGPU();
            captureMouse(true);
            break;

        case NetEvent::Type::DISCONNECTED:
            m_hud->showMsg("DISCONNECTED");
            m_phase = Phase::MENU;
            m_players.clear(); m_local = nullptr;
            captureMouse(false);
            break;

        case NetEvent::Type::STATE_UPDATE: {
            m_info = ev.info;
            for (auto& ps : ev.players) {
                auto it = m_players.find(ps.id);
                if (it == m_players.end()) {
                    Player3D p(ps);
                    if (ps.id == m_net->myId()) {
                        p.setLocal(true);
                        m_players[ps.id] = std::move(p);
                        m_local = &m_players[ps.id];
                        if (m_map->isLoaded() && !m_map->getSpawns().empty()) {
                            auto sp = m_map->getSpawns()[0];
                            for (auto& s : m_map->getSpawns())
                                if (s.team == ps.team) { sp=s; break; }
                            m_local->respawn(sp);
                        }
                    } else {
                        m_players[ps.id] = std::move(p);
                    }
                } else {
                    if (ps.id == m_net->myId()) it->second.reconcile(ps);
                    else                        it->second.setState(ps);
                }
            }
            break;
        }

        case NetEvent::Type::BULLET:
            // Show tracers from other players
            if (ev.bullet.shooterId != m_net->myId())
                m_renderer->addTracer(ev.bullet);
            break;

        case NetEvent::Type::HIT:
            if (ev.playerId == m_net->myId())
                m_hud->showHit(ev.headshot);
            break;

        case NetEvent::Type::KILL:
            m_hud->pushKill("Player", ev.text, ev.weapon, ev.headshot);
            break;

        case NetEvent::Type::ROUND_START:
            m_info = ev.info;
            m_hud->showMsg("ROUND " + std::to_string(m_info.round) + " START");
            if (m_local && m_map->isLoaded()) {
                for (auto& s : m_map->getSpawns())
                    if (s.team == m_local->getTeam()) { m_local->respawn(s); break; }
            }
            break;

        case NetEvent::Type::ROUND_END:
            m_hud->showMsg(ev.winTeam==Team::TERRORIST ? "TERRORISTS WIN!" : "COUNTER-TERRORISTS WIN!");
            break;

        case NetEvent::Type::CHAT:
            m_hud->pushChat(ev.text);
            break;
        }
    }
}

// ─── RENDER ───────────────────────────────────────────────────────────────────
void Game::render() {
    if (m_phase == Phase::MENU || m_phase == Phase::CONNECTING) {
        // Clear OpenGL, draw SFML menu
        glClearColor(0.05f,0.06f,0.09f,1.f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(m_glfw);

        m_sfmlWin.setVisible(true);
        m_sfmlWin.clear({8,10,16});
        m_menu->draw();
        m_sfmlWin.display();
    } else {
        // 3D render
        m_sfmlWin.setVisible(false);

        m_renderer->beginFrame();
        if (m_map && m_map->isLoaded()) {
            m_renderer->renderWorld(*m_map, m_cam, m_winW, m_winH);

            auto others = remotePlayerList();
            m_renderer->renderRemotePlayers(others, m_cam, m_winW, m_winH);
        }
        m_renderer->renderTracers(m_cam, m_winW, m_winH);

        if (m_local && m_local->isAlive() && !m_scopeMode)
            m_renderer->renderWeaponModel(
                m_weapAnim.current,
                m_weapAnim.isReloading,
                m_weapAnim.reloadFrac,
                m_weapAnim.fireTimer,
                m_weapAnim.bobT,
                m_cam, m_winW, m_winH);

        m_renderer->endFrame();
        glfwSwapBuffers(m_glfw);

        // SFML HUD overlay (drawn to its own window and displayed on top using transparency)
        // NOTE: On most systems you need compositor support for transparent SFML overlay.
        // Simpler: draw HUD directly into a sf::RenderTexture then blit as OpenGL texture.
        // For now we display HUD in a secondary borderless window positioned exactly over GLFW.
        m_sfmlWin.setVisible(true);
        m_sfmlWin.clear(sf::Color(0,0,0,0));
        if (m_local)
            m_hud->draw(m_sfmlWin, m_local->state(), m_info, m_scopeMode);
        // Scoreboard
        if (m_menu->getState() == MenuState::SCOREBOARD)
            m_menu->draw();
        m_sfmlWin.display();
    }
}

// ─── HELPERS ──────────────────────────────────────────────────────────────────
void Game::captureMouse(bool capture) {
    m_mouseCaptured = capture;
    glfwSetInputMode(m_glfw, GLFW_CURSOR,
        capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (capture) {
        double x,y; glfwGetCursorPos(m_glfw,&x,&y);
        m_lastMouseX=x; m_lastMouseY=y;
    }
}

void Game::connectToServer(const ConnectInfo& ci) {
    m_phase = Phase::CONNECTING;
    m_menu->showMsg("Connecting to " + ci.ip + "...");
    m_net->connect(ci.ip, (uint16_t)ci.port, ci.name);
}

void Game::spawnLocal(const SpawnPoint& sp) {
    if (m_local) m_local->respawn(sp);
}

std::vector<Player3D> Game::remotePlayerList() const {
    std::vector<Player3D> out;
    for (auto& [id, p] : m_players)
        if (!p.isLocal()) out.push_back(p);
    return out;
}

// ─── GLFW CALLBACKS ───────────────────────────────────────────────────────────
void Game::cbResize(GLFWwindow*, int w, int h) {
    if (!s_instance) return;
    s_instance->m_winW = w; s_instance->m_winH = h;
    s_instance->m_renderer->resize(w, h);
    s_instance->m_sfmlWin.setSize({(unsigned)w,(unsigned)h});
}

void Game::cbKey(GLFWwindow*, int key, int, int action, int) {
    if (!s_instance) return;
    if (key == GLFW_KEY_Y && action == GLFW_PRESS
        && s_instance->m_phase == Phase::PLAYING) {
        s_instance->m_hud->m_chatOpen = !s_instance->m_hud->m_chatOpen;
    }
}

void Game::cbMouseMove(GLFWwindow*, double x, double y) {
    if (!s_instance) return;
    if (!s_instance->m_mouseCaptured) { s_instance->m_lastMouseX=x; s_instance->m_lastMouseY=y; return; }
    float dx = (float)(x - s_instance->m_lastMouseX);
    float dy = (float)(y - s_instance->m_lastMouseY);
    s_instance->m_lastMouseX = x; s_instance->m_lastMouseY = y;

    float sens = s_instance->m_menu->getSettings().mouseSens;
    s_instance->m_cam.rotate(dx, -dy, sens);
    s_instance->m_input.yaw   = s_instance->m_cam.yaw;
    s_instance->m_input.pitch = s_instance->m_cam.pitch;
}

} // namespace CS
