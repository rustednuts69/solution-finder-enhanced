#ifndef SFT_GAME_CORE_H
#define SFT_GAME_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#define SFT_GAME_WIDTH 10
#define SFT_GAME_HEIGHT 20
#define SFT_GAME_BAG_SIZE 7
#define SFT_GAME_QUEUE_SIZE 64
#define SFT_GAME_FIELD_TEXT_CAPACITY 224
#define SFT_GAME_PATTERN_TEXT_CAPACITY 2048

typedef enum SFTPieceType {
    SFT_PIECE_NONE = 0,
    SFT_PIECE_I = 1,
    SFT_PIECE_L = 2,
    SFT_PIECE_O = 3,
    SFT_PIECE_Z = 4,
    SFT_PIECE_T = 5,
    SFT_PIECE_J = 6,
    SFT_PIECE_S = 7
} SFTPieceType;

typedef enum SFTGameCommand {
    SFT_CMD_LEFT = 1,
    SFT_CMD_RIGHT = 2,
    SFT_CMD_SOFT_DROP = 3,
    SFT_CMD_HARD_DROP = 4,
    SFT_CMD_ROTATE_CW = 5,
    SFT_CMD_ROTATE_CCW = 6,
    SFT_CMD_HOLD = 7,
    SFT_CMD_TICK = 8,
    SFT_CMD_LOCK = 9,
    SFT_CMD_ROTATE_180 = 10
} SFTGameCommand;

typedef enum SFTLockResetMode {
    SFT_LOCK_RESET_NONE = 0,
    SFT_LOCK_RESET_MOVE = 1,
    SFT_LOCK_RESET_STEP = 2
} SFTLockResetMode;

typedef struct SFTPCScoutCandidate {
    int pieces;
    int clear_lines;
} SFTPCScoutCandidate;

typedef struct SFTGameState {
    int board[SFT_GAME_HEIGHT * SFT_GAME_WIDTH];
    int current;
    int hold;
    int can_hold;
    int rotation;
    int x;
    int y;
    int queue[SFT_GAME_QUEUE_SIZE];
    int queue_index;
    int queue_count;
    int custom_queue_enabled;
    int bag_index;
    unsigned int rng_state;
    int game_over;
    int lines_cleared;
    int pieces_locked;
    int last_clear_lines;
    int last_clear_t_spin;
    int last_clear_t_spin_mini;
    int last_action_was_rotation;
    int gravity_ms;
    int gravity_level;
    int gravity_base_level;
    int level_progression_enabled;
    int progression_start_lines;
    int lock_delay_ms;
    int gravity_elapsed_ms;
    long long gravity_accumulator_units;
    int lock_elapsed_ms;
    int grounded;
    int lock_reset_mode;
    int lock_reset_limit;
    int lock_resets_used;
    int lowest_y;
    int gravity_enabled;
    int infinite_lock_delay;
    int infinite_hold;
} SFTGameState;

void sft_game_init(SFTGameState *state);
void sft_game_init_seeded(SFTGameState *state, unsigned int seed);
void sft_game_reset(SFTGameState *state);
void sft_game_reset_seeded(SFTGameState *state, unsigned int seed);
void sft_game_load_fumen_cells(SFTGameState *state, const int *cells240);
void sft_game_set_tuning(SFTGameState *state, int gravity_ms, int lock_delay_ms);
void sft_game_set_gravity_level(SFTGameState *state, int level);
void sft_game_reset_level_progression(SFTGameState *state, int level);
void sft_game_set_level_progression(SFTGameState *state, int enabled);
int sft_game_lock_delay_for_level(int level);
void sft_game_set_lock_delay(SFTGameState *state, int lock_delay_ms);
void sft_game_set_lock_reset(SFTGameState *state, int mode, int move_limit);
void sft_game_set_options(SFTGameState *state, int gravity_enabled, int infinite_lock_delay, int infinite_hold);
void sft_game_set_custom_queue(SFTGameState *state, const int *pieces, int count);
void sft_game_clear_custom_queue(SFTGameState *state);
void sft_game_set_hold_piece(SFTGameState *state, int piece);
int sft_game_drop_to_surface(SFTGameState *state);
void sft_game_write_fumen_cells(const SFTGameState *state, int include_active, int *cells240);
void sft_game_tick(SFTGameState *state, int elapsed_ms, int soft_drop);
int sft_game_command(SFTGameState *state, int command);
int sft_game_cell(const SFTGameState *state, int x, int y, int include_active);
int sft_game_ghost_cell(const SFTGameState *state, int x, int y);
void sft_game_write_render_cells(const SFTGameState *state, int *visible_cells, int *ghost_cells);
int sft_game_queue_piece(const SFTGameState *state, int index);
int sft_game_piece_cell(int piece, int rotation, int x, int y);
int sft_game_pc_scout_candidates(const SFTGameState *state, int max_pieces, SFTPCScoutCandidate *candidates, int capacity);
int sft_game_write_active_pattern(
    const SFTGameState *state,
    int piece_count,
    char *buffer,
    int capacity
);
int sft_game_write_active_patterns(
    const SFTGameState *state,
    int piece_count,
    char *buffer,
    int capacity
);
int sft_game_write_sfinder_field(
    const SFTGameState *state,
    int clear_lines,
    char *buffer,
    int capacity
);

#ifdef __cplusplus
}
#endif

#endif
