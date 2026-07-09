#include "game_core.h"

#include <string.h>

typedef struct Offset {
    int x;
    int y;
} Offset;

static const int bag_order[SFT_GAME_BAG_SIZE] = {
    SFT_PIECE_I, SFT_PIECE_T, SFT_PIECE_O, SFT_PIECE_L, SFT_PIECE_J, SFT_PIECE_S, SFT_PIECE_Z
};

static const Offset piece_offsets[8][4][4] = {
    {{{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}},
    {{{0,2},{1,2},{2,2},{3,2}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,1},{1,1},{2,1},{3,1}}, {{1,0},{1,1},{1,2},{1,3}}},
    {{{0,1},{1,1},{2,1},{2,2}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{0,2},{1,2}}},
    {{{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}}},
    {{{0,2},{1,2},{1,1},{2,1}}, {{2,2},{1,1},{2,1},{1,0}}, {{0,1},{1,1},{1,0},{2,0}}, {{1,2},{0,1},{1,1},{0,0}}},
    {{{0,1},{1,1},{2,1},{1,2}}, {{1,0},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{0,1},{1,1},{1,2}}},
    {{{0,1},{1,1},{2,1},{0,2}}, {{1,0},{1,1},{1,2},{2,2}}, {{2,0},{0,1},{1,1},{2,1}}, {{0,0},{1,0},{1,1},{1,2}}},
    {{{1,2},{2,2},{0,1},{1,1}}, {{1,2},{1,1},{2,1},{2,0}}, {{1,1},{2,1},{0,0},{1,0}}, {{0,2},{0,1},{1,1},{1,0}}}
};

static const int srs_kicks_jlstz[8][5][2] = {
    {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}},
    {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}},
    {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}},
    {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}},
    {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}},
    {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}},
    {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}},
    {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}}
};

static const int srs_kicks_i[8][5][2] = {
    {{0,0}, {-2,0}, {1,0}, {-2,-1}, {1,2}},
    {{0,0}, {2,0}, {-1,0}, {2,1}, {-1,-2}},
    {{0,0}, {-1,0}, {2,0}, {-1,2}, {2,-1}},
    {{0,0}, {1,0}, {-2,0}, {1,-2}, {-2,1}},
    {{0,0}, {2,0}, {-1,0}, {2,1}, {-1,-2}},
    {{0,0}, {-2,0}, {1,0}, {-2,-1}, {1,2}},
    {{0,0}, {1,0}, {-2,0}, {1,-2}, {-2,1}},
    {{0,0}, {-1,0}, {2,0}, {-1,2}, {2,-1}}
};

static const int srs_180_kicks_jlstz[4][6][2] = {
    {{0,0}, {0,1}, {1,1}, {-1,1}, {1,0}, {-1,0}},
    {{0,0}, {1,0}, {1,2}, {1,1}, {0,2}, {0,1}},
    {{0,0}, {0,-1}, {-1,-1}, {1,-1}, {-1,0}, {1,0}},
    {{0,0}, {-1,0}, {-1,2}, {-1,1}, {0,2}, {0,1}}
};

static const int srs_180_kicks_i[4][6][2] = {
    {{0,0}, {-1,0}, {-2,0}, {1,0}, {2,0}, {0,1}},
    {{0,0}, {0,1}, {0,2}, {0,-1}, {0,-2}, {1,0}},
    {{0,0}, {1,0}, {2,0}, {-1,0}, {-2,0}, {0,-1}},
    {{0,0}, {0,1}, {0,2}, {0,-1}, {0,-2}, {-1,0}}
};

static int board_index(int x, int y) {
    return y * SFT_GAME_WIDTH + x;
}

static int queue_at(const SFTGameState *state, int offset) {
    int count = state->custom_queue_enabled ? state->queue_count : SFT_GAME_BAG_SIZE;
    if (count <= 0) {
        return SFT_PIECE_NONE;
    }
    return state->queue[(state->queue_index + offset) % count];
}

static unsigned int next_random(SFTGameState *state) {
    state->rng_state = state->rng_state * 1664525u + 1013904223u;
    return state->rng_state;
}

