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
 * - None for now!
 * Roadmap:
 *  - Add title screen that leads to diff select (might take a while, need a fullscreen image)
 *  - Add music (Probably not going to happen because I don't have a track)
 *  - Add gameboy color colors, plan is to make every numbered tile have a different color (Done! ^_^ )
 *  - Invert font (Done! ^_^ )
 *  - Rework direction input handling (Done! ^_^ )
 */

// Declaring function as it and click_tile call eachother
void click_tile(uint8_t, uint8_t);

typedef struct {
    uint32_t is_revealed : 1;  // Is tile revealed to the player
    uint32_t is_bomb : 1;  // Is the tile a bomb
    uint32_t is_flagged: 1;  // Does it have a flag in it
} board_tile_t; // Information stored on each tile
// constants
// CGB Palettes
const uint16_t mine_tiles_cgb_palette[] = {
    RGB_WHITE, RGB_DARKGRAY, RGB_LIGHTGRAY, RGB_BLACK, // Default palette
    RGB_WHITE, RGB_DARKGRAY, RGB_BLUE, RGB_BLACK,  // 1 neighbour
    RGB_WHITE, RGB_DARKGRAY, RGB_GREEN, RGB_BLACK,  // 2 neighbours
    RGB_WHITE, RGB_DARKGRAY, RGB_RED, RGB_BLACK,  // 3 neighbours
    RGB_WHITE, RGB_DARKGRAY, RGB_PURPLE, RGB_BLACK,  // 4 neighbours
    RGB_WHITE, RGB_DARKGRAY, RGB_DARKRED, RGB_BLACK,  // 5 neighbours
    RGB_WHITE, RGB_DARKGRAY, RGB_CYAN, RGB_BLACK,  // 6 neighbours
    RGB_WHITE, RGB_DARKGRAY, RGB_PURPLE, RGB_BLACK,  // 7 neighbours
    };

// Sprite palette (only one is required and used)
const uint16_t sprite_palette[] = {
    RGB_BLUE, RGB_WHITE, RGB_LIGHTGRAY, RGB_BLACK
};

const uint8_t font_start = 0x32;  // Tile font start at this tile
const uint8_t ui_background_start = 0x92;  // UI background tiles start
const uint8_t bomb_indicator = 10;  // Identifies tile as a bomb in bombsNear

const uint8_t num_rows = 9;  // Number of Y positions screen can show (Not including UI)
const uint8_t num_cols = 6;  // Number of X positions screen can show (Not including UI)

const uint8_t origin_x_offset = 8;  // Gameboy sprite offset from top left in x axis
const uint8_t origin_y_offset = 16;  // Gameboy sprite offset from top left in y axis

const uint8_t board_tile_pixels_length = 16;  // Length (in pixels) of meta-tiles used.
const uint8_t board_tile_screen_tiles_length = 2;  // Length (in pixels) of meta-tiles used.
const uint8_t board_tile_screen_tile_area = 4;  // Area (in area of screen tiles) of meta-tiles used.
const uint8_t screen_tile_pixels_length = 8;  // Length (in pixels) of screen tiles.


const uint8_t flag_offset = 11;  // offset of flag tile from mineTiles
// Difficulty names
const char d10_name[] = "Easy  ";  // Easy string name
const char d16_name[] = "Inter ";  // Intermediate string name
const char d22_name[] = "Expert";  // Expert string name
const char d32_name[] = "NiMare";  // Nightmare string name

// Typed vars
uint8_t time_passed[5];  // UI tile string value of how much time passed since game start
const uint8_t time_passed_length = 5;  // length of time_passed
uint8_t game_status[6]; // UI tile string value of the game's status (diff, won, lost)
const uint8_t game_status_length = 6;  // length of game status
uint8_t flags_used[8];  // UI tile string value of how much flags are left out of total
const uint8_t flags_used_length = 8;  // length of flag used string
uint8_t scroll_show[5];  // UI tile string value of current string scroll
const uint8_t scroll_show_length = 5;  // length of scroll status string

uint8_t time_overflow_tracker = 0;
time_t start_time;  // Game start time, to track in UI
time_t previous_time_since;
bool input_enabled;  // Will player input move the sprite
bool a_clicked;  // Is a clicked
bool select_clicked;  // Is select clicked
bool b_clicked;  // Is B clicked
bool moving_dir; // Is moving
bool select_menu_open;  // Is the difficulty selection menu open
int8_t select_param;  // What board size is selected
uint8_t scroll_state[2];  // Board offset from screen
uint8_t cursor[2];  // Player location on board screen, (x, y)
uint8_t board_size;  // Root of the number of tiles on the board, limited due to RAM.
uint16_t bombs_num;  // Number of bombs on the board
uint16_t flags_num;  // Number of flags left
uint16_t flag_balance;  // Bombs unflagged
bool isFirstClick;  // Should generate bombs on click
board_tile_t *board_tiles = 0;  // Information about tiles on the board, size of BoardNum^2

// Helper functions
// Takes a string (str) and writes it's ascii value as tiles to another array (array), at the specified size (array_size)
void write_str_to_tile_array(uint8_t* array, uint8_t array_size, const char* str) {
    for (uint8_t i = 0; i < array_size; i++) {
        if (str[i] == 0) break;
        array[i] = str[i] - ' ' + font_start;
    }
}

// For winning and losing
void game_over(bool isWin) {
    // Update status
    write_str_to_tile_array(game_status, game_status_length, isWin ? "Win   " : "Lose  ");
    // Display status
    set_bkg_tiles(13, 3, 6, 1, game_status);
    // Disable input
    input_enabled = 0;
}

// Takes 2D coordinates and flattens them to 1D
uint16_t flatten_coords(uint8_t x, uint8_t y, uint8_t row_size) {
    return ((uint16_t)y) * row_size + x;
}

// checks if given cooridnates are in the board
bool coords_in_board(int8_t x, int8_t y) {
    return 	!(x < 0 || y < 0 || x >= (uint16_t) board_size || y >= (uint16_t) board_size);
}


// Actual position on board (unlike cursor which is graphical position)
uint8_t cursor_board_x() {
    return cursor[0] + scroll_state[0];
}
uint8_t cursor_board_y() {
    return cursor[1] + scroll_state[1];
}

// Finds the amount of bombs near a tile (technically including itself)
// bomb_indicator if tile is bomb
uint8_t bombsNearTile(uint8_t tile_x, uint8_t tile_y) {
    // If the tile is a bomb return bomb_indicator
    if (board_tiles[flatten_coords(tile_x, tile_y, board_size)].is_bomb) return bomb_indicator;
    // Loop over nearby tiles and find bombs
    uint8_t numBombs = 0;
    for (short i = -1; i < 2; i++) {
        for (short j = -1; j < 2; j++) {
            if (!coords_in_board(tile_x + i, tile_y + j)) continue;
            numBombs += board_tiles[flatten_coords(i + tile_x, j + tile_y, board_size)].is_bomb;
        }
    }
    return numBombs;
}

// Draws a board tile (tile_num) to a specified location (x, y)
void reveal_tile(uint8_t x, uint8_t y, int8_t tile_num) {
    // Check if coordinates are in the graphical part of the screen
    if (x >= num_cols || y >= num_rows) return;
    // CGB specific palette swap to color tiles
    if (_cpu == CGB_TYPE) {
        uint8_t palette_ind = tile_num;
        // If empty. I know can be simplified to smaller and greater then but more intuitive this way
        if (tile_num == 0 || tile_num == -1) {
            // Regular palette if unique
            palette_ind = 0;
        }
        // 8 uses default palette colors
        else if (tile_num == 8) {
            palette_ind = 0;
        }
        // Check if flag
        else if ((short)tile_num == flag_offset) {
            // Red palette for flag
            palette_ind = 3;
        }
        // Check if bomb
        else if ((short)tile_num == bomb_indicator) {
            palette_ind = 3;
        }
        // Write meta_data (tile palette is bits 0-2)
        VBK_REG = 1;
        // Setting number of tiles at once didn't work so for loop to loop over all 4 scren tiles
        for (uint8_t tile_ind = 0; tile_ind < board_tile_screen_tile_area; tile_ind++) {
            // Same bug as in move_sprite_grid
            set_bkg_tile_xy(x * board_tile_screen_tiles_length + tile_ind % 2, y * board_tile_screen_tiles_length + (int)tile_ind / 2, palette_ind);
        }
        // Back to write regular data
        VBK_REG = 0;
    }
    // Draw background with given tile, offset is board_tile_screen_tile_area * tile_num and starts from board_tile_screen_tile_area
    set_bkg_tiles(2 * x, 2 * y, 2, 2, mine_tile_sheet_map + board_tile_screen_tile_area + board_tile_screen_tile_area * tile_num);
}

// Draws the whole board, factoring in the screen scroll
void draw_current_board() {
    for (uint16_t i = 0; i < num_cols; i++) {
        // The X position of the drawn tiles with scroll
        uint8_t new_x = i + scroll_state[0];

        for (uint8_t j = 0; j < num_rows; j++) {
            // The y position of the drawn tile with scroll
            uint8_t new_y = j + scroll_state[1];
            // vram offset
            int8_t tile_offset = -1;
            if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_flagged) {
                // set offset to flag tile
                tile_offset = flag_offset;
            }
            else if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_revealed) {
                // find bombs near
                uint8_t bombs_near = bombsNearTile(new_x, new_y);
                // set to offset to bombs near
                tile_offset = bombs_near;
            }
            reveal_tile(i, j, (int8_t)tile_offset);
        }
    }
}

