#include <gb/gb.h>
#include <gb/cgb.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <rand.h>
#include <time.h>

#include "Tiles/mine_tile_sheet_data.c"
#include "Tiles/mine_tile_sheet_map.c"
#include "Tiles/sprites_data.c"
#include "Tiles/sprites_map.c"
#include "Tiles/ChicagoFont_data.c"
#include "Tiles/ChicagoFont_map.c"
#include "Tiles/ui_background_data.c"
#include "Tiles/ui_background_map.c"
#include "Tiles/mode_selector_data.c"

/**
 * Known bugs and issues:
 * - If a tile is revealed when the number of flags near it matches the number on it,
 *   it will also be immediately clicked.
 * Roadmap:
 * - Redesign cursor animation and add more frames.
 * - Visualize screen scrolling better.
 * - Bugfixes & stability improvements.
 * - More code refactoring.
 * - Maybe add save?
 * - Improve protected tiles implementation in bombs creation.
 * - Move UI to window layer and use background scrolling functionality.
 */

// Declaring function as it and click_tile call eachother.
void click_tile(uint8_t, uint8_t);

// Information stored on each tile.
typedef struct {
    bool is_revealed : 1;
    bool is_bomb     : 1;
    bool is_flagged  : 1;
} board_tile_t;

// Represents all uniquely drawn tiles.
typedef enum {
    TILE_TYPE_HIDDEN,
    TILE_TYPE_ONE,
    TILE_TYPE_TWO,
    TILE_TYPE_THREE,
    TILE_TYPE_FOUR,
    TILE_TYPE_FIVE,
    TILE_TYPE_SIX,
    TILE_TYPE_SEVEN,
    TILE_TYPE_EIGHT,
    TILE_TYPE_BOMB,
    TILE_TYPE_FLAG,
    TILE_TYPE_EMPTY,
} tile_type_t;

// CGB Palettes.
const uint16_t MINE_TILES_CGB_PALETTE[] = {
    RGB_WHITE, RGB_DARKGRAY, RGB_LIGHTGRAY, RGB_BLACK, // Default palette.
    RGB_WHITE, RGB_DARKGRAY, RGB_BLUE     , RGB_BLACK, // 1 neighbour.
    RGB_WHITE, RGB_DARKGRAY, RGB_GREEN    , RGB_BLACK, // 2 neighbours.
    RGB_WHITE, RGB_DARKGRAY, RGB_RED      , RGB_BLACK, // 3 neighbours.
    RGB_WHITE, RGB_DARKGRAY, RGB_PURPLE   , RGB_BLACK, // 4 neighbours.
    RGB_WHITE, RGB_DARKGRAY, RGB_DARKRED  , RGB_BLACK, // 5 neighbours.
    RGB_WHITE, RGB_DARKGRAY, RGB_CYAN     , RGB_BLACK, // 6 neighbours.
    RGB_WHITE, RGB_DARKGRAY, RGB_PURPLE   , RGB_BLACK, // 7 neighbours.
};

// Sprite palette (only one is required and used).
const uint16_t SPRITE_PALETTE[] = {
    RGB_BLUE, RGB_WHITE, RGB_LIGHTGRAY, RGB_BLACK
};

// Memory placements.
const uint8_t FONT_START          = 44;
const uint8_t UI_BACKGROUND_START = 140;

// Sprite data.
const uint8_t NUM_CURSOR_SPRITES = 8;

// Screen size (without UI).
const uint8_t NUM_ROWS = 9;
const uint8_t NUM_COLS = 6;

// Gameboy sprite top-left offsets from screen.
const uint8_t ORIGIN_X_OFFSET = 8;
const uint8_t ORIGIN_Y_OFFSET = 16;

// Constant scale factors.
const uint8_t BOARD_TILE_PIXELS_LENGTH       = 16;
const uint8_t BOARD_TILE_SCREEN_TILES_LENGTH = 2;
const uint8_t BOARD_TILE_SCREEN_TILE_AREA    = 4;
const uint8_t SCREEN_TILE_PIXELS_LENGTH      = 8;

// Time constants. Max determined by number of characters.
const uint32_t MAX_TIME = 999999;


