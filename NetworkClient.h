#pragma once
#include "GameTypes.h"
#include "Protocol.h"
#include <SFML/Network.hpp>
#include <deque>
#include <string>
#include <vector>

namespace CS {

enum class NetState { DISCONNECTED, CONNECTING, CONNECTED };

struct NetEvent {
    enum class Type { CONNECTED, DISCONNECTED, STATE_UPDATE,
                      BULLET, HIT, KILL, ROUND_START, ROUND_END, CHAT };
    Type type;
    // payloads
    std::vector<PlayerState> players;
    GameInfo     info;
    BulletEvent  bullet;
    std::string  text;
    uint8_t      playerId  = 0;
    bool         headshot  = false;
    Team         winTeam   = Team::UNASSIGNED;
    WeaponID     weapon    = WeaponID::GLOCK;
    int          damage    = 0;
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool connect(const std::string& ip, uint16_t port, const std::string& name);
    void disconnect();
    bool isConnected() const { return m_state==NetState::CONNECTED; }
    NetState state()   const { return m_state; }
    uint8_t  myId()    const { return m_myId;  }

    void update();
    void sendInput(const InputState& in);
    void sendTeam(Team t);
    void sendChat(const std::string& msg);
    void sendBuy(uint8_t slot);

    bool pollEvent(NetEvent& out);

private:
    sf::TcpSocket m_tcp;
    sf::UdpSocket m_udp;
    sf::IpAddress m_serverIp;
    uint16_t      m_serverPort = 0;
    NetState      m_state  = NetState::DISCONNECTED;
    uint8_t       m_myId   = 0;
    uint32_t      m_tick   = 0;

    std::vector<uint8_t>  m_buf;
    std::deque<NetEvent>  m_events;

    void recvTCP();
    void recvUDP();
    void processPacket(const uint8_t* data, size_t sz);
    void push(NetEvent&& e){ m_events.push_back(std::move(e)); }

    template<typename T>
    void sendTCP(const T& pkt){
        if(m_state==NetState::DISCONNECTED) return;
        uint16_t len=sizeof(T);
        sf::Packet p; p.append(&len,2); p.append(&pkt,sizeof(T));
        m_tcp.send(p);
    }
};

} // namespace CS

// ── Implementation ─────────────────────────────────────────────────────────────
#include <iostream>
#include <cstring>

