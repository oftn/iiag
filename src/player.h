//
// player.h
//

#ifndef PLAYER_H
#define PLAYER_H

#include "world.h"
#include "creature.h"

#define PLYR world.plyr
#define plyr_is_me(C) ((C) == &PLYR)

// player willed actions
void plyr_act_pickup(void);
void plyr_act_drop(void);
void plyr_act_inv(void);
void plyr_act_move(int, int);

// the inevitable
void plyr_ev_death(void);
void plyr_ev_lvlup(void);

#endif