// Difficulty names.
const char D10_NAME[] = "Easy  ";
const char D16_NAME[] = "Inter ";
const char D22_NAME[] = "Expert";
const char D32_NAME[] = "NiMare";

// Globals.

// UI tile values.
uint8_t time_passed[6] = { 0 };
uint8_t game_status[6] = { 0 };
uint8_t flags_used[8]  = { 0 };
uint8_t scroll_show[5] = { 0 };

// Game states.
bool board_manipulation_enabled = true;
bool counting_time              = false;
bool select_menu_open           = false;
bool initialized_bombs          = false;

// Time management.
uint16_t time_overflow_tracker = 0;
time_t start_time              = 0;
time_t previous_time_since     = 0;
uint32_t game_time             = 0;

// Button states.
bool a_clicked      = false;
bool b_clicked      = false;
bool select_clicked = false;

// Handling continous movement.
time_t movement_time = 0;

// What board size is selected.
int8_t select_param;

// Game board information (x, y).
uint8_t board_size      = 0;
uint8_t cursor[2]       = { 0 };
uint8_t scroll_state[2] = { 0 };

// Information about tiles on the board, should have size of BoardNum^2.
board_tile_t *board_tiles = NULL;

// Game information
uint16_t bombs_num       = 0;
uint16_t flags_num       = 0;
uint16_t unflagged_bombs = 0;

// Takes a string (str) and writes it's ascii value as tiles to another array (array), at the specified size (array_size).
void write_str_to_tile_array(uint8_t* o_array, uint8_t array_size, const char* str)
{
    for (uint8_t char_index = 0; char_index < array_size; char_index++)
    {
        if (str[char_index] == '\0')
        {
            break;
        }

        o_array[char_index] = str[char_index] - ' ' + FONT_START;
    }
}

// Take a number (8-bit, num) and write it to an array (array) from index arr_start to arr_end.
void number_to_chars(uint8_t *array, uint8_t arr_start, uint8_t arr_end, uint32_t num)
{
    for (uint8_t neg_index = 1; neg_index <= arr_end - arr_start; neg_index++)
    {
        array[arr_end - neg_index] = num % 10 + FONT_START + '0' - ' ';
        num = num / 10;
    }
}

// Assumes that the numbers are in order.
tile_type_t bombs_nearby_to_tile_type(int bombs_nearby)
{
    if (bombs_nearby == 0)
    {
        return TILE_TYPE_EMPTY;
    }
    else
    {
        return TILE_TYPE_ONE - 1 + bombs_nearby;
    }
}

// Stop the game and notify the user that an error occured.
void crash(int8_t exit_code)
{
    // Modify game status.
    write_str_to_tile_array(game_status, sizeof(game_status), "Crash");
    set_bkg_tiles(13, 3, sizeof(game_status), 1, game_status);

    write_str_to_tile_array(time_passed, sizeof(time_passed), "Error");
    set_bkg_tiles(13, 5, sizeof(time_passed), 1, time_passed);

    // Write crash code.
    number_to_chars(time_passed, 0, sizeof(time_passed), exit_code);
    set_bkg_tiles(13, 7, sizeof(time_passed), 1, time_passed);

    exit(exit_code);
}

// For winning and losing.
void game_over(bool is_win)
{
    // Update status.
    write_str_to_tile_array(game_status, sizeof(game_status), is_win ? "Win   " : "Lose  ");

    // Display status.
    set_bkg_tiles(13, 3, 6, 1, game_status);

    // Disable interactivity.
    board_manipulation_enabled = false;
    counting_time = false;
}

// Takes 2D coordinates and flattens them to 1D.
uint16_t flatten_coords(uint8_t x, uint8_t y, uint8_t row_size)
{
    return ((uint16_t)y) * row_size + x;
}

// checks if given cooridnates are in the board.
bool coords_in_board(int8_t x, int8_t y)
{
    return !(x < 0 || y < 0 || x >= (uint16_t) board_size || y >= (uint16_t) board_size);
}


// Actual position on board (unlike cursor which is graphical position).
uint8_t cursor_board_x()
{
    return cursor[0] + scroll_state[0];
}

