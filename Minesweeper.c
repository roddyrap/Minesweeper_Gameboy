#include <gb/gb.h>
#include <stdlib.h>
#include "rand.h"

#include "Tiles/mine_tile_sheet_data.c"
#include "Tiles/mine_tile_sheet_map.c"
#include "Tiles/sprites_data.c"
#include "Tiles/sprites_map.c"

// TODO: Can't click on tiles if scrolled, isn't aligned properly with rest of the code

typedef struct {
    unsigned int is_revealed : 1;
    unsigned int is_bomb : 1;
} board_tile;

// Last funs decleration
void init();
void checkInput();
void anim_sprite(UINT8);
UINT8 bombsNearTile(UINT8, UINT8);

UINT8 num_rows = 9;  // Number of Y positions screen can show (Not including WIN) 
UINT8 num_cols = 10;  // Number of X positions screen can show (Not including WIN)
UINT8 scroll_state[2];  // Board offset from screen
UINT8 cursor[2];  // Player location on board screen, (x, y)
UINT8 board_size;  // Root of the number of tiles on the board, limited due to RAM.
UINT16 bombs_num;  // Number of bombs on the board
BOOLEAN isFirstClick;  // Should generate bombs on click
board_tile *board_tiles;  // Information about tiles on the board, size of BoardNum^2
// Helper functions
UINT16 flatten_coords(UINT8 x, UINT8 y, UINT8 row_size) {
	return ((UINT16)y) * row_size + x;
}

BOOLEAN coords_in_board(INT8 x, INT8 y) {
	return 	!(x < 0 || y < 0 || x >= (UINT16) board_size || y >= (UINT16) board_size);
}
// Actual position on board
UINT8 cursor_board_x() {
	return cursor[0] + scroll_state[0];
}
UINT8 cursor_board_y() {
	return cursor[1] + scroll_state[1];
}
// Movement
// Scrolls the board and redraws *all* tiles, no bkg_scroll functionality used. 1-st bit for scrolled vertically, 2-nd bit for scrolled horizontally
// Doesn't warp.
// IS BROKEN.
UINT8 scroll_board(INT8 x, INT8 y, UINT8 amount) {
	UINT8 res = 0;
	
	if ((board_size - num_cols > scroll_state[0] && x == 1) || (scroll_state[0] > 0 && x == -1)) {
		scroll_state[0] += x * amount;
		res += 1;
	}
	if ((board_size - num_rows > scroll_state[1] && y == 1) || (scroll_state[1] > 0 && y == -1)) {
		scroll_state[1] += y * amount;
		res += 2;
	}
	if (res == 0) return 0;
	// If scrolling happened, redraw
	for (UINT16 i = 0; i < num_cols; i++) {
		UINT8 new_x = i + scroll_state[0];
		
		for (UINT8 j = 0; j < num_rows; j++) {
			UINT8 new_y = j + scroll_state[1];
			UINT8 tile_offset = 0;
			if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_revealed) {
				UINT8 bombs_near = bombsNearTile(new_x, new_y);
				tile_offset = 4 + 4 * bombs_near;
			}
			set_bkg_tiles(2 * i, 2 * j, 2, 2, mine_tile_sheet_map + tile_offset);
		}
	}
	return res;
}

void move_sprite_grid(UINT8 new_x, UINT8 new_y) {
	// Different off-sets because of Gameboy Schenanigans
	move_sprite(0, new_x * 16 + 8, new_y * 16 + 16);
	move_sprite(1, new_x * 16 + 16, new_y * 16 + 16);
	move_sprite(2, new_x * 16 + 8, new_y * 16 + 24);
	move_sprite(3, new_x * 16 + 16, new_y * 16+ 24);
}
void move_cursor(INT8 x, INT8 y, UINT8 amount) {
	// Check boundaries (with cursor wrapping)
	INT8 scroll_x = 0;
	INT8 scroll_y = 0;
	if (cursor[0] == 0 && x == -1) {
		scroll_x = -1;
	}
	else if (cursor[1] == 0 && y == -1) {
		scroll_y = -1;
	}
	else if (cursor[0] == num_cols - 1 && x == 1) {
		scroll_x = 1;
	}
	else if (cursor[1] == num_rows - 1 && y == 1) {
		scroll_y = 1;
	}
	// Move
	else {
		cursor[0] += x * amount;
		cursor[1] += y * amount;
		move_sprite_grid(cursor[0], cursor[1]); 
		return;
	}
	scroll_board(scroll_x, scroll_y, 1);
	// Check boundaries (with cursor wrapping)
}

void reveal_tile(UINT8 x, UINT8 y, UINT8 tile_num) {
	set_bkg_tiles(2 * x, 2 * y, 2, 2, mine_tile_sheet_map + 4 + 4 * tile_num);
}


// 10 if tile is bomb
UINT8 bombsNearTile(UINT8 tile_x, UINT8 tile_y) {
	if (board_tiles[flatten_coords(tile_x, tile_y, board_size)].is_bomb) return 10;
	UINT8 numBombs = 0;
	for (INT16 i = -1; i < 2; i++) {
		for (INT16 j = -1; j < 2; j++) {
			if (!coords_in_board(tile_x + i, tile_y + j) ||  i == 0 && j == 0) continue;
			numBombs += board_tiles[flatten_coords(i + tile_x, j + tile_y, board_size)].is_bomb;
		}
	}
	return numBombs;
}

