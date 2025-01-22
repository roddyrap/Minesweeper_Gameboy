#include "board.h"

#include <string.h>
#include <rand.h>

game_board_t *board_new(uint8_t board_size, uint16_t bombs_num)
{
    game_board_t *board = malloc(sizeof(game_board_t));

    board->board_size = board_size,

    // Game information
    board->bombs_num = bombs_num;
    board->flags_num = bombs_num;
    board->unflagged_bombs = bombs_num;

    // Bombs should only be initialized after first click because the first click should not be
    // adjacent to any bomb.
    board->initialized_bombs = false;

    // Information about tiles on the board, should have size of BoardNum^2.
    board->tiles = NULL;

    board->__clicked_bomb = false;

    return board;
}

void board_free(game_board_t* board)
{
    if (board == NULL) return;

    if (board->tiles != NULL)
    {
        free(board->tiles);
        board->tiles = NULL;
    }

    free(board);
    board = NULL;
}


// First player click, initializing bombs.
bool board_initialize_bombs(game_board_t *board, uint8_t player_x, uint8_t player_y)
{
    if (board->tiles != NULL)
    {
        // Clear tiles.
        memset(board->tiles, 0, sizeof(board_tile_t) * board->board_size * board->board_size);
    }
    else {
        // Because we use calloc, we know that the boolean flags aren't toggled, and can
        // skip initializing them.
        board->tiles = calloc(board->board_size * board->board_size, sizeof(board_tile_t));
        if (board->tiles == NULL)
        {
            return false;
        }
    }

    // Bomb generation script - Uses Reservoir sampling to create the random locations. This method
    // requires an array to save the locations in. This wastes quite a lot of memory but is much
    // more efficient.
    uint16_t* bomb_locations = (uint16_t*)malloc(board->bombs_num * sizeof(uint16_t));
    if (bomb_locations == NULL)
    {
        return false;
    }

    uint16_t protected_modifier = 0;

    // Setting the starting values to the location array.
    for (uint32_t tile_index = 0; tile_index < board->bombs_num; tile_index++)
    {
        // Checking if the tile is protected. If it is this tile will be skipped (with the incrementation of protected_modifier) and so on.
        while (abs((int16_t)player_x - (tile_index + protected_modifier) % board->board_size) <= 1 && abs((int16_t)player_y - (tile_index + protected_modifier) / board->board_size) <= 1)
        {
            protected_modifier++;
        }

        bomb_locations[tile_index] = tile_index + protected_modifier;
    }

    // Iterating over every other available number.
    for (uint32_t tile_index = board->bombs_num; tile_index + protected_modifier < board->board_size * board->board_size; tile_index++)
    {
        // Checking if its protected.
        while (abs((int16_t)player_x - (tile_index + protected_modifier) % board->board_size) <= 1 && abs((int16_t)player_y - (tile_index + protected_modifier) / board->board_size) <= 1)
        {
            protected_modifier++;
        }

        // Replacing it with a previously in number.
        uint32_t generated_index = rand() % tile_index;
        if (generated_index < board->bombs_num)
        {
            bomb_locations[generated_index] = tile_index + protected_modifier;
        }
    }

    // Iterating over the chosen numbers in order to insert them to the board matrix.
    for (int32_t bomb_index = 0; bomb_index < board->bombs_num; bomb_index++)
    {
        board->tiles[bomb_locations[bomb_index]].is_bomb = true;
    }

    board->initialized_bombs = true;

    free(bomb_locations);
    bomb_locations = NULL;

    return true;
}

// Tile flag status is changed due to a click.
bool board_flag_tile(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return false;
    }

    board_tile_t *tile_p = &(board->tiles[board_flatten_coords(board, x, y)]);
    if (tile_p->is_revealed)
    {
        return false;
    }

    bool is_bomb = tile_p->is_bomb;

    if (tile_p->is_flagged)
    {
        tile_p->is_flagged = false;
        board->flags_num++;
        if (is_bomb)
        {
            board->unflagged_bombs++;
        }
    }
    else
    {
        if (board->flags_num <= 0)
        {
            return false;
        }

        // Set tile to flagged.
        tile_p->is_flagged = true;
        board->flags_num--;

        if (is_bomb)
        {
            board->unflagged_bombs--;
        }
    }

    return true;
}

// Click on tile.
bool board_click_tile(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return false;
    }

    board_tile_t *board_tile = board_get_tile(board, x, y);

    // Revealing a flagged tile is not allowed.
    if (board_tile->is_flagged || board_tile->is_revealed)
    {
        return false;
    }

    if (board_is_bomb(board, x, y))
    {
        board->__clicked_bomb = true;
        return true;
    }

    return true;
}

void board_mark_revealed(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return;
    }

    board_get_tile(board, x, y)->is_revealed = true;
}

// Takes 2D coordinates and flattens them to 1D.
uint16_t board_flatten_coords(game_board_t *board, uint8_t x, uint8_t y)
{
    return ((uint16_t)y) * board->board_size + x;
}

// checks if given cooridnates are in the board.
bool coords_in_board(game_board_t *board, int8_t x, int8_t y)
{
    return !(x < 0 || y < 0 || x >= (uint16_t) board->board_size || y >= (uint16_t) board->board_size);
}

bool board_is_flag(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return false;
    }

    return board_get_tile(board, x, y)->is_flagged;
}

// TODO: I think this can be cleaner.
bool board_is_game_over(game_board_t *board, bool *o_victory)
{
    if (board->__clicked_bomb)
    {
        *o_victory = false;
        return true;
    }

    if (board->unflagged_bombs == 0)
    {
        *o_victory = true;
        return true;
    }

    return false;
}

board_tile_t *board_get_tile(game_board_t *board, uint8_t x, uint8_t y)
{
    if (board->tiles == NULL)
    {
        return NULL;
    }

    return &(board->tiles[board_flatten_coords(board, x, y)]);
}

bool board_is_bomb(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return false;
    }

    return board_get_tile(board, x, y)->is_bomb;
}

bool board_is_revealed(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return false;
    }

    return board_get_tile(board, x, y)->is_revealed;
}

// Doesn't consider if the tile itself is a bomb.
uint8_t board_bombs_near_tile(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return 0;
    }

    // Loop over nearby tiles and find bombs.
    uint8_t num_bombs = 0;
    for (int16_t x_modifier = -1; x_modifier < 2; x_modifier++)
    {
        for (int16_t y_modifier = -1; y_modifier < 2; y_modifier++)
        {
            if (!coords_in_board(board, x + x_modifier, y + y_modifier) ||
                (x_modifier == 0 && y_modifier == 0))
            {
                continue;
            }

            num_bombs += board_get_tile(board, x + x_modifier, y + y_modifier)->is_bomb;
        }
    }

    return num_bombs;
}

uint8_t board_flags_near_tile(game_board_t *board, uint8_t x, uint8_t y)
{
    if (!board->initialized_bombs)
    {
        return 0;
    }

    // Loop over nearby tiles and find bombs.
    uint8_t num_flags = 0;
    for (int16_t x_modifier = -1; x_modifier < 2; x_modifier++)
    {
        for (int16_t y_modifier = -1; y_modifier < 2; y_modifier++)
        {
            if (!coords_in_board(board, x + x_modifier, y + y_modifier) ||
                (x_modifier == 0 && y_modifier == 0))
            {
                continue;
            }

            num_flags += board_is_flag(board, x + x_modifier, y + y_modifier);
        }
    }

    return num_flags;
}