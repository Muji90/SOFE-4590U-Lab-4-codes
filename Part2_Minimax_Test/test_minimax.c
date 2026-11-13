#include <stdio.h>
#include <stdbool.h>
#include "game_logic.h"

#define MAX_TEST_STATE 20

/*
 * Independently calculate winning and losing
 * states using dynamic programming.
 *
 * This checks the Minimax AI against a
 * separate method.
 */

int main(void)
{
    bool winning[MAX_TEST_STATE + 1] = {false};

#if LAST_MOVE_WINS
    winning[0] = false;
#else
    winning[0] = true;
#endif

    /* Calculate optimal outcomes. */
    for (int n = 1; n <= MAX_TEST_STATE; n++)
    {
        for (int move = 1; move <= GAME_MAX_MOVE; move++)
        {
            if (move <= n && !winning[n - move])
            {
                winning[n] = true;
                break;
            }
        }
    }

    int passed = 0;
    int failed = 0;

    printf("SOFE 4590U Lab 4 - Minimax Optimality Test\n");
    printf("------------------------------------------\n");

    for (int remaining = 1;
         remaining <= MAX_TEST_STATE;
         remaining++)
    {
        int move = find_best_move(remaining);

        bool legal =
            move >= 1 &&
            move <= GAME_MAX_MOVE &&
            move <= remaining;

        bool optimal = false;

        if (legal)
        {
            if (!winning[remaining])
            {
                /*
                 * No winning move exists.
                 * Any legal move is optimal.
                 */
                optimal = true;
            }
            else
            {
                /*
                 * A winning move must leave
                 * a losing state for the opponent.
                 */
                optimal = !winning[remaining - move];
            }
        }

        printf(
            "Remaining: %2d | AI Move: %d | %s\n",
            remaining,
            move,
            optimal ? "PASS" : "FAIL"
        );

        if (optimal)
        {
            passed++;
        }
        else
        {
            failed++;
        }
    }

    printf("------------------------------------------\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);

    return failed == 0 ? 0 : 1;
}

