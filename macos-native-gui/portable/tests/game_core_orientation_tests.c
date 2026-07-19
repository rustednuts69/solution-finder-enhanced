#include "game_core.h"

#include <stdio.h>

typedef struct ExpectedCell {
    int x;
    int y;
} ExpectedCell;

static int contains(const ExpectedCell expected[4], int x, int y) {
    for (int i = 0; i < 4; i++) {
        if (expected[i].x == x && expected[i].y == y) {
            return 1;
        }
    }
    return 0;
}

static int check_piece(int piece, const char *name, const ExpectedCell expected[4]) {
    SFTGameState state;
    int queue[] = {piece};
    int visible[SFT_GAME_WIDTH * SFT_GAME_HEIGHT];
    int ghosts[SFT_GAME_WIDTH * SFT_GAME_HEIGHT];
    int occupied = 0;

    sft_game_init_seeded(&state, 12345u);
    sft_game_set_custom_queue(&state, queue, 1);

    if (state.current != piece || state.rotation != 0 || state.x != 3 || state.y != 17) {
        fprintf(stderr, "%s did not spawn in the expected SRS state\n", name);
        return 0;
    }

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            const int actual = sft_game_piece_cell(piece, 0, x, y);
            const int wanted = contains(expected, x, y);
            if (actual != wanted) {
                fprintf(stderr, "%s spawn mismatch at local cell (%d,%d)\n", name, x, y);
                return 0;
            }
            occupied += actual;
        }
    }

    if (occupied != 4) {
        fprintf(stderr, "%s spawn contains %d cells instead of 4\n", name, occupied);
        return 0;
    }

    for (int i = 0; i < 4; i++) {
        const int boardX = state.x + expected[i].x;
        const int boardY = state.y + expected[i].y;
        if (sft_game_cell(&state, boardX, boardY, 1) != piece) {
            fprintf(stderr, "%s active spawn was not rendered at board cell (%d,%d)\n", name, boardX, boardY);
            return 0;
        }
    }

    sft_game_write_render_cells(&state, visible, ghosts);
    for (int y = 0; y < SFT_GAME_HEIGHT; ++y) {
        for (int x = 0; x < SFT_GAME_WIDTH; ++x) {
            const int index = y * SFT_GAME_WIDTH + x;
            if (visible[index] != sft_game_cell(&state, x, y, 1)
                || ghosts[index] != sft_game_ghost_cell(&state, x, y)) {
                fprintf(stderr, "%s render snapshot mismatch at board cell (%d,%d)\n", name, x, y);
                return 0;
            }
        }
    }
    return 1;
}

int main(void) {
    static const ExpectedCell i_cells[4] = {{0,2}, {1,2}, {2,2}, {3,2}};
    static const ExpectedCell l_cells[4] = {{0,1}, {1,1}, {2,1}, {2,2}};
    static const ExpectedCell o_cells[4] = {{1,1}, {2,1}, {1,2}, {2,2}};
    static const ExpectedCell z_cells[4] = {{0,2}, {1,2}, {1,1}, {2,1}};
    static const ExpectedCell t_cells[4] = {{0,1}, {1,1}, {2,1}, {1,2}};
    static const ExpectedCell j_cells[4] = {{0,1}, {1,1}, {2,1}, {0,2}};
    static const ExpectedCell s_cells[4] = {{1,2}, {2,2}, {0,1}, {1,1}};

    return check_piece(SFT_PIECE_I, "I", i_cells)
        && check_piece(SFT_PIECE_L, "L", l_cells)
        && check_piece(SFT_PIECE_O, "O", o_cells)
        && check_piece(SFT_PIECE_Z, "Z", z_cells)
        && check_piece(SFT_PIECE_T, "T", t_cells)
        && check_piece(SFT_PIECE_J, "J", j_cells)
        && check_piece(SFT_PIECE_S, "S", s_cells)
        ? 0
        : 1;
}