// Movement
// Scrolls the board and redraws *all* tiles, no bkg_scroll functionality used. Doesn't warp.
void scroll_board(int8_t x, int8_t y) {
    // Tracking if screen was scrolled (logically) to know if to draw board
    bool has_scrolled = 0;
    // Check conditions for horizontal scroll
    if ((scroll_state[0] < board_size - num_cols && x > 0) || (scroll_state[0] > 0 && x < 0)) {
        // Track scroll
        scroll_state[0] += x;
        // Draw when should
        has_scrolled = 1;
    }
    // Check conditions for vertical scroll
    if ((board_size - num_rows > scroll_state[1] && y > 0) || (scroll_state[1] > 0 && y < 0)) {
        // Track scroll
        scroll_state[1] += y;
        // Draw when should
        has_scrolled = 1;
    }
    // If didn't scroll return
    if (has_scrolled == 0) return;
    // If scrolling happened, redraw
    draw_current_board();
}

void move_sprite_grid(uint8_t new_x, uint8_t new_y) {
    // Different off-sets because of Gameboy Schenanigans
    for (uint8_t sprite_ind = 0; sprite_ind < board_tile_screen_tile_area; sprite_ind++) {
        // I want to swap the literal "8" with the variable "screen_tile_pixel_length" but even though they are the same value it doesn't work. Bit shifting by the correct amount (3) doesn't work as well.
        // This is a a known and documented bug in lcc, although I can't find it now.
        move_sprite(sprite_ind, new_x * board_tile_pixels_length + origin_x_offset + 8 * (sprite_ind % 2), new_y * board_tile_pixels_length + origin_y_offset + 8 * ( (int)sprite_ind / 2));

    }
}


