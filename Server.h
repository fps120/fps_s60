#pragma once
#include "ServerGame3D.h"
#include "Protocol.h"
#include <SFML/Network.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>

namespace CS {

struct ClientConn3D {
    uint8_t id=0;
    std::unique_ptr<sf::TcpSocket> sock;
    std::string name;
    Team team=Team::UNASSIGNED;
    bool active=true;
    std::optional<sf::IpAddress> udpIp{sf::IpAddress(0u)};
    uint16_t udpPort=0;
    std::vector<uint8_t> buf;
};

class Server3D {
public:
    Server3D(uint16_t tcp,uint16_t udp);
    ~Server3D();
    bool start(const std::string& mapPath);
    void run();
    void stop(){ m_running=false; }

private:
    uint16_t m_tcpPort,m_udpPort;
    sf::TcpListener   m_listener;
    sf::UdpSocket     m_udp;
    sf::SocketSelector m_sel;
    std::unordered_map<uint8_t,ClientConn3D> m_clients;
    ServerGame3D m_game;
    bool m_running=false;
    float m_broadcastAccum=0;
    static constexpr float TICK_DT=1.f/TICK_RATE;
    static constexpr float BCAST_DT=1.f/20.f;

    void acceptClients();
    void recvClients();
    void recvUDP();
    void processTCP(ClientConn3D& cc,const uint8_t* d,size_t sz);
    void broadcastState();
    void sendFullState(ClientConn3D& cc);
    void kickClient(uint8_t id,const char* r);
    ClientConn3D* findClient(uint8_t id);