uint8_t cursor_board_y()
{
    return cursor[1] + scroll_state[1];
}

// Finds the amount of bombs near a tile (technically including itself).
// BOMB_INDICATOR if tile is bomb.
uint8_t bombs_near_tile(uint8_t tile_x, uint8_t tile_y)
{
    // If the tile is a bomb return BOMB_INDICATOR.
    if (board_tiles[flatten_coords(tile_x, tile_y, board_size)].is_bomb)
    {
        return TILE_TYPE_BOMB;
    }

    // Loop over nearby tiles and find bombs.
    uint8_t num_bombs = 0;
    for (int16_t x_modifier = -1; x_modifier < 2; x_modifier++)
    {
        for (int16_t y_modifier = -1; y_modifier < 2; y_modifier++)
        {
            if (!coords_in_board(tile_x + x_modifier, tile_y + y_modifier))
            {
                continue;
            }

            num_bombs += board_tiles[flatten_coords(x_modifier + tile_x, y_modifier + tile_y, board_size)].is_bomb;
        }
    }

    return num_bombs;
}

// Draws a board tile (tile_type) to a specified location (x, y).
void draw_tile(uint8_t x, uint8_t y, tile_type_t tile_type)
{
    // Check if coordinates are in the graphical part of the screen.
    if (x >= NUM_COLS || y >= NUM_ROWS)
    {
        return;
    }

    // CGB specific palette swap to color tiles.
    if (_cpu == CGB_TYPE)
    {
        uint8_t palette_ind = tile_type;

        // Eight's color is light grey.
        if (tile_type == TILE_TYPE_EIGHT || tile_type == TILE_TYPE_EMPTY)
        {
            palette_ind = TILE_TYPE_HIDDEN;
        }
        // Three is red, and bombs and flags are meant to catch attention.
        else if (tile_type == TILE_TYPE_FLAG || tile_type == TILE_TYPE_BOMB)
        {
            palette_ind = TILE_TYPE_THREE;
        }

        // Write meta_data (tile palette is bits 0-2).
        VBK_REG = 1;

        // Setting number of tiles at once didn't work so for loop to loop over all 4 scren tiles.
        for (uint8_t tile_ind = 0; tile_ind < BOARD_TILE_SCREEN_TILE_AREA; tile_ind++)
        {
            set_bkg_tile_xy(x * BOARD_TILE_SCREEN_TILES_LENGTH + tile_ind % 2, y * BOARD_TILE_SCREEN_TILES_LENGTH + (int32_t)tile_ind / 2, palette_ind);
        }

        // Back to write regular data.
        VBK_REG = 0;
    }

    // Draw background with given tile, offset is BOARD_TILE_SCREEN_TILE_AREA * tile_type and starts from 0.
    set_bkg_tiles(2 * x, 2 * y, BOARD_TILE_SCREEN_TILES_LENGTH, BOARD_TILE_SCREEN_TILES_LENGTH,
                  mine_tile_sheet_map + BOARD_TILE_SCREEN_TILE_AREA * tile_type);
}

// Draws the whole board, factoring in the screen scroll.
void draw_current_board()
{
    for (uint8_t x_modifier = 0; x_modifier < NUM_COLS; x_modifier++)
    {
        // The X position of the drawn tiles with scroll.
        uint8_t new_x = x_modifier + scroll_state[0];

        for (uint8_t y_modifier = 0; y_modifier < NUM_ROWS; y_modifier++)
        {
            // The y position of the drawn tile with scroll.
            uint8_t new_y = y_modifier + scroll_state[1];

            // vram offset.
            tile_type_t tile_type = TILE_TYPE_HIDDEN;
            if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_flagged)
            {
                // set offset to flag tile.
                tile_type = TILE_TYPE_FLAG;
            }
            else if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_revealed)
            {
                // set to offset to bombs near.
                tile_type = bombs_nearby_to_tile_type(bombs_near_tile(new_x, new_y));
            }

            draw_tile(x_modifier, y_modifier, (tile_type_t)tile_type);
        }
    }
}