void move_cursor(int8_t x, int8_t y) {
    // Check if player wants to scroll
    int8_t scroll_x = 0;
    int8_t scroll_y = 0;
    // Check scroll conditions
    if (cursor[0] == 0 && x < 0) {
        scroll_x = x;
    }
    else if (cursor[1] == 0 && y < 0) {
        scroll_y = y;
    }
    else if (cursor[0] == num_cols - 1 && x > 0) {
        scroll_x = x;
    }
    else if (cursor[1] == num_rows - 1 && y > 0) {
        scroll_y = y;
    }
    // Move
    else {
        cursor[0] += x;
        cursor[1] += y;
        // Draw player grid
        move_sprite_grid(cursor[0], cursor[1]);
        // Return if moved
        return;
    }
    scroll_board(scroll_x, scroll_y);
    // Check boundaries (with cursor wrapping)
}

// Finds the amount of flags near a tile (not including itself)
uint8_t flagsNearTile(uint8_t tile_x, uint8_t tile_y) {
    // Track number of flags
    uint8_t numFlags = 0;
    // Loop over neighbores
    for (short i = -1; i < 2; i++) {
        for (short j = -1; j < 2; j++) {
            // Check if neighbour is flag
            if (!coords_in_board(tile_x + i, tile_y + j) ||  i == 0 && j == 0) continue;
            // Track flags near
            numFlags += board_tiles[flatten_coords(i + tile_x, j + tile_y, board_size)].is_flagged;
        }
    }
    return numFlags;
}