    template<typename T>
    void sendTCP(ClientConn3D& cc,const T& pkt){
        if(!cc.active)return;
        uint16_t len=sizeof(T);
        sf::Packet p;p.append(&len,2);p.append(&pkt,sizeof(T));
        if(cc.sock->send(p)!=sf::Socket::Status::Done)cc.active=false;
    }
    void broadcastTCP(const void* d,size_t sz,uint8_t except=0){
        uint16_t len=(uint16_t)sz;
        sf::Packet p;p.append(&len,2);p.append(d,sz);
        for(auto& [id,cc]:m_clients){if(!cc.active||id==except)continue;
            if(cc.sock->send(p)!=sf::Socket::Status::Done)cc.active=false;}
    }
    template<typename T> void broadcastTCP(const T& pkt,uint8_t ex=0){broadcastTCP(&pkt,sizeof(T),ex);}
    void sendUDP(const sf::IpAddress& ip,uint16_t port,const void* d,size_t sz){
        sf::Packet p;p.append(d,sz);m_udp.send(p,ip,port);}
};

// ── Implementation ─────────────────────────────────────────────────────────────
inline Server3D::Server3D(uint16_t tcp,uint16_t udp):m_tcpPort(tcp),m_udpPort(udp){
    m_game.onBullet=[this](const BulletEvent& e){
        Protocol::PktBulletEvt pkt;pkt.evt=e;broadcastTCP(pkt);};
    m_game.onHit=[this](uint8_t s,uint8_t v,int dmg,bool hs,bool k){
        Protocol::PktHitEvt he{};he.shooter=s;he.victim=v;he.dmg=(int16_t)dmg;he.hs=hs;he.killed=k;
        auto* sc=findClient(s);if(sc)sendTCP(*sc,he);
        if(k){Protocol::PktKillFeed kf{};
            auto* vc=findClient(v);
            if(sc)std::strncpy(kf.killer,sc->name.c_str(),23);
            if(vc)std::strncpy(kf.victim,vc->name.c_str(),23);
            kf.hs=hs;
            auto it=m_game.players().find(s);
            if(it!=m_game.players().end())kf.wpn=it->second.st.weapon;
            broadcastTCP(kf);}};
    m_game.onRound=[this](bool nr){
        const auto& gi=m_game.info();
        if(nr){Protocol::PktRoundStart rs{};rs.round=gi.round;rs.info=gi;broadcastTCP(rs);}
        else{Protocol::PktRoundEnd re{};re.ts=gi.tScore;re.cts=gi.ctScore;
            re.winner=gi.tScore>0?Team::TERRORIST:Team::CT;broadcastTCP(re);}};
}
inline Server3D::~Server3D(){stop();}

inline bool Server3D::start(const std::string& mapPath){
    if(!m_game.loadMap(mapPath)){std::cerr<<"[Server3D] Map load failed\n";return false;}
    if(m_listener.listen(m_tcpPort)!=sf::Socket::Status::Done){std::cerr<<"[Server3D] TCP bind failed\n";return false;}
    m_listener.setBlocking(false);
    m_sel.add(m_listener);
    if(m_udp.bind(m_udpPort)!=sf::Socket::Status::Done){std::cerr<<"[Server3D] UDP bind failed\n";return false;}
    m_udp.setBlocking(false);
    m_running=true;
    std::cout<<"[Server3D] TCP:"<<m_tcpPort<<"  UDP:"<<m_udpPort<<"  map:"<<mapPath<<"\n";
    return true;
}

inline void Server3D::run(){
    sf::Clock clk; float broadcastAccum=0,tickAccum=0,statusT=0;
    while(m_running){
        float dt=clk.restart().asSeconds();dt=std::min(dt,0.1f);
        acceptClients();recvClients();recvUDP();
        tickAccum+=dt;
        while(tickAccum>=TICK_DT){m_game.tick(TICK_DT);tickAccum-=TICK_DT;}
        broadcastAccum+=dt;
        if(broadcastAccum>=BCAST_DT){broadcastState();broadcastAccum-=BCAST_DT;}
        std::vector<uint8_t> dead;
        for(auto& [id,cc]:m_clients)if(!cc.active)dead.push_back(id);
        for(auto id:dead)kickClient(id,"disconnected");
        statusT+=dt;
        if(statusT>=10.f){statusT=0;
            std::cout<<"[Server3D] Players:"<<m_game.players().size()
                     <<"  Round:"<<m_game.info().round<<"\n";}
        sf::sleep(sf::milliseconds(1));
    }
}

inline void Server3D::acceptClients(){
    auto sock=std::make_unique<sf::TcpSocket>();sock->setBlocking(false);
    if(m_listener.accept(*sock)==sf::Socket::Status::Done){
        uint8_t id=m_game.addPlayer("Connecting...");
        ClientConn3D cc;cc.id=id;cc.sock=std::move(sock);cc.active=true;
        cc.udpIp=cc.sock->getRemoteAddress();
        m_sel.add(*cc.sock);m_clients[id]=std::move(cc);}
}

inline void Server3D::recvClients(){
    for(auto& [id,cc]:m_clients){if(!cc.active)continue;
        uint8_t tmp[4096];std::size_t got=0;
        auto s=cc.sock->receive(tmp,sizeof(tmp),got);
        if(s==sf::Socket::Status::Done&&got>0)cc.buf.insert(cc.buf.end(),tmp,tmp+got);
        else if(s==sf::Socket::Status::Disconnected){cc.active=false;continue;}
        while(cc.buf.size()>=2){
            uint16_t len;std::memcpy(&len,cc.buf.data(),2);
            if(cc.buf.size()<(size_t)(2+len))break;
            processTCP(cc,cc.buf.data()+2,len);
            cc.buf.erase(cc.buf.begin(),cc.buf.begin()+2+len);}}
}

inline void Server3D::recvUDP(){
    sf::Packet pkt;std::optional<sf::IpAddress> from{sf::IpAddress(0u)};uint16_t port;
    while(m_udp.receive(pkt,from,port)==sf::Socket::Status::Done){
        if(pkt.getDataSize()<sizeof(Protocol::Header))continue;
        auto* h=reinterpret_cast<const Protocol::Header*>(pkt.getData());
        if(h->magic!=Protocol::MAGIC)continue;
        if(h->type==Protocol::PktType::INPUT&&pkt.getDataSize()>=sizeof(Protocol::PktInput)){
            auto* pi=reinterpret_cast<const Protocol::PktInput*>(pkt.getData());
            auto it=m_clients.find(pi->playerId);
            if(it!=m_clients.end()){it->second.udpIp=from;it->second.udpPort=port;
                m_game.applyInput(pi->playerId,pi->input);}}}
}

inline void Server3D::processTCP(ClientConn3D& cc,const uint8_t* d,size_t sz){
    if(sz<sizeof(Protocol::Header))return;
    auto* h=reinterpret_cast<const Protocol::Header*>(d);
    switch(h->type){
    case Protocol::PktType::CONNECT:
        if(sz>=sizeof(Protocol::PktConnect)){
            auto* pc=reinterpret_cast<const Protocol::PktConnect*>(d);
            cc.name=pc->name;
            const_cast<SrvPlayer&>(m_game.players().at(cc.id)).st;
            Protocol::PktConnAck ack;ack.id=cc.id;ack.ok=true;sendTCP(cc,ack);
            sendFullState(cc);
            std::cout<<"[Server3D] '"<<cc.name<<"' id="<<(int)cc.id<<"\n";}
        break;
    case Protocol::PktType::TEAM_SELECT:
        if(sz>=sizeof(Protocol::PktTeamSelect)){
            auto* ts=reinterpret_cast<const Protocol::PktTeamSelect*>(d);
            cc.team=ts->team;m_game.setTeam(cc.id,ts->team);}
        break;
    case Protocol::PktType::CHAT:
        if(sz>=sizeof(Protocol::PktChat)){
            auto* cm=reinterpret_cast<const Protocol::PktChat*>(d);
            Protocol::PktChat fwd{};
            std::snprintf(fwd.text,sizeof(fwd.text),"[%s] %s",cc.name.c_str(),cm->text);
            broadcastTCP(fwd);}
        break;
    case Protocol::PktType::BUY:
        if(sz>=sizeof(Protocol::PktBuy)){
            auto* bk=reinterpret_cast<const Protocol::PktBuy*>(d);
            if(m_game.info().state==RoundState::BUY&&bk->slot<6){
                auto it=m_game.players().find(cc.id);
                if(it!=m_game.players().end()){
                    auto& sp=const_cast<SrvPlayer&>(it->second);
                    WeaponID wid=static_cast<WeaponID>(bk->slot);
                    int cost=WEAPONS[(int)wid].cost;
                    if(sp.st.money>=cost){sp.st.money-=cost;sp.st.weapon=wid;
                        sp.st.ammo=WEAPONS[(int)wid].magSize;sp.st.ammoRes=WEAPONS[(int)wid].magSize*2;}}}}
        break;
    case Protocol::PktType::DISCONNECT:
        cc.active=false;break;
    default:break;}
}

inline void Server3D::broadcastState(){
    Protocol::PktStateUpdate su{};su.info=m_game.info();su.count=0;
    for(auto& [id,sp]:m_game.players()){if(su.count>=MAX_PLAYERS)break;su.players[su.count++]=sp.st;}
    for(auto& [id,cc]:m_clients){if(!cc.active||cc.udpPort==0)continue;
        sendUDP(cc.udpIp,cc.udpPort,&su,sizeof(su));}
}

inline void Server3D::sendFullState(ClientConn3D& cc){
    Protocol::PktFullState fs{};fs.info=m_game.info();fs.count=0;
    std::strncpy(fs.mapName,"de_dust2_3d",31);
    for(auto& [id,sp]:m_game.players()){if(fs.count>=MAX_PLAYERS)break;fs.players[fs.count++]=sp.st;}
    sendTCP(cc,fs);
}

inline void Server3D::kickClient(uint8_t id,const char* r){
    auto it=m_clients.find(id);if(it==m_clients.end())return;
    std::cout<<"[Server3D] Kick "<<it->second.name<<" ("<<r<<")\n";
    m_sel.remove(*it->second.sock);it->second.sock->disconnect();
    m_game.removePlayer(id);
    Protocol::PktChat bye{};std::snprintf(bye.text,sizeof(bye.text),"*** %s left",it->second.name.c_str());
    broadcastTCP(bye,id);
    m_clients.erase(it);
}

inline ClientConn3D* Server3D::findClient(uint8_t id){
    auto it=m_clients.find(id);return it!=m_clients.end()?&it->second:nullptr;}

} // namespace CS