// Movement.
// Scrolls the board and redraws *all* tiles, no bkg_scroll functionality used. Doesn't warp.
void scroll_board(int8_t x, int8_t y)
{
    // Tracking if screen was scrolled (logically) to know if to draw board.
    bool has_scrolled = false;
    // Check conditions for horizontal scroll.
    if ((scroll_state[0] < board_size - NUM_COLS && x > 0) || (scroll_state[0] > 0 && x < 0))
    {
        // Track scroll.
        scroll_state[0] += x;
        // Draw when should.
        has_scrolled = true;
    }

    // Check conditions for vertical scroll.
    if ((board_size - NUM_ROWS > scroll_state[1] && y > 0) || (scroll_state[1] > 0 && y < 0))
    {
        // Track scroll.
        scroll_state[1] += y;
        // Draw when should.
        has_scrolled = true;
    }

    // If didn't scroll return.
    if (!has_scrolled)
    {
        return;
    }

    // If scrolling happened, redraw.
    draw_current_board();
}

void move_sprite_grid(uint8_t new_x, uint8_t new_y)
{
    // Different off-sets because of Gameboy Schenanigans.
    for (uint8_t sprite_ind = 0; sprite_ind < BOARD_TILE_SCREEN_TILE_AREA; sprite_ind++)
    {
        move_sprite(
            sprite_ind,
            new_x * BOARD_TILE_PIXELS_LENGTH + ORIGIN_X_OFFSET + SCREEN_TILE_PIXELS_LENGTH * (sprite_ind % 2),
            new_y * BOARD_TILE_PIXELS_LENGTH + ORIGIN_Y_OFFSET + SCREEN_TILE_PIXELS_LENGTH * (sprite_ind / 2));
    }
}


void move_cursor(int8_t x, int8_t y)
{
    // Check if player wants to scroll.
    int16_t scroll_x = 0;
    int16_t scroll_y = 0;

    if ((cursor[0] == 0 && x < 0) || (cursor[0] == NUM_COLS - 1 && x > 0))
    {
        scroll_x = x;
    }

    if ((cursor[1] == 0 && y < 0) || (cursor[1] == NUM_ROWS - 1 && y > 0))
    {
        scroll_y = y;
    }

    cursor[0] += scroll_x == 0 ? x : 0;
    cursor[1] += scroll_y == 0 ? y : 0;

    // Draw player grid.
    move_sprite_grid(cursor[0], cursor[1]);

    if (scroll_x != 0 || scroll_y != 0)
    {
        scroll_board(scroll_x, scroll_y);
    }
}

// Finds the amount of flags near a tile (not including itself).
uint8_t flags_near_tile(uint8_t tile_x, uint8_t tile_y)
{
    // Track number of flags.
    uint8_t num_flags = 0;
    // Loop over neighbores.
    for (int16_t x_modifier = -1; x_modifier < 2; x_modifier++)
    {
        for (int16_t y_modifier = -1; y_modifier < 2; y_modifier++)
        {
            // Check if neighbour is flag.
            if (!coords_in_board(tile_x + x_modifier, tile_y + y_modifier) || (x_modifier == 0 && y_modifier == 0))
            {
                continue;
            }

            // Track flags near.
            num_flags += board_tiles[flatten_coords(x_modifier + tile_x, y_modifier + tile_y, board_size)].is_flagged;
        }
    }

    return num_flags;
}

// Reveal nearby tiles.
void reveal_nearby(uint8_t x, uint8_t y)
{
    // Loop over neighbores.
    for (int16_t x_modifier = -1; x_modifier < 2; x_modifier++)
    {
        for (int16_t y_modifier = -1; y_modifier < 2; y_modifier++)
        {
            // If tile is not in board or is iteslf continue to next one.
            if (!coords_in_board(x + x_modifier, y + y_modifier) ||  (x_modifier == 0 && y_modifier == 0))
            {
                continue;
            }

            board_tile_t* board_tile = &board_tiles[flatten_coords(x_modifier + x, y_modifier + y, board_size)];
            if (board_tile->is_flagged || board_tile->is_revealed)
            {
                continue;
            }

            // Reveal tile by simulating clicking on it.
            click_tile(x + x_modifier, y + y_modifier);
        }
    }
}