static void refill_queue(SFTGameState *state) {
    int bag[SFT_GAME_BAG_SIZE];
    for (int i = 0; i < SFT_GAME_BAG_SIZE; i++) {
        bag[i] = bag_order[i];
    }
    for (int i = SFT_GAME_BAG_SIZE - 1; i > 0; i--) {
        int j = (int)(next_random(state) % (unsigned int)(i + 1));
        int temp = bag[i];
        bag[i] = bag[j];
        bag[j] = temp;
    }
    for (int i = 0; i < SFT_GAME_BAG_SIZE; i++) {
        state->queue[i] = bag[i];
    }
    state->queue_index = 0;
    state->queue_count = SFT_GAME_BAG_SIZE;
    state->custom_queue_enabled = 0;
}

static int next_piece(SFTGameState *state) {
    int piece = queue_at(state, 0);
    state->queue_index += 1;
    if (state->custom_queue_enabled) {
        if (state->queue_count > 0) {
            state->queue_index %= state->queue_count;
        }
    } else if (state->queue_index >= SFT_GAME_BAG_SIZE) {
        refill_queue(state);
    }
    return piece;
}

static int collides(const SFTGameState *state, int piece, int rotation, int x, int y) {
    if (piece <= SFT_PIECE_NONE || piece > SFT_PIECE_S) {
        return 1;
    }

    int r = ((rotation % 4) + 4) % 4;
    for (int i = 0; i < 4; i++) {
        int px = x + piece_offsets[piece][r][i].x;
        int py = y + piece_offsets[piece][r][i].y;
        if (px < 0 || px >= SFT_GAME_WIDTH || py < 0) {
            return 1;
        }
        if (py >= SFT_GAME_HEIGHT) {
            continue;
        }
        if (state->board[board_index(px, py)] != 0) {
            return 1;
        }
    }
    return 0;
}

static int is_grounded(const SFTGameState *state) {
    return collides(state, state->current, state->rotation, state->x, state->y - 1);
}

static void update_grounded(SFTGameState *state) {
    state->grounded = is_grounded(state);
    if (!state->grounded) {
        state->lock_elapsed_ms = 0;
    }
}

static void spawn_piece(SFTGameState *state, int piece) {
    state->current = piece;
    state->rotation = 0;
    state->x = 3;
    state->y = 17;
    state->can_hold = 1;
    state->gravity_elapsed_ms = 0;
    state->lock_elapsed_ms = 0;
    state->grounded = 0;
    state->last_action_was_rotation = 0;
    if (collides(state, state->current, state->rotation, state->x, state->y)) {
        state->game_over = 1;
    }
    update_grounded(state);
}

static int corner_blocked(const SFTGameState *state, int x, int y) {
    if (x < 0 || x >= SFT_GAME_WIDTH || y < 0) {
        return 1;
    }
    if (y >= SFT_GAME_HEIGHT) {
        return 0;
    }
    return state->board[board_index(x, y)] != 0;
}

static int t_spin_corner_count(const SFTGameState *state) {
    int center_x = state->x + 1;
    int center_y = state->y + 1;
    int blocked = 0;
    blocked += corner_blocked(state, center_x - 1, center_y - 1);
    blocked += corner_blocked(state, center_x + 1, center_y - 1);
    blocked += corner_blocked(state, center_x - 1, center_y + 1);
    blocked += corner_blocked(state, center_x + 1, center_y + 1);
    return blocked;
}

static int t_spin_front_corner_count(const SFTGameState *state) {
    int center_x = state->x + 1;
    int center_y = state->y + 1;
    int rotation = ((state->rotation % 4) + 4) % 4;
    switch (rotation) {
    case 0:
        return corner_blocked(state, center_x - 1, center_y + 1)
            + corner_blocked(state, center_x + 1, center_y + 1);
    case 1:
        return corner_blocked(state, center_x + 1, center_y + 1)
            + corner_blocked(state, center_x + 1, center_y - 1);
    case 2:
        return corner_blocked(state, center_x - 1, center_y - 1)
            + corner_blocked(state, center_x + 1, center_y - 1);
    default:
        return corner_blocked(state, center_x - 1, center_y + 1)
            + corner_blocked(state, center_x - 1, center_y - 1);
    }
}

static int is_t_spin(const SFTGameState *state) {
    return state->current == SFT_PIECE_T
        && state->last_action_was_rotation
        && t_spin_corner_count(state) >= 3;
}

static int is_t_spin_mini(const SFTGameState *state) {
    return is_t_spin(state)
        && t_spin_corner_count(state) == 3
        && t_spin_front_corner_count(state) < 2;
}

