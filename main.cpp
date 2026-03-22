#include "Game.h"
#include <iostream>
int main(){try{CS::Game g;return g.run();}catch(const std::exception& e){std::cerr<<"[FATAL] "<<e.what()<<"\n";return 1;}}