// Click on tile, revealing it if possible.
void click_tile(uint8_t x, uint8_t y)
{
    board_tile_t* board_tile = &board_tiles[flatten_coords(x, y, board_size)];

    // Revealing a flagged tile is not allowed.
    if (board_tile->is_flagged)
    {
        return;
    }

    uint8_t bombs_near = bombs_near_tile(x, y);

    // If the tile is a bomb.
    if (bombs_near == TILE_TYPE_BOMB)
    {
        game_over(false);
    }

    // If the tile was not previously revealed.
    if (!board_tile->is_revealed)
    {
        // Set it to revealed.
        board_tile->is_revealed = true;

        // If no bombs are nearby autoreveal all nearby tiles.
        if (bombs_near == 0)
        {
            reveal_nearby(x, y);
        }
    }

    // Reveal surrounding spaces if the number of surrounding flags matches the number of surrounding bombs.
    if (flags_near_tile(x, y) == bombs_near && bombs_near != 0)
    {
        reveal_nearby(x, y);
    }

    // Re-draw clicked tile to show revealed status.
    draw_tile(x - scroll_state[0], y - scroll_state[1], bombs_nearby_to_tile_type(bombs_near));
}

// Change board size and number of bombs, functions as a reset.
void set_board_size(uint8_t new_size, uint8_t num_bombs)
{
    // Reset time
    previous_time_since = 0;
    time_overflow_tracker = 0;
    game_time = 0;

    // Reset board data.
    board_size = new_size;
    bombs_num = num_bombs;
    flags_num = num_bombs;
    unflagged_bombs = num_bombs;

    // Reset buttons.
    a_clicked = false;
    b_clicked = false;

    // Reset game status.
    board_manipulation_enabled = true;
    counting_time = false;

    // Close select menu.
    select_menu_open = false;

    // Initialize changing ui.
    write_str_to_tile_array(time_passed, sizeof(time_passed), "000000"  );
    write_str_to_tile_array(flags_used , sizeof(flags_used) , "000/000 ");
    write_str_to_tile_array(scroll_show, sizeof(scroll_show), "00|00 "  );

    // Initialize cursor.
    cursor[0] = 0;
    cursor[1] = 0;
    move_sprite_grid(cursor[0], cursor[1]);

    // Reset scroll.
    scroll_state[0] = 0;
    scroll_state[1] = 0;

    // updating flag string.
    number_to_chars(flags_used, 4, 7, flags_num);

    // Reset board data to zero.
    if (board_tiles != NULL)
    {
        free(board_tiles);
        board_tiles = NULL;
    }

    // Because we use calloc, we know that the boolean flags aren't toggled, and can
    // skip initializing them.
    board_tiles = (board_tile_t*)calloc(board_size * board_size, sizeof(board_tile_t));
    if (board_tiles == NULL)
    {
        crash(1);
    }

    // Bombs were not initialized and will be at next click.
    initialized_bombs = false;

    // Re-draw board.
    draw_current_board();
}

// First player click, initializing bombs.
void initialize_bombs()
{
    // Action is long and stops progress, fast processing is worth it.
    if (_cpu == CGB_TYPE)
    {
        cpu_fast();
    }

    initialized_bombs = true;

    // Bomb generation script - Uses Reservoir sampling to create the random locations.
    uint8_t player_x = cursor_board_x();
    uint8_t player_y = cursor_board_y();

    // This method requires an array to save the locations in. This wastes quite a lot of memory but is much more efficient.
    uint16_t* bomb_locations = (uint16_t*)malloc(bombs_num * sizeof(uint16_t));
    if (bomb_locations == NULL)
    {
        crash(1);
    }

    uint16_t protected_modifier = 0;

    // Setting the starting values to the location array.
    for (uint32_t tile_index = 0; tile_index < bombs_num; tile_index++)
    {
        // Checking if the tile is protected. If it is this tile will be skipped (with the incrementation of protected_modifier) and so on.
        while (abs((int16_t)player_x - (tile_index + protected_modifier) % board_size) <= 1 && abs((int16_t)player_y - (tile_index + protected_modifier) / board_size) <= 1)
        {
            protected_modifier++;
        }

        bomb_locations[tile_index] = tile_index + protected_modifier;
    }

    // Iterating over every other available number.
    for (uint32_t tile_index = bombs_num; tile_index + protected_modifier < board_size * board_size; tile_index++)
    {
        // Checking if its protected.
        while (abs((int16_t)player_x - (tile_index + protected_modifier) % board_size) <= 1 && abs((int16_t)player_y - (tile_index + protected_modifier) / board_size) <= 1)
        {
            protected_modifier++;
        }

        // Replacing it with a previously in number.
        uint32_t generated_index = rand() % tile_index;
        if (generated_index < bombs_num)
        {
            bomb_locations[generated_index] = tile_index + protected_modifier;
        }
    }

    // Iterating over the chosen numbers in order to insert them to the board matrix.
    for (int32_t bomb_index = 0; bomb_index < bombs_num; bomb_index++)
    {
        board_tiles[bomb_locations[bomb_index]].is_bomb = true;
    }

    free(bomb_locations);
    bomb_locations = NULL;

    // Stop fast processing.
    if (_cpu == CGB_TYPE)
    {
        cpu_slow();
    }

    // Start time measurement.
    start_time = clock();
    counting_time = true;
}

