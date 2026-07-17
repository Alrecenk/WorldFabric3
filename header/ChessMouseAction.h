#ifndef _CHESS_CLICK_ACTION_H_
#define _CHESS_CLICK_ACTION_H_ 1

#include "ActionMap.h"

class ChessMouseAction : public virtual RayAction {
public:
	int64_t held_piece = -1;
	int64_t next_held_piece = -1 ;
	bool clicked = false ;
};

#endif // #ifndef _CHESS_CLICK_ACTION_H_