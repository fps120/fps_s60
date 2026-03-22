#pragma once
#include "GameTypes.h"
#include <SFML/Network.hpp>

namespace Protocol {

constexpr uint16_t  PORT_TCP  = 27015;
constexpr uint16_t  PORT_UDP  = 27016;
constexpr uint32_t  MAGIC     = 0xC5163D00;

enum class PktType : uint8_t {
    // C→S TCP
    CONNECT=1, DISCONNECT=2, TEAM_SELECT=3, BUY=4, CHAT=5,
    // S→C TCP
    CONN_ACK=20, KICK=21, FULL_STATE=22,
    ROUND_START=23, ROUND_END=24, KILL_FEED=25, CHAT_MSG=26,
    BULLET_EVT=27, HIT_EVT=28,
    // UDP both
    INPUT=50, STATE_UPDATE=51, PING=52, PONG=53,
};

#pragma pack(push,1)
struct Header { uint32_t magic=MAGIC; PktType type; uint16_t seq=0; };

struct PktConnect    { Header hdr={MAGIC,PktType::CONNECT};    char name[24]={}; };
struct PktConnAck    { Header hdr={MAGIC,PktType::CONN_ACK};   uint8_t id=0; bool ok=true; char reason[32]={}; };
struct PktTeamSelect { Header hdr={MAGIC,PktType::TEAM_SELECT}; CS::Team team; };
struct PktBuy        { Header hdr={MAGIC,PktType::BUY};        uint8_t slot=0; };

struct PktInput {
    Header hdr={MAGIC,PktType::INPUT};
    uint8_t playerId;
    CS::InputState input;
    uint32_t tick;
};

struct PktStateUpdate {
    Header hdr={MAGIC,PktType::STATE_UPDATE};
    uint32_t tick;
    uint8_t count;
    CS::PlayerState players[CS::MAX_PLAYERS];
    CS::GameInfo    info;
};

struct PktBulletEvt  { Header hdr={MAGIC,PktType::BULLET_EVT}; CS::BulletEvent evt; };
struct PktHitEvt     { Header hdr={MAGIC,PktType::HIT_EVT};    uint8_t shooter,victim; int16_t dmg; bool hs,killed; };
struct PktKillFeed   { Header hdr={MAGIC,PktType::KILL_FEED};  char killer[24]; char victim[24]; CS::WeaponID wpn; bool hs; };
struct PktChat       { Header hdr={MAGIC,PktType::CHAT};       char text[128]; };

struct PktRoundStart { Header hdr={MAGIC,PktType::ROUND_START}; uint16_t round; CS::GameInfo info; };
struct PktRoundEnd   { Header hdr={MAGIC,PktType::ROUND_END};   CS::Team winner; uint8_t ts,cts; };

struct PktFullState {
    Header hdr={MAGIC,PktType::FULL_STATE};
    CS::GameInfo info;
    uint8_t count;
    CS::PlayerState players[CS::MAX_PLAYERS];
    char mapName[32];
};
#pragma pack(pop)

// Length-prefixed TCP send helper
template<typename T>
inline bool sendTCP(sf::TcpSocket& sock, const T& pkt) {
    uint16_t len = sizeof(T);
    sf::Packet p;
    p.append(&len, 2);
    p.append(&pkt, sizeof(T));
    return sock.send(p) == sf::Socket::Status::Done;
}

} // namespace Protocol