// Tile flag status is changed due to a click.
void flag_tile(uint8_t x, uint8_t y)
{
    board_tile_t *tile_p = &board_tiles[flatten_coords(x, y, board_size)];
    if (!initialized_bombs || tile_p->is_revealed)
    {
        return;
    }

    bool is_bomb = tile_p->is_bomb;

    if (tile_p->is_flagged)
    {
        tile_p->is_flagged = false;
        draw_tile(x - scroll_state[0], y - scroll_state[1], TILE_TYPE_HIDDEN);
        flags_num++;
        if (is_bomb)
        {
            unflagged_bombs++;
        }
    }
    else
    {
        if (flags_num <= 0)
        {
            return;
        }

        // Set tile to flagged.
        tile_p->is_flagged = true;

        // Re-draw tile.
        draw_tile(x - scroll_state[0], y - scroll_state[1], TILE_TYPE_FLAG);
        flags_num--;
        if (is_bomb)
        {
            unflagged_bombs--;
        }

        if (unflagged_bombs == 0)
        {
            game_over(true);
        }
    }
}

// Show backround and sprites.
void update_switches()
{
    SHOW_BKG;
    SHOW_SPRITES;
}

// Update ui to show current status.
void update_ui()
{
    // Update time.
    if (!select_menu_open)
    {
        if (counting_time)
        {
            // Track time.
            time_t time_since = clock() - start_time;

            // If the current time passed is smaller than previously, an overflow had occurred.
            if (time_since < previous_time_since)
            {
                // Logging the overflow.
                time_overflow_tracker++;
            }

            // Log the current time passed for the future.
            previous_time_since = time_since;

            // Don't want overflow to occur in (uint32_t)uint16 * uint16 + uint16.
            if (time_overflow_tracker < USHRT_MAX)
            {
                // Cast to uint32_t to enable values larger than the internal gameboy overflow. Multiply the
                // overflow tracker to get actual overflowed time in seconds and add current time in seconds to it.
                // If time is too large to present in the board, cap it.
                game_time = ((uint32_t)time_overflow_tracker * USHRT_MAX + time_since) / CLOCKS_PER_SEC;
            }
            else
            {
                game_time = MAX_TIME;
            }

            if (game_time > MAX_TIME)
            {
                game_time = MAX_TIME;
            }
        }

        // Draw string.
        number_to_chars(time_passed, 0, sizeof(time_passed), game_time);
        set_bkg_tiles(13, 7, sizeof(time_passed), 1, time_passed);
    }

    // Update flags left.
    number_to_chars(flags_used, 0, 3, flags_num);
    set_bkg_tiles(13, 11, 4, 2, flags_used);

    // Update scroll status.
    number_to_chars(scroll_show, 0, 2, scroll_state[0]);
    number_to_chars(scroll_show, 3, 5, scroll_state[1]);
    set_bkg_tiles(13, 16, 5, 1, scroll_show);
}