// Reveal nearby tiles
void reveal_nearby(uint8_t x, uint8_t y) {
    // Loop over neighbores
    for (short i = -1; i < 2; i++) {
        for (short j = -1; j < 2; j++) {
            // If tile is not in board or is iteslf continue to next one
            if (!coords_in_board(x + i, y + j) ||  i == 0 && j == 0) continue;
            // If the tile is flagged or was already revealed continue
            if (board_tiles[flatten_coords(i + x, j + y, board_size)].is_flagged || board_tiles[flatten_coords(i + x, j + y, board_size)].is_revealed) continue;
            // Reveal tile by simulating clicking on it
            click_tile(x + i, y + j);
        }
    }
}

// Click on tile, revealing it if possible
void click_tile(uint8_t x, uint8_t y) {
    // If the wanted tile is flagged return
    if (board_tiles[flatten_coords(x, y, board_size)].is_flagged) return;
    // Find the amount of bombs near the tile
    uint8_t bombsNear = bombsNearTile(x, y);
    // If the tile is a bomb
    if (bombsNear == bomb_indicator) {
        // Lose
        game_over(0);
    }
    // If the tile was not previously revealed
    if (board_tiles[flatten_coords(x, y, board_size)].is_revealed == 0) {
        // Set it to revealed
        board_tiles[flatten_coords(x, y, board_size)].is_revealed = 1;
        // If no bombs are nearby autoreveal all nearby tiles
        if (bombsNear == 0)  reveal_nearby(x, y);
    }
    // If the amount of flags nearby equals the number of a tile (and it isn't empty) reveal all nearby tiles
    if (flagsNearTile(x, y) == bombsNear && bombsNear != 0) reveal_nearby(x, y);  // Will auto-reveal numbered tiles with enough flags, this is not a wanted behaviour but I don't have a way to efficently change it.
    // Re-draw clicked tile to show revealed status
    reveal_tile(x - scroll_state[0], y - scroll_state[1], bombsNear);
}

// Take a number (8-bit, num) and write it to an array (array) from index arr_start to arr_end
void number_to_chars(uint8_t *array, uint8_t arr_start, uint8_t arr_end, unsigned long num) {
    for (uint8_t neg_ind = 1; neg_ind <= arr_end - arr_start; neg_ind++) {
        // Set index to last digit in tile structuring
        array[arr_end - neg_ind] = num % 10 + font_start + '0' - ' ';
        // Alter number
        num = num / 10;
    }
}

