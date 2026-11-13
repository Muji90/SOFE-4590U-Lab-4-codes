#include "game_logic.h"

/*
 * Deterministic Minimax subtraction game.
 *
 * Players take turns removing 1, 2 or 3.
 * The AI maximizes the result.
 * The human minimizes the result.
 *
 * AI victory    = +1
 * Human victory = -1
 */

int minimax(int remaining, int is_ai_turn)
{
    if (remaining <= 0)
    {
#if LAST_MOVE_WINS
        return is_ai_turn ? -1 : 1;
#else
        return is_ai_turn ? 1 : -1;
#endif
    }

    int best_score = is_ai_turn ? -2 : 2;

    for (int move = 1; move <= GAME_MAX_MOVE; move++)
    {
        if (move <= remaining)
        {
            int score = minimax(
                remaining - move,
                !is_ai_turn
            );

            if (is_ai_turn)
            {
                if (score > best_score)
                {
                    best_score = score;
                }
            }
            else
            {
                if (score < best_score)
                {
                    best_score = score;
                }
            }
        }
    }

    return best_score;
}

/*
 * Determine the best move for the AI.
 * The first optimal move wins ties,
 * ensuring deterministic behavior.
 */
int find_best_move(int remaining)
{
    if (remaining <= 0)
    {
        return 0;
    }

    int best_move = 1;
    int best_score = -2;

    for (int move = 1; move <= GAME_MAX_MOVE; move++)
    {
        if (move <= remaining)
        {
            int score = minimax(
                remaining - move,
                0
            );

            if (score > best_score)
            {
                best_score = score;
                best_move = move;
            }
        }
    }

    return best_move;
}
