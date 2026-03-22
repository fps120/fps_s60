# CS 1.6 Clone — 3D First Person Shooter
### C++17 · OpenGL 3.3 Core · SFML 2.5 · GLFW 3 · GLM

---

## 📦 Dependencies

```bash
# Ubuntu / Debian / WSL2
sudo apt install \
    cmake build-essential \
    libglfw3-dev \
    libsfml-dev \
    libglm-dev \
    libgl1-mesa-dev
```

---

## 🔧 Build

```bash
cd cs16_3d
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Produces:
- `cs_client` — 3D game client
- `cs_server` — dedicated server

---

## 🖥️ Running

```bash
# Terminal 1 — Start server
cd build
./cs_server                         # default: port 27015, de_dust2_3d
./cs_server -port 27020 -map de_dust2_3d

# Terminal 2+ — Start client(s)
./cs_client
```

---

## 🎮 Controls

| Key / Button          | Action                    |
|-----------------------|---------------------------|
| **W A S D**           | Move                      |
| **Space**             | Jump (incl. onto crates)  |
| **L-Ctrl**            | Crouch                    |
| **Mouse**             | Look (FPS mouse-look)     |
| **LMB**               | Shoot                     |
| **RMB**               | Scope (AWP only)          |
| **R**                 | Reload                    |
| **1 – 6**             | Switch weapon             |
| **Tab**               | Scoreboard                |
| **Y**                 | Open chat                 |
| **Escape**            | Back to menu              |

---

## 🗺️ Map Format

```
MAP_NAME  my_map

# Rooms (rendered from inside — ceiling/floor/4 walls)
ROOM  cx cy cz  sx sy sz  r g b

# Solid boxes (walls, columns — block movement & bullets)
SOLIDBOX  cx cy cz  sx sy sz  r g b

# Props (jumpable/cover objects)
PROP CRATE_S  cx cy cz  yaw    # small crate  0.5×0.5×0.5m
PROP CRATE_L  cx cy cz  yaw    # large crate  0.7×1.2×0.7m
PROP BARREL   cx cy cz  yaw    # barrel       0.4×0.9×0.4m

# Spawn points
SPAWN T   x y z  yaw_degrees
SPAWN CT  x y z  yaw_degrees

# Point lights
LIGHT  x y z  r g b  radius
```

---

## ⚙️ Architecture

```
cs16_3d/
├── shared/
│   ├── GameTypes.h       3D player state, weapons, AABB, physics constants
│   └── Protocol.h        TCP/UDP packet definitions
│
├── vendor/glad/          OpenGL 3.3 function loader (GLFW-backed)
│
├── shaders/
│   ├── world.vert/frag   Phong lighting + distance fog
│   └── weapon.vert/frag  Viewmodel shader (separate depth clear)
│
├── client/
│   ├── Camera.h          FPS camera: yaw/pitch, view/proj matrices, bob
│   ├── Map3D.h/cpp       Map loading → vertex buffer → GPU draw
│   ├── Player3D.h/cpp    AABB physics, step-on-objects, raycast shooting
│   ├── Renderer3D.h/cpp  OpenGL render: world, capsule players, weapons,
│   │                     3D bullet tracers + hit sparks
│   ├── WeaponModel.h     Weapon animation state (bob, recoil, reload)
│   ├── HUD3D.h           SFML overlay: HP/ammo/crosshair/scope/kill feed
│   ├── Menu3D.h          Full menu system (main/connect/settings/team/scores)
│   ├── NetworkClient.h   TCP+UDP client
│   └── Game.h/cpp        GLFW window + main loop + SFML HUD overlay
│
└── server/
    ├── ServerGame3D.h    Authoritative 3D physics + raytrace shooting
    ├── Server.h          TCP/UDP server, 20 Hz state broadcast
    └── main.cpp          CLI entry point
```

---

## 🔫 Weapons

| Slot | Name      | Damage | FireRate | Magazine | Cost   |
|------|-----------|--------|----------|----------|--------|
| 1    | Knife     | 34     | —        | —        | free   |
| 2    | Glock-18  | 25     | 4/s      | 20       | $200   |
| 3    | MP5       | 26     | 10/s     | 30       | $1500  |
| 4    | AK-47     | 36     | 10/s     | 30       | $2500  |
| 5    | AWP       | 115    | 1.5/s    | 5        | $4750  |
| 6    | M3        | 20×8   | 1/s      | 8        | $1700  |

---

## 💡 Technical highlights

### 3D jump-on-objects physics
Each axis resolved separately. Y-axis: if player bottom overlaps solid top → land on it, `grounded=true`. Works for crates, barrels — same AABB logic at any height.

### Weapon viewmodel
Rendered with a **separate depth buffer clear** so it never clips through walls. Uses its own projection matrix (tighter near plane). Procedural box meshes — no external model files needed.

### Bullet tracers
3D `GL_LINES` from muzzle to impact, fading over 200–500ms. Hit sparks (particle system) at impact point — red for player hits, yellow for wall hits.

### Network
- **TCP**: connect/disconnect/events/buy/chat
- **UDP**: input (every frame ~60Hz) + state snapshot (20Hz)
- Client-side prediction: local player runs same physics as server; server state reconciles if drift > 0.5m