static int clear_lines(SFTGameState *state) {
    int write_y = 0;
    int cleared = 0;
    int new_board[SFT_GAME_HEIGHT * SFT_GAME_WIDTH];
    memset(new_board, 0, sizeof(new_board));

    for (int y = 0; y < SFT_GAME_HEIGHT; y++) {
        int full = 1;
        for (int x = 0; x < SFT_GAME_WIDTH; x++) {
            if (state->board[board_index(x, y)] == 0) {
                full = 0;
                break;
            }
        }
        if (full) {
            cleared += 1;
            continue;
        }
        for (int x = 0; x < SFT_GAME_WIDTH; x++) {
            new_board[board_index(x, write_y)] = state->board[board_index(x, y)];
        }
        write_y += 1;
    }

    memcpy(state->board, new_board, sizeof(new_board));
    state->lines_cleared += cleared;
    return cleared;
}

static void lock_piece(SFTGameState *state) {
    int t_spin = is_t_spin(state);
    int t_spin_mini = t_spin ? is_t_spin_mini(state) : 0;
    int r = ((state->rotation % 4) + 4) % 4;
    for (int i = 0; i < 4; i++) {
        int px = state->x + piece_offsets[state->current][r][i].x;
        int py = state->y + piece_offsets[state->current][r][i].y;
        if (px >= 0 && px < SFT_GAME_WIDTH && py >= 0 && py < SFT_GAME_HEIGHT) {
            state->board[board_index(px, py)] = state->current;
        }
    }
    state->last_clear_lines = clear_lines(state);
    state->last_clear_t_spin = t_spin;
    state->last_clear_t_spin_mini = t_spin_mini;
    state->pieces_locked += 1;
    spawn_piece(state, next_piece(state));
}

static int try_move(SFTGameState *state, int dx, int dy) {
    if (!collides(state, state->current, state->rotation, state->x + dx, state->y + dy)) {
        state->x += dx;
        state->y += dy;
        if (dx != 0) {
            state->last_action_was_rotation = 0;
        }
        if (dx != 0 || dy > 0) {
            state->lock_elapsed_ms = 0;
        }
        update_grounded(state);
        return 1;
    }
    update_grounded(state);
    return 0;
}

static int srs_transition_index(int from, int to) {
    if (from == 0 && to == 1) { return 0; }
    if (from == 1 && to == 0) { return 1; }
    if (from == 1 && to == 2) { return 2; }
    if (from == 2 && to == 1) { return 3; }
    if (from == 2 && to == 3) { return 4; }
    if (from == 3 && to == 2) { return 5; }
    if (from == 3 && to == 0) { return 6; }
    if (from == 0 && to == 3) { return 7; }
    return -1;
}

static int try_rotate(SFTGameState *state, int delta) {
    int current_rotation = ((state->rotation % 4) + 4) % 4;
    int next_rotation = ((current_rotation + delta) % 4 + 4) % 4;
    int transition = srs_transition_index(current_rotation, next_rotation);
    if (transition < 0) {
        update_grounded(state);
        return 0;
    }

    if (state->current == SFT_PIECE_O) {
        if (!collides(state, state->current, next_rotation, state->x, state->y)) {
            state->rotation = next_rotation;
            state->lock_elapsed_ms = 0;
            state->last_action_was_rotation = state->current == SFT_PIECE_T ? 1 : 0;
            update_grounded(state);
            return 1;
        }
        update_grounded(state);
        return 0;
    }

    const int (*kicks)[2] = state->current == SFT_PIECE_I ? srs_kicks_i[transition] : srs_kicks_jlstz[transition];
    for (int i = 0; i < 5; i++) {
        int nx = state->x + kicks[i][0];
        int ny = state->y + kicks[i][1];
        if (!collides(state, state->current, next_rotation, nx, ny)) {
            state->x = nx;
            state->y = ny;
            state->rotation = next_rotation;
            state->lock_elapsed_ms = 0;
            state->last_action_was_rotation = state->current == SFT_PIECE_T ? 1 : 0;
            update_grounded(state);
            return 1;
        }
    }
    update_grounded(state);
    return 0;
}