// Change board size and number of bombs, functions as a reset
void set_board_size(uint8_t new_size, uint8_t num_bombs) {
    // Track time
    start_time = clock();
    // Resetting previous time
    previous_time_since = 0;
    // Resetting time overflow tracker
    time_overflow_tracker = 0;
    // Track board size
    board_size = new_size;
    // Track number of bombs
    bombs_num = num_bombs;
    // Set available flags
    flags_num = num_bombs;
    // Set unflagged bombs
    flag_balance = num_bombs;
    // Init buttons
    a_clicked = 0;
    select_clicked = 0;
    b_clicked = 0;
    moving_dir = 0;
    input_enabled = 1;
    // menus
    select_menu_open = 0;
    // Initialize changing ui
    write_str_to_tile_array(time_passed, time_passed_length, "00000");
    write_str_to_tile_array(flags_used, flags_used_length, "000//000");
    write_str_to_tile_array(scroll_show, scroll_show_length, "00/00");
    // Initialize cursor
    cursor[0] = 0;
    cursor[1] = 0;
    move_sprite_grid(cursor[0], cursor[1]);
    // Init scroll
    scroll_state[0] = 0;
    scroll_state[1] = 0;
    // updating flag string
    number_to_chars(flags_used, 5, 8, flags_num);
    // Freeing used tile memory
    free(board_tiles);
    // Reset board data to zero
    board_tiles = malloc(board_size * board_size);
    for (uint16_t i=0; i < (uint16_t)(board_size * board_size); i++){
        board_tiles[i].is_bomb = 0;
        board_tiles[i].is_revealed = 0;
        board_tiles[i].is_flagged = 0;
    }
    // Bombs were not initialized and will be at next click
    isFirstClick = 1;
    // Re-draw board
    draw_current_board();
}

// First player click, initializing bombs
void first_tile() {
    // If Game Boy Color
    if (_cpu == CGB_TYPE) {
        // Action is long and stops progress, fast processing is worth it
        cpu_fast();
    }
    // Set first click to pressed
    isFirstClick = 0;
    // Bomb generation script - Uses Reservoir sampling to create the random locations
    // Getting the number of protected tiles
    uint8_t player_x = cursor_board_x();  // Player board position X
    uint8_t player_y = cursor_board_y();  // Player board position Y

    // This method requires an array to save the locations in. This wastes quite a lot of memory but is much more efficient
    uint16_t* bomb_locations;
    // Formatting the array to the correct size
    bomb_locations = malloc(bombs_num);
    uint16_t protected_modifier = 0;
    // Setting the starting values to the location array
    for (uint32_t i = 0; i < bombs_num; i++) {
        // Checking if the tile is protected. If it is this tile will be skipped (with the incrementation of protected_modifier) and so on.
        while (abs((short)player_x - (i + protected_modifier) % board_size) <= 1 && abs((short)player_y - (i + protected_modifier) / board_size) <= 1) {
            protected_modifier++;
        }
        bomb_locations[i] = i + protected_modifier;
    }
    // Iterating over every other available number
    for (uint32_t i = bombs_num; i + protected_modifier < board_size * board_size; i++) {
        // Checking if its protected
        while (abs((short)player_x - (i + protected_modifier) % board_size) <= 1 && abs((short)player_y - (i + protected_modifier) / board_size) <= 1) {
            protected_modifier++;
        }
        // Replacing it with a previously in number
        uint32_t j = rand() % i;
        if (j < bombs_num) {
            bomb_locations[j] = i + protected_modifier;
        }
    }
    // Iterating over the chosen numbers in order to insert them to the board matrix
    for (int i = 0; i < bombs_num; i++) {
        board_tiles[bomb_locations[i]].is_bomb = 1;
    }
    // DEBUG - bomb view
    //	for (uint16_t tile_i = 0; tile_i < (uint16_t)board_size * board_size; tile_i++) {
    //        set_bkg_tiles(tile_i / board_size, tile_i % board_size, 1, 1, nearTiles + board_tiles[tile_i].is_bomb);
    //    }
    // End DEBUG
    // If Game Boy Color
    if (_cpu == CGB_TYPE) {
        // Stop fast processing
        cpu_slow();
    }
    // Click tile
    click_tile(player_x, player_y);
}