// Moves the select menu cursor.
void move_select(int8_t y)
{
    // Doesn't do anything if menu is closed.
    if (!select_menu_open)
    {
        return;
    }

    // Change tracking parameter.
    select_param += y;

    // Warp select cursor if going out of bounds.
    if (select_param > 3) select_param = 0;
    if (select_param < 0) select_param = 3;

    move_sprite(8, 24, 32 + select_param * 16);
}

// 0: Easy, 1: Inter, 2: Expert, 3: NiMare.
void set_difficulty(uint8_t new_diff)
{
    switch (new_diff)
    {
        case 0:
            write_str_to_tile_array(game_status, sizeof(game_status), D10_NAME);
            set_board_size(10, 10);
            break;
        case 1:
            write_str_to_tile_array(game_status, sizeof(game_status), D16_NAME);
            set_board_size(16, 40);
            break;
        case 2:
            write_str_to_tile_array(game_status, sizeof(game_status), D22_NAME);
            set_board_size(22, 99);
            break;
        case 3:
            write_str_to_tile_array(game_status, sizeof(game_status), D32_NAME);
            set_board_size(32, 200);
            break;
        default:
            set_difficulty(0);
    }

    // Draw status.
    set_bkg_tiles(13, 3, 6, 1, game_status);
}

void select()
{
    if (!select_menu_open)
    {
        select_menu_open = true;

        // Disable palettes if GBC.
        if (_cpu == CGB_TYPE)
        {
            // Write metadata.
            VBK_REG = 1;

            // Reset Palettes.
            for (uint8_t tile_x = 0; tile_x < 8; tile_x++)
            {
                for (uint8_t tile_y = 0; tile_y < 18; tile_y++)
                {
                    set_bkg_tile_xy(2 + tile_x, tile_y, 0);
                }
            }

            // Write regualr.
            VBK_REG = 0;
        }

        // Draw menu background.
        set_bkg_tiles(2, 0, 8, 18, ui_background_map);

        // Write text to background.
        uint8_t select_text[6] = { 0 };
        write_str_to_tile_array(select_text, 6, D10_NAME);
        set_bkg_tiles(3, 2, 6, 1, select_text);
        write_str_to_tile_array(select_text, 6, D16_NAME);
        set_bkg_tiles(3, 4, 6, 1, select_text);
        write_str_to_tile_array(select_text, 6, D22_NAME);
        set_bkg_tiles(3, 6, 6, 1, select_text);
        write_str_to_tile_array(select_text, 6, D32_NAME);
        set_bkg_tiles(3, 8, 6, 1, select_text);

        // Hide board sprite.
        set_sprite_prop(0, S_PRIORITY);
        set_sprite_prop(1, S_PRIORITY);
        set_sprite_prop(2, S_PRIORITY);
        set_sprite_prop(3, S_PRIORITY);

        // Show select cursor.
        set_sprite_prop(8, 0);

        // Move cursor to last tracked position.
        move_select(0);
    }
    else
    {
        select_menu_open = false;

        // Hide select cursor.
        move_sprite(8, 0, 0);
        draw_current_board();

        // Show board sprite.
        set_sprite_prop(0, 0);
        set_sprite_prop(1, 0);
        set_sprite_prop(2, 0);
        set_sprite_prop(3, 0);
    }

}

// Change board cursor sprites to an offset.
void anim_sprite(uint8_t state)
{
    // Change offset based of state.
    uint8_t offset = BOARD_TILE_SCREEN_TILE_AREA * state;

    // Change sprites by offset.
    set_sprite_tile(0, offset);
    set_sprite_tile(1, offset + 1);
    set_sprite_tile(2, offset + 2);
    set_sprite_tile(3, offset + 3);
}


void handle_movement(uint8_t x_movement, uint8_t y_movement)
{
    // Move in menu or game if possible.
    if (select_menu_open)
    {
        move_select(y_movement);
    }
    else
    {
        move_cursor(x_movement, y_movement);
    }
}