static int try_rotate_180(SFTGameState *state) {
    int current_rotation = ((state->rotation % 4) + 4) % 4;
    int next_rotation = (current_rotation + 2) % 4;

    if (state->current == SFT_PIECE_O) {
        if (!collides(state, state->current, next_rotation, state->x, state->y)) {
            state->rotation = next_rotation;
            state->lock_elapsed_ms = 0;
            state->last_action_was_rotation = state->current == SFT_PIECE_T ? 1 : 0;
            update_grounded(state);
            return 1;
        }
        update_grounded(state);
        return 0;
    }

    const int (*kicks)[2] = state->current == SFT_PIECE_I
        ? srs_180_kicks_i[current_rotation]
        : srs_180_kicks_jlstz[current_rotation];
    for (int i = 0; i < 6; i++) {
        int nx = state->x + kicks[i][0];
        int ny = state->y + kicks[i][1];
        if (!collides(state, state->current, next_rotation, nx, ny)) {
            state->x = nx;
            state->y = ny;
            state->rotation = next_rotation;
            state->lock_elapsed_ms = 0;
            state->last_action_was_rotation = state->current == SFT_PIECE_T ? 1 : 0;
            update_grounded(state);
            return 1;
        }
    }
    update_grounded(state);
    return 0;
}

void sft_game_init(SFTGameState *state) {
    sft_game_init_seeded(state, 0x51f17u);
}

void sft_game_init_seeded(SFTGameState *state, unsigned int seed) {
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->rng_state = seed == 0 ? 0x51f17u : seed;
    state->gravity_ms = 1000;
    state->lock_delay_ms = 500;
    state->gravity_enabled = 1;
    state->infinite_lock_delay = 0;
    state->infinite_hold = 0;
    refill_queue(state);
    spawn_piece(state, next_piece(state));
}

void sft_game_reset(SFTGameState *state) {
    sft_game_init(state);
}

void sft_game_reset_seeded(SFTGameState *state, unsigned int seed) {
    sft_game_init_seeded(state, seed);
}

void sft_game_load_fumen_cells(SFTGameState *state, const int *cells240) {
    if (!state || !cells240) {
        return;
    }
    memset(state->board, 0, sizeof(state->board));
    for (int y = 0; y < SFT_GAME_HEIGHT; y++) {
        int fumen_row = 22 - y;
        for (int x = 0; x < SFT_GAME_WIDTH; x++) {
            int value = cells240[fumen_row * 10 + x];
            state->board[board_index(x, y)] = value;
        }
    }
    state->game_over = collides(state, state->current, state->rotation, state->x, state->y);
    update_grounded(state);
}

void sft_game_set_tuning(SFTGameState *state, int gravity_ms, int lock_delay_ms) {
    if (!state) {
        return;
    }
    state->gravity_ms = gravity_ms < 10 ? 10 : gravity_ms;
    state->lock_delay_ms = lock_delay_ms < 0 ? 0 : lock_delay_ms;
}

void sft_game_set_options(SFTGameState *state, int gravity_enabled, int infinite_lock_delay, int infinite_hold) {
    if (!state) {
        return;
    }
    state->gravity_enabled = gravity_enabled ? 1 : 0;
    state->infinite_lock_delay = infinite_lock_delay ? 1 : 0;
    state->infinite_hold = infinite_hold ? 1 : 0;
}

void sft_game_set_custom_queue(SFTGameState *state, const int *pieces, int count) {
    if (!state || !pieces || count <= 0) {
        return;
    }
    int normalized_count = count > SFT_GAME_QUEUE_SIZE ? SFT_GAME_QUEUE_SIZE : count;
    int write_count = 0;
    for (int i = 0; i < normalized_count; i++) {
        if (pieces[i] > SFT_PIECE_NONE && pieces[i] <= SFT_PIECE_S) {
            state->queue[write_count] = pieces[i];
            write_count += 1;
        }
    }
    if (write_count <= 0) {
        return;
    }
    state->queue_count = write_count;
    state->queue_index = 0;
    state->custom_queue_enabled = 1;
    state->game_over = 0;
    spawn_piece(state, next_piece(state));
}

void sft_game_clear_custom_queue(SFTGameState *state) {
    if (!state) {
        return;
    }
    refill_queue(state);
    state->game_over = 0;
    spawn_piece(state, next_piece(state));
}

void sft_game_set_hold_piece(SFTGameState *state, int piece) {
    if (!state) {
        return;
    }
    state->hold = piece > SFT_PIECE_NONE && piece <= SFT_PIECE_S ? piece : SFT_PIECE_NONE;
}