// Tile flag status is changed due to a click
void flag_tile(uint8_t x, uint8_t y) {
    // Find clicked tile
    board_tile_t *tile_p = &board_tiles[flatten_coords(x, y, board_size)];
    // If bombs were not placed yet or tile is revealed return
    if (isFirstClick || tile_p->is_revealed) return;
    // Check if tile is bomb
    bool is_bomb = tile_p->is_bomb;
    // If tile is flagged
    if (tile_p->is_flagged) {
        // Set it not to be
        tile_p->is_flagged = 0;
        // Re-draw tile
        reveal_tile(x - scroll_state[0], y - scroll_state[1], -1);
        // Flag is available to be re-used
        flags_num++;
        // If tile contains a bomb increment number of unflagged bombs
        if (is_bomb) flag_balance++;
    }
    // If tile isn't flag
    else {
        // If no flags are available return
        if (flags_num <= 0) return;
        // Set tile to flagged
        tile_p->is_flagged = 1;
        // Re-draw tile
        reveal_tile(x - scroll_state[0], y - scroll_state[1], 11);
        // Decrement number of flags
        flags_num--;
        // If player is right decrement number of unflagged bombs
        if (is_bomb) flag_balance--;
        // if no unflagged bombs are left
        if (flag_balance == 0) {
            // Win
            game_over(1);
        }
    }
}

//.show backround and sprites
void updateSwitches() {
    SHOW_BKG;
    SHOW_SPRITES;
}

// Update ui to show current status
void update_ui() {
    // Update time
    if (input_enabled) {
        // Track time
        time_t time_since = clock() - start_time;
        // If the current time passed is smaller than previously, an overflow had occurred.
        if (time_since < previous_time_since) {
            // Logging the overflow
            time_overflow_tracker += 1;
        }
        // Log the current time passed for the future
        previous_time_since = time_since;
        // Write time to string - The cast to long is in order to pass the 16 bit overflow the clock has, I then add the
        // current time in clock cycles and get the seconds as unsigned longs in it.
        // The cast to uint16_t is a lazy way to verify the time will never demand more than 5 characters, which
        // Will be more than is allocated to it on the screen. Time caps at 65546 seconds, and I'm fine with it.
        number_to_chars(time_passed, 0, 5, (uint16_t)(((unsigned long)time_overflow_tracker * USHRT_MAX + time_since) / CLOCKS_PER_SEC));
        // Draw string
        set_bkg_tiles(13, 7, 5, 1, time_passed);
    }
    // Update flags left
    number_to_chars(flags_used, 0, 3, flags_num);
    set_bkg_tiles(13, 11, 4, 2, flags_used);
    // Update scroll status
    // X
    number_to_chars(scroll_show, 0, 2, scroll_state[0]);
    // Y
    number_to_chars(scroll_show, 3, 5, scroll_state[1]);
    set_bkg_tiles(13, 16, 5, 1, scroll_show);
}

// Moves the select menu cursor
void move_select(int8_t y) {
    // Doesn't do anything if menu is closed
    if (!select_menu_open) return;
    // Change tracking parameter
    select_param += y;
    // Manages scope
    if (select_param > 3) select_param = 0;
    if (select_param < 0) select_param = 3;
    // Moves
    move_sprite(8, 24, 32 + select_param * 16);
}

// 0: Easy, 1: Inter, 2: Expert, 3: NiMare
void set_difficulty(uint8_t new_diff) {
    switch (new_diff)
    {
        case 0:  // Easy
            write_str_to_tile_array(game_status, 6, d10_name);
            set_board_size(10, 10);
            break;
        case 1:  // Intermediate
            write_str_to_tile_array(game_status, 6, d16_name);
            set_board_size(16, 40);
            break;
        case 2:  // Expert
            write_str_to_tile_array(game_status, 6, d22_name);
            set_board_size(22, 99);
            break;
        case 3:  // Nightmare
            write_str_to_tile_array(game_status, 6, d32_name);
            set_board_size(32, 200);
            break;
        default:  // Nothing
            set_difficulty(0);
    }
    // Draw status
    set_bkg_tiles(13, 3, 6, 1, game_status);
}

