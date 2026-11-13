#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#define GAME_START_NUMBER 20
#define GAME_MAX_MOVE 3

/*
 * 0 = Player taking the last number loses.
 * 1 = Player taking the last number wins.
 *
 * The supplied Minimax example's terminal
 * scoring corresponds to last-move-loses.
 */
#define LAST_MOVE_WINS 0

int minimax(int remaining, int is_ai_turn);
int find_best_move(int remaining);

#endif