// Checks user input.
void handle_input()
{
    // Singular call to IO for performance.
    uint8_t joypad_status = joypad();

    // Direction input handling.
    time_t current_time = clock();
    int8_t x_movement = -!!(joypad_status & J_LEFT) + !!(joypad_status & J_RIGHT);
    int8_t y_movement = -!!(joypad_status & J_UP  ) + !!(joypad_status & J_DOWN );
    if ((x_movement || y_movement) && ((current_time - movement_time) / 13 > 0))
    {
        movement_time = current_time;
        handle_movement(x_movement, y_movement);
    }

    // If start is pressed.
    if (joypad_status & J_START)
    {
        // Set difficulty, close the menu and restart.
        if (select_menu_open)
        {
            select();
        }

        set_difficulty(select_param);
    }

    // If select is pressed.
    if (joypad_status & J_SELECT)
    {
        if (!select_clicked)
        {
            select_clicked = true;
            select();
        }
    }
    else
    {
        select_clicked = false;
    }

    // Click A.
    if (joypad_status & J_A)
    {
        if (!a_clicked && board_manipulation_enabled && !select_menu_open)
        {
            if (!initialized_bombs)
            {
                initialize_bombs();
            }

            click_tile(cursor_board_x(), cursor_board_y());

            a_clicked = true;
        }
    }
    else
    {
        a_clicked = false;
    }

    // Click B.
    if (joypad_status & J_B)
    {
        // If wasn't clicked.
        if (!b_clicked && board_manipulation_enabled && !select_menu_open)
        {
            b_clicked = true;

            flag_tile(cursor_board_x(), cursor_board_y());
        }
    }
    else
    {
        b_clicked = false;
    }
}


void init()
{
    // No sound is used.
    // NR52_REG = 0x8F;	// Turn on the sound.
    // NR51_REG = 0x11;	// Enable the sound channels.
    // NR50_REG = 0x77;	// Increase the volume to its max.

    // GBC Support.
    if (_cpu == CGB_TYPE)
    {
        // Setting up CGB palettes, 0 is used as default and overrides it as well.
        set_bkg_palette(0, 8, MINE_TILES_CGB_PALETTE);
        // Sprite Palette.
        set_sprite_palette(0, 1, SPRITE_PALETTE);
    }

    // Init map.
    set_bkg_data(0  , 44, mine_tile_sheet_data); // Load Minetiles to memory.
    set_bkg_data(44 , 96, ChicagoFont_data    ); // Load ChicagoFont to memory, 96 tiles.
    set_bkg_data(140, 9 , ui_background_data  ); // Load ui background to memory, 9 tiles.

    // Initialize cursor sprite.
    set_sprite_data(0, NUM_CURSOR_SPRITES, sprites_data);
    set_sprite_data(NUM_CURSOR_SPRITES, 1, mode_selector_data);

    // Setting cursor sprites to sprite memory.
    for (uint8_t sprite_index = 0; sprite_index <= NUM_CURSOR_SPRITES; sprite_index++)
    {
        set_sprite_tile(sprite_index, sprite_index);
    }

    set_sprite_prop(8, S_PRIORITY);

    // Init UI Background.
    set_bkg_tiles(12, 0, 8, 18, ui_background_map);

    // Write constant text to UI (+1 for null terminator).
    uint8_t ui_consts[7];

    write_str_to_tile_array(ui_consts, 6, "Status");
    set_bkg_tiles(13, 1, 6, 1, ui_consts);

    write_str_to_tile_array(ui_consts, 4, "Time");
    set_bkg_tiles(13, 5, 4, 1, ui_consts);

    write_str_to_tile_array(ui_consts, 5, "Flags");
    set_bkg_tiles(13, 9, 5, 1, ui_consts);

    write_str_to_tile_array(ui_consts, 6, "Scroll");
    set_bkg_tiles(13, 14, 6, 1, ui_consts);

    // Init board. Important to do after UI init so Status will be written.
    select_param = 0;
    set_difficulty(select_param);

    DISPLAY_ON; // Turn on the display.
}

// Animate board sprite based on frames passed.
void set_sprite_anim()
{
    time_t current_time = clock();
    anim_sprite((current_time / 50) % 2);
}

void main()
{
    init();
    update_switches();

    // Game loop.
    while(true)
    {
        // Handle game logic.
        set_sprite_anim();

        handle_input();
        update_ui();

        // Wait for frame to render to screen, iirc.
        wait_vbl_done();
    }
}