void select() {
    // Check if menu open
    if (select_menu_open == 0) {
        // When not set it to open
        select_menu_open = 1;
        // Disable palettes if GBC
        if (_cpu == CGB_TYPE) {
            // Write metadata
            VBK_REG = 1;
            // Loop over all select menu tiles
            for (uint8_t tile_x = 0; tile_x < 8; tile_x++) {
                for (uint8_t tile_y = 0; tile_y < 18; tile_y++) {
                    // Set palette to zero
                    set_bkg_tile_xy(2 + tile_x, tile_y, 0);
                }
            }
            // Write regualr
            VBK_REG = 0;
        }

        // Draw menu background
        set_bkg_tiles(2, 0, 8, 18, ui_background_map);
        // Write text to background using one array
        uint8_t select_text[6];
        // Write East diff
        write_str_to_tile_array(select_text, 6, d10_name);
        set_bkg_tiles(3, 2, 6, 1, select_text);
        // Write Inter diff
        write_str_to_tile_array(select_text, 6, d16_name);
        set_bkg_tiles(3, 4, 6, 1, select_text);
        // Write Expert diff
        write_str_to_tile_array(select_text, 6, d22_name);
        set_bkg_tiles(3, 6, 6, 1, select_text);
        // Write NiMare diff
        write_str_to_tile_array(select_text, 6, d32_name);
        set_bkg_tiles(3, 8, 6, 1, select_text);
        // Hide board sprite
        set_sprite_prop(0, S_PRIORITY);
        set_sprite_prop(1, S_PRIORITY);
        set_sprite_prop(2, S_PRIORITY);
        set_sprite_prop(3, S_PRIORITY);
        // Show select cursor
        set_sprite_prop(8, 0);
        // Move cursor to last tracked position
        move_select(0);

    }
    else {
        // Track menu as closed
        select_menu_open = 0;
        // Move select cursor to hidden origin
        move_sprite(8, 0, 0);
        // Draw board to remove drawing of selection menu
        draw_current_board();
        // Show board sprite
        set_sprite_prop(0, 0);
        set_sprite_prop(1, 0);
        set_sprite_prop(2, 0);
        set_sprite_prop(3, 0);
    }

}

// Change board cursor sprites to an offset
void anim_sprite(uint8_t state) {
    // Change offset based of state
    uint8_t offset = 0;
    if (state == 1) offset = 4;
    // Change sprites by offset
    set_sprite_tile(0, offset);
    set_sprite_tile(1, offset + 1);
    set_sprite_tile(2, offset + 2);
    set_sprite_tile(3, offset + 3);
}

// Checks user input, should be changed as input is clunky
void checkInput() {
    // Timed calls
    // Input slower is a repeating timer to slow down movement
    static uint8_t inputSlower = 0;
    // Increment timer
    inputSlower++;
    // Animate board sprite based on frames passed
    if (inputSlower % 10 == 0) anim_sprite(inputSlower % 20 / 10);
    // If start is pressed
    if (joypad() & J_START) {
            // If select menu isn't open restart to current diff
            if (!select_menu_open) set_difficulty(select_param);
            // If select menu is open
            else {
                // Close select menu
                select();
                // Change difficulty
                set_difficulty(select_param);
            }
    }
    // If select is pressed
    if (joypad() & J_SELECT) {
        // If clicked too soon return
        if (select_clicked) return;
        // Track click
        select_clicked = 1;
        // Update select screen status
        select();
    }
    // Avoid continous click
    else select_clicked = 0;
    // Direction input handling
    // Avoid clicking to quickly
    if (!moving_dir || (inputSlower - moving_dir) % 4 == 0) {
        // If up is clicked
        if (joypad() & J_UP) {
            // If can move
            if (input_enabled || select_menu_open) {
                // Move up
                move_cursor(0, -1);
                // Track movement
                moving_dir = inputSlower;
            }
            // If select menu is open
            if (select_menu_open) {
                // Move select cursor up
                move_select(-1);
                // Track movement
                moving_dir = inputSlower;
            }
        }
        // If moves down
        if (joypad() & J_DOWN) {
            // If can move
            if (input_enabled || select_menu_open) {
                // Move down
                move_cursor(0, 1);
                // Track movement
                moving_dir = inputSlower;
            }
            // If select menu is open
            if (select_menu_open) {
                // Move cursor select down
                move_select(1);
                // Track movement
                moving_dir = inputSlower;
            }


        }
        // If can't move on board return (as left and right don't matter otherwise)
        if (!input_enabled || select_menu_open) return;
        // If left is pressed
        if (joypad() & J_LEFT) {
            // Move left
            move_cursor(-1, 0);
            // Track movemenet
            moving_dir = inputSlower;

        }
        // If right is pressed
        if (joypad() & J_RIGHT) {
            // Move right
            move_cursor(1, 0);
            // Track movement
            moving_dir = inputSlower;
        }
    }
    // Not moving
    if (!(joypad() & J_RIGHT || joypad() & J_LEFT || joypad() & J_UP || joypad() & J_DOWN)) moving_dir = 0;

    // Click A
    if (joypad() & J_A) {
        // If not already clicked
        if (!a_clicked) {
            // Clicked
            a_clicked = 1;
            // If first click update bombs, if not click on cursor location in board
            if (isFirstClick) first_tile();
            else click_tile(cursor_board_x(), cursor_board_y());
        }
    }
    // If not clicked then not clicked
    else a_clicked = 0;
    // click B
    if (joypad() & J_B) {
        // If wasn't clicked
        if (!b_clicked) {
            // Clicked
            b_clicked = 1;
            // Flag cursor tile
            flag_tile(cursor_board_x(), cursor_board_y());
        }
    }
    // If not clicked then not clicked
    else b_clicked = 0;
}