namespace CS {

NetworkClient::NetworkClient(){
    m_tcp.setBlocking(false);
    m_udp.setBlocking(false);
    m_udp.bind(sf::Socket::AnyPort);
}
NetworkClient::~NetworkClient(){ disconnect(); }

bool NetworkClient::connect(const std::string& ip,uint16_t port,const std::string& name){
    m_serverIp=sf::IpAddress::resolve(ip).value(); m_serverPort=port;
    m_state=NetState::CONNECTING;
    auto s=m_tcp.connect(m_serverIp,port,sf::seconds(5.f));
    if(s!=sf::Socket::Status::Done&&s!=sf::Socket::Status::NotReady){ m_state=NetState::DISCONNECTED; return false; }
    Protocol::PktConnect pkt; std::strncpy(pkt.name,name.c_str(),23);
    sendTCP(pkt);
    std::cout<<"[Net] Connecting to "<<ip<<":"<<port<<"\n";
    return true;
}

void NetworkClient::disconnect(){
    if(m_state==NetState::DISCONNECTED) return;
    Protocol::Header bye{Protocol::MAGIC,Protocol::PktType::DISCONNECT};
    sendTCP(bye);
    m_tcp.disconnect(); m_state=NetState::DISCONNECTED;
}

void NetworkClient::update(){ recvTCP(); recvUDP(); }

void NetworkClient::sendInput(const InputState& in){
    if(!isConnected()) return;
    Protocol::PktInput pkt; pkt.playerId=m_myId; pkt.input=in; pkt.tick=m_tick++;
    sf::Packet p; p.append(&pkt,sizeof(pkt));
    m_udp.send(p,m_serverIp,m_serverPort+1);
}

void NetworkClient::sendTeam(Team t){
    Protocol::PktTeamSelect pkt; pkt.team=t; sendTCP(pkt);
}
void NetworkClient::sendChat(const std::string& msg){
    Protocol::PktChat pkt; std::strncpy(pkt.text,msg.c_str(),127); sendTCP(pkt);
}
void NetworkClient::sendBuy(uint8_t slot){
    Protocol::PktBuy pkt; pkt.slot=slot; sendTCP(pkt);
}

void NetworkClient::recvTCP(){
    uint8_t tmp[4096]; std::size_t got=0;
    auto s=m_tcp.receive(tmp,sizeof(tmp),got);
    if(s==sf::Socket::Status::Done&&got>0) m_buf.insert(m_buf.end(),tmp,tmp+got);
    else if(s==sf::Socket::Status::Disconnected){ m_state=NetState::DISCONNECTED; NetEvent e; e.type=NetEvent::Type::DISCONNECTED; push(std::move(e)); return; }
    while(m_buf.size()>=2){
        uint16_t len; std::memcpy(&len,m_buf.data(),2);
        if(m_buf.size()<(size_t)(2+len)) break;
        processPacket(m_buf.data()+2,len);
        m_buf.erase(m_buf.begin(),m_buf.begin()+2+len);
    }
}

void NetworkClient::recvUDP(){
    sf::Packet pkt; std::optional<sf::IpAddress> from; uint16_t port;
    while(m_udp.receive(pkt,from,port)==sf::Socket::Status::Done){
        if(pkt.getDataSize()<sizeof(Protocol::Header)) continue;
        auto* h=reinterpret_cast<const Protocol::Header*>(pkt.getData());
        if(h->magic!=Protocol::MAGIC) continue;
        if(h->type==Protocol::PktType::STATE_UPDATE&&pkt.getDataSize()>=sizeof(Protocol::PktStateUpdate)){
            auto* su=reinterpret_cast<const Protocol::PktStateUpdate*>(pkt.getData());
            NetEvent e; e.type=NetEvent::Type::STATE_UPDATE; e.info=su->info;
            for(int i=0;i<su->count;i++) e.players.push_back(su->players[i]);
            push(std::move(e));
        }
    }
}

void NetworkClient::processPacket(const uint8_t* d,size_t sz){
    if(sz<sizeof(Protocol::Header)) return;
    auto* h=reinterpret_cast<const Protocol::Header*>(d);
    switch(h->type){
    case Protocol::PktType::CONN_ACK:
        if(sz>=sizeof(Protocol::PktConnAck)){
            auto* a=reinterpret_cast<const Protocol::PktConnAck*>(d);
            if(a->ok){ m_myId=a->id; m_state=NetState::CONNECTED; NetEvent e; e.type=NetEvent::Type::CONNECTED; push(std::move(e)); }
            else{ m_state=NetState::DISCONNECTED; }
        } break;
    case Protocol::PktType::FULL_STATE:
        if(sz>=sizeof(Protocol::PktFullState)){
            auto* fs=reinterpret_cast<const Protocol::PktFullState*>(d);
            NetEvent e; e.type=NetEvent::Type::STATE_UPDATE; e.info=fs->info;
            for(int i=0;i<fs->count;i++) e.players.push_back(fs->players[i]);
            push(std::move(e));
        } break;
    case Protocol::PktType::BULLET_EVT:
        if(sz>=sizeof(Protocol::PktBulletEvt)){
            auto* be=reinterpret_cast<const Protocol::PktBulletEvt*>(d);
            NetEvent e; e.type=NetEvent::Type::BULLET; e.bullet=be->evt; push(std::move(e));
        } break;
    case Protocol::PktType::HIT_EVT:
        if(sz>=sizeof(Protocol::PktHitEvt)){
            auto* he=reinterpret_cast<const Protocol::PktHitEvt*>(d);
            NetEvent e; e.type=NetEvent::Type::HIT; e.playerId=he->victim; e.headshot=he->hs; e.damage=he->dmg; push(std::move(e));
        } break;
    case Protocol::PktType::KILL_FEED:
        if(sz>=sizeof(Protocol::PktKillFeed)){
            auto* kf=reinterpret_cast<const Protocol::PktKillFeed*>(d);
            NetEvent e; e.type=NetEvent::Type::KILL;
            e.text=std::string(kf->killer)+" -> "+kf->victim;
            e.weapon=kf->wpn; e.headshot=kf->hs; push(std::move(e));
        } break;
    case Protocol::PktType::ROUND_START:
        if(sz>=sizeof(Protocol::PktRoundStart)){
            auto* rs=reinterpret_cast<const Protocol::PktRoundStart*>(d);
            NetEvent e; e.type=NetEvent::Type::ROUND_START; e.info=rs->info; push(std::move(e));
        } break;
    case Protocol::PktType::ROUND_END:
        if(sz>=sizeof(Protocol::PktRoundEnd)){
            auto* re=reinterpret_cast<const Protocol::PktRoundEnd*>(d);
            NetEvent e; e.type=NetEvent::Type::ROUND_END; e.winTeam=re->winner; push(std::move(e));
        } break;
    case Protocol::PktType::CHAT_MSG:
        if(sz>=sizeof(Protocol::PktChat)){
            auto* cm=reinterpret_cast<const Protocol::PktChat*>(d);
            NetEvent e; e.type=NetEvent::Type::CHAT; e.text=cm->text; push(std::move(e));
        } break;
    default: break;
    }
}

bool NetworkClient::pollEvent(NetEvent& out){
    if(m_events.empty()) return false;
    out=std::move(m_events.front()); m_events.pop_front(); return true;
}

} // namespace CS