void click_tile(UINT8 x, UINT8 y) {
	if (board_tiles[flatten_coords(x, y, board_size)].is_revealed) return;
	board_tiles[flatten_coords(x, y, board_size)].is_revealed = 1;  // Revealed tile, to avoid duplication code in auto-reveal which I will hopefully add
	UINT8 bombsNear = bombsNearTile(x, y);
	reveal_tile(x - scroll_state[0], y - scroll_state[1], bombsNear);
	// Auto-open if blank
	if (bombsNear != 0) return;
	for (INT16 i = -1; i < 2; i++) {
		for (INT16 j = -1; j < 2; j++) {
			if (!coords_in_board(x + i, y + j) ||  x == 0 && y == 0) continue;
			click_tile(i + x, j + y);
		}
	}
}

void set_board_size(UINT8 new_size, UINT8 num_bombs) {
	board_size = new_size;
	bombs_num = num_bombs;
	board_tiles = malloc(board_size * board_size);
	for (UINT16 i=0; i < (UINT16)(board_size * board_size); i++){
		board_tiles[i].is_bomb = 0;
		board_tiles[i].is_revealed = 0;
	}
	isFirstClick = 1;
	// Individually setting background because Idk
	for (UINT16 i = 0; i < num_cols; i++) {
		for (UINT8 j = 0; j < num_rows;j++)
			set_bkg_tiles(2 * i, 2 * j, 2, 2, mine_tile_sheet_map);
	}
}

void first_tile() {
	isFirstClick = 0;
	UINT8 player_x = cursor_board_x();
	UINT8 player_y = cursor_board_y();
	BOOLEAN on_row_edge = player_y == 0 || player_y == board_size - 1;
    BOOLEAN on_col_edge = player_x == 0 || player_x == board_size - 1;
    UINT8 protected_tiles_num = 9;
    if (on_row_edge && on_col_edge) protected_tiles_num = 4;
    else if (on_row_edge || on_col_edge) protected_tiles_num = 6;

    for (UINT16 bomb_i = 0; bomb_i < bombs_num; bomb_i++) {
        UINT16 bomb_tile_i = ((UINT16)rand() * (UINT8)rand()) % ((UINT16)board_size * board_size - protected_tiles_num - bomb_i);

        UINT16 tile_i;
        for (tile_i = 0; bomb_tile_i > 0; tile_i++) {
            if (!board_tiles[tile_i].is_bomb && !(abs((INT16)player_x - tile_i / board_size) <= 1 && abs((INT16)player_x - tile_i % board_size) <= 1)) {
                bomb_tile_i--;
            }
        }
        board_tiles[tile_i].is_bomb = 1;
    }
	// DEBUG - bomb view
    //	for (UINT16 tile_i = 0; tile_i < (UINT16)board_size * board_size; tile_i++) {
    //        set_bkg_tiles(tile_i / board_size, tile_i % board_size, 1, 1, nearTiles + board_tiles[tile_i].is_bomb);
    //    }
    // End DEBUG
	click_tile(player_x, player_y);
}


void updateSwitches() {
	SHOW_BKG;
	SHOW_SPRITES;
}


void main() {

	init();
	
	while(1) {
		
		checkInput();
		updateSwitches();
		wait_vbl_done();
	}
	
}

void init() {
	
	DISPLAY_ON;		// Turn on the display
	NR52_REG = 0x8F;	// Turn on the sound
	NR51_REG = 0x11;	// Enable the sound channels
	NR50_REG = 0x77;	// Increase the volume to its max
	// Init map
	set_bkg_data(0, 49, mine_tile_sheet_data);	// Load Minetiles to memory
	// Initialize cursor
	cursor[0] = 0;
	cursor[1] = 0;
	// Init scroll
	scroll_state[0] = 0;
	scroll_state[1] = 0;
	// Initialize cusor sprite
	UINT8 num_cursor_sprites = 8;
	set_sprite_data(0, num_cursor_sprites, sprites_data);
	// Setting cursor sprites to sprite memory
	for (UINT8 i = 0; i < num_cursor_sprites; i++) {
		set_sprite_tile(i, i);
	}
	UWORD spritePalette[] = {0, 0, 0, 0};
  	set_sprite_palette(0, 1, spritePalette);
	move_sprite_grid(cursor[0], cursor[1]);
	// Init board
	set_board_size(10, 10);
}

void checkInput() {
	// Timed calls
	static UINT8 inputSlower = 0;
	inputSlower++;
	if (inputSlower % 30 == 0) anim_sprite(inputSlower % 60 / 30);
	if (inputSlower % 4 != 0) return;
    if (joypad() & J_A) {
		if (isFirstClick) first_tile();
		else click_tile(cursor_board_x(), cursor_board_y());
    }
	if (joypad() & J_B) {

	}
	if (joypad() & J_UP) {
		move_cursor(0, -1, 1);
	}
	if (joypad() & J_DOWN) {
		move_cursor(0, 1, 1);
	}
	if (joypad() & J_LEFT) {
		move_cursor(-1, 0, 1);
	}
	if (joypad() & J_RIGHT) {
		move_cursor(1, 0, 1);
	}
}

void anim_sprite(UINT8 state) {
	UINT8 offset = 0;
	if (state == 1) offset = 4;
	set_sprite_tile(0, offset);
	set_sprite_tile(1, offset + 1);
	set_sprite_tile(2, offset + 2);
	set_sprite_tile(3, offset + 3);
}