void init() {
    NR52_REG = 0x8F;	// Turn on the sound
    NR51_REG = 0x11;	// Enable the sound channels
    NR50_REG = 0x77;	// Increase the volume to its max
    // GBC Support
    if (_cpu == CGB_TYPE) {
        // Setting up CGB palettes, 0 is used as default and overrides it as well
        set_bkg_palette(0, 8, mine_tiles_cgb_palette);
        // Sprite Palette
        set_sprite_palette(0, 1, sprite_palette);
    }
    // Init map
    set_bkg_data(0, 49, mine_tile_sheet_data);	// Load Minetiles to memory
    set_bkg_data(50, 96, ChicagoFont_data);	// Load ChicagoFont to memory, 96 tiles
    set_bkg_data(146, 9, ui_background_data);	// Load ui background to memory, 96 tiles
    // Initialize cursor sprite
    uint8_t num_cursor_sprites = 8;
    set_sprite_data(0, num_cursor_sprites, sprites_data);
    set_sprite_data(num_cursor_sprites, 1, mode_selector_data);
    // Setting cursor sprites to sprite memory
    for (uint8_t i = 0; i <= num_cursor_sprites; i++) {
        set_sprite_tile(i, i);
    }
    set_sprite_prop(8, S_PRIORITY);
    // Init UI
    // Background
    set_bkg_tiles(12, 0, 8, 18, ui_background_map);
    // Text

    uint8_t ui_consts[6];
    // Status
    write_str_to_tile_array(ui_consts, 6, "Status");
    set_bkg_tiles(13, 1, 6, 1, ui_consts);

    // time
    write_str_to_tile_array(ui_consts, 4, "Time");
    set_bkg_tiles(13, 5, 4, 1, ui_consts);

    // Flags
    write_str_to_tile_array(ui_consts, 5, "Flags");
    set_bkg_tiles(13, 9, 5, 1, ui_consts);

    // Scroll
    write_str_to_tile_array(ui_consts, 6, "Scroll");
    set_bkg_tiles(13, 14, 6, 1, ui_consts);

    // Init board (Important to do after UI init so Status will be written. Cost me like half an hour)
    select_param = 0;
    set_difficulty(select_param);
    DISPLAY_ON; // Turn on the display
}

// Main function that is run
void main() {

    // Initialize gameboy and other variables
    init();

    // Game loop
    while(1) {

        // Check for player input
        checkInput();
        // Update gameboy switches
        updateSwitches();
        // Something (not sure oops)
        wait_vbl_done();
        // Update UI
        update_ui();
    }
}