void sft_game_write_fumen_cells(const SFTGameState *state, int include_active, int *cells240) {
    if (!state || !cells240) {
        return;
    }
    memset(cells240, 0, sizeof(int) * 240);
    for (int y = 0; y < SFT_GAME_HEIGHT; y++) {
        int fumen_row = 22 - y;
        for (int x = 0; x < SFT_GAME_WIDTH; x++) {
            cells240[fumen_row * 10 + x] = sft_game_cell(state, x, y, include_active);
        }
    }
}

void sft_game_tick(SFTGameState *state, int elapsed_ms, int soft_drop) {
    if (!state || state->game_over) {
        return;
    }
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }

    if (state->gravity_enabled || soft_drop) {
        int gravity = soft_drop ? state->gravity_ms / 20 : state->gravity_ms;
        if (gravity < 10) {
            gravity = 10;
        }
        state->gravity_elapsed_ms += elapsed_ms;
        while (state->gravity_elapsed_ms >= gravity && !state->game_over) {
            state->gravity_elapsed_ms -= gravity;
            if (!try_move(state, 0, -1)) {
                break;
            }
        }
    }

    update_grounded(state);
    if (state->grounded && !state->infinite_lock_delay) {
        state->lock_elapsed_ms += elapsed_ms;
        if (state->lock_elapsed_ms >= state->lock_delay_ms) {
            lock_piece(state);
        }
    }
}

int sft_game_command(SFTGameState *state, int command) {
    if (!state || state->game_over) {
        return 0;
    }

    switch (command) {
    case SFT_CMD_LEFT:
        return try_move(state, -1, 0);
    case SFT_CMD_RIGHT:
        return try_move(state, 1, 0);
    case SFT_CMD_SOFT_DROP:
        return try_move(state, 0, -1);
    case SFT_CMD_HARD_DROP:
        while (try_move(state, 0, -1)) {}
        lock_piece(state);
        return 1;
    case SFT_CMD_ROTATE_CW:
        return try_rotate(state, 1);
    case SFT_CMD_ROTATE_CCW:
        return try_rotate(state, -1);
    case SFT_CMD_ROTATE_180:
        return try_rotate_180(state);
    case SFT_CMD_HOLD: {
        if (!state->can_hold && !state->infinite_hold) {
            return 0;
        }
        int old_hold = state->hold;
        state->hold = state->current;
        state->can_hold = state->infinite_hold ? 1 : 0;
        spawn_piece(state, old_hold == SFT_PIECE_NONE ? next_piece(state) : old_hold);
        state->can_hold = state->infinite_hold ? 1 : 0;
        update_grounded(state);
        return 1;
    }
    case SFT_CMD_TICK:
        sft_game_tick(state, state->gravity_ms, 0);
        return 1;
    case SFT_CMD_LOCK:
        lock_piece(state);
        return 1;
    default:
        return 0;
    }
}

int sft_game_cell(const SFTGameState *state, int x, int y, int include_active) {
    if (!state || x < 0 || x >= SFT_GAME_WIDTH || y < 0 || y >= SFT_GAME_HEIGHT) {
        return 0;
    }
    if (include_active && state->current > 0) {
        int r = ((state->rotation % 4) + 4) % 4;
        for (int i = 0; i < 4; i++) {
            int px = state->x + piece_offsets[state->current][r][i].x;
            int py = state->y + piece_offsets[state->current][r][i].y;
            if (px == x && py == y) {
                return state->current;
            }
        }
    }
    return state->board[board_index(x, y)];
}

int sft_game_ghost_cell(const SFTGameState *state, int x, int y) {
    if (!state || state->game_over || state->current <= SFT_PIECE_NONE || x < 0 || x >= SFT_GAME_WIDTH || y < 0 || y >= SFT_GAME_HEIGHT) {
        return 0;
    }

    int ghost_y = state->y;
    while (!collides(state, state->current, state->rotation, state->x, ghost_y - 1)) {
        ghost_y -= 1;
    }

    int r = ((state->rotation % 4) + 4) % 4;
    for (int i = 0; i < 4; i++) {
        int px = state->x + piece_offsets[state->current][r][i].x;
        int py = ghost_y + piece_offsets[state->current][r][i].y;
        if (px == x && py == y) {
            return state->current;
        }
    }
    return 0;
}

int sft_game_queue_piece(const SFTGameState *state, int index) {
    if (!state || index < 0 || index >= SFT_GAME_QUEUE_SIZE) {
        return 0;
    }
    return queue_at(state, index);
}
