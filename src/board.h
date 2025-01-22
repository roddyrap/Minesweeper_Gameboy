#ifndef MINESWEEPER_BOARD_H
#define MINESWEEPER_BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Information stored on each tile.
typedef struct {
    bool is_bomb     : 1;
    bool is_flagged  : 1;

    // Should be set by an external user because board.h doesn't handle drawing.
    bool is_revealed : 1;
} board_tile_t;

typedef struct{
    uint8_t board_size;

    // Game information
    uint16_t bombs_num;
    uint16_t flags_num;
    uint16_t unflagged_bombs;

    // Bombs should only be initialized after first click because the first click should not be
    // adjacent to any bomb.
    bool initialized_bombs;

    // Information about tiles on the board, should have size of BoardNum^2.
    board_tile_t *tiles;

    bool __clicked_bomb;
} game_board_t;

game_board_t *board_new(uint8_t board_size, uint16_t bombs_num);

void board_free(game_board_t* board);

bool board_initialize_bombs(game_board_t *board, uint8_t player_x, uint8_t player_y);

bool board_flag_tile(game_board_t *board, uint8_t x, uint8_t y);
bool board_click_tile(game_board_t *board, uint8_t x, uint8_t y);
void board_mark_revealed(game_board_t *board, uint8_t x, uint8_t y);

bool board_is_flag(game_board_t *board, uint8_t x, uint8_t y);
bool board_is_bomb(game_board_t *board, uint8_t x, uint8_t y);
bool board_is_revealed(game_board_t *board, uint8_t x, uint8_t y);

uint8_t board_bombs_near_tile(game_board_t *board, uint8_t x, uint8_t y);
uint8_t board_flags_near_tile(game_board_t *board, uint8_t x, uint8_t y);

bool board_is_game_over(game_board_t *board, bool *o_victory);


board_tile_t *board_get_tile(game_board_t *board, uint8_t x, uint8_t y);

// Takes 2D coordinates and flattens them to 1D.
uint16_t board_flatten_coords(game_board_t *board, uint8_t x, uint8_t y);

// checks if given cooridnates are in the board.
bool coords_in_board(game_board_t *board, int8_t x, int8_t y);

#endif // MINESWEEPER_BOARD_H