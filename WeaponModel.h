#pragma once
#include "GameTypes.h"
#include <cmath>
#include <algorithm>
namespace CS{struct WeaponAnimState{float fireTimer=0,reloadTimer=0,bobT=0,reloadFrac=0;bool isReloading=false;WeaponID current=WeaponID::GLOCK;void update(const PlayerState& ps,float dt,float moveSpeed){current=ps.weapon;isReloading=ps.reloading;if(fireTimer>0)fireTimer-=dt;if(isReloading){float total=WEAPONS[(int)current].reloadTime;reloadTimer+=dt;reloadFrac=1.f-std::min(reloadTimer/total,1.f);}else{reloadTimer=0;reloadFrac=0;}if(moveSpeed>0.5f)bobT+=dt*moveSpeed*1.2f;}void onShoot(){fireTimer=WEAPONS[(int)current].fireRate>0?1.f/WEAPONS[(int)current].fireRate:0.f;}};}
