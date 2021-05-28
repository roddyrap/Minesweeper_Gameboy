#include <gb/gb.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "rand.h"
#include "time.h"

#include "Tiles/mine_tile_sheet_data.c"
#include "Tiles/mine_tile_sheet_map.c"
#include "Tiles/sprites_data.c"
#include "Tiles/sprites_map.c"
#include "Tiles/ChicagoFont_data.c"
#include "Tiles/ChicagoFont_map.c"
#include "Tiles/ui_background_data.c"
#include "Tiles/ui_background_map.c"
#include "Tiles/mode_selector_data.c"

// TODO: Can't click on tiles if scrolled, isn't aligned properly with rest of the code

typedef struct {
    unsigned int is_revealed : 1;
    unsigned int is_bomb : 1;
	unsigned int is_flagged: 1;
} board_tile;
// constants
const int font_start = 0x32; // Starts at space

// Last funs decleration
void init();
void checkInput();
void anim_sprite(UINT8);
UINT8 bombsNearTile(UINT8, UINT8);
void click_tile(UINT8, UINT8);
void win();
void lose();

// consts
const int ui_background_start = 0x92;

// Typed vars
unsigned char time_passed[5];
unsigned char game_status[4];
unsigned char flags_used[8];
unsigned char scroll_show[5];

unsigned char ui_ordered_data[144];
time_t start_time;
BOOLEAN input_enabled;
BOOLEAN a_clicked;
BOOLEAN select_clicked;
BOOLEAN b_clicked;
BOOLEAN moving_dir;
BOOLEAN select_menu_open;
INT8 select_param;
UINT8 num_rows = 9;  // Number of Y positions screen can show (Not including WIN) 
UINT8 num_cols = 6;  // Number of X positions screen can show (Not including WIN)
UINT8 scroll_state[2];  // Board offset from screen
UINT8 cursor[2];  // Player location on board screen, (x, y)
UINT8 board_size;  // Root of the number of tiles on the board, limited due to RAM.
UINT16 bombs_num;  // Number of bombs on the board
UINT16 flags_num;  // Number of flags left
UINT16 flag_balance;  // Bombs unflagged
BOOLEAN isFirstClick;  // Should generate bombs on click
board_tile *board_tiles;  // Information about tiles on the board, size of BoardNum^2
// Helper functions
void write_to_array(unsigned char* array, UINT8 array_size, char* str) {
	for (UINT8 i = 0; i < array_size; i++) {
		if (str[i] == 0) break;
		array[i] = str[i] - ' ' + 0x32;
	}
}


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

void draw_current_board() {
	for (UINT16 i = 0; i < num_cols; i++) {
		UINT8 new_x = i + scroll_state[0];
		
		for (UINT8 j = 0; j < num_rows; j++) {
			UINT8 new_y = j + scroll_state[1];
			UINT8 tile_offset = 0;
			if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_flagged) {
				tile_offset = 11 * 4 + 4;
			}
			else if (board_tiles[flatten_coords(new_x, new_y, board_size)].is_revealed) {
				UINT8 bombs_near = bombsNearTile(new_x, new_y);
				tile_offset = 4 + 4 * bombs_near;
			}
			set_bkg_tiles(2 * i, 2 * j, 2, 2, mine_tile_sheet_map + tile_offset);
		}
	}
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
	draw_current_board();
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

void reveal_tile(UINT8 x, UINT8 y, INT8 tile_num) {
	if (x >= num_cols || y >= num_rows) return;
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

UINT8 flagsNearTile(UINT8 tile_x, UINT8 tile_y) {
	UINT8 numFlags = 0;
	for (INT16 i = -1; i < 2; i++) {
		for (INT16 j = -1; j < 2; j++) {
			if (!coords_in_board(tile_x + i, tile_y + j) ||  i == 0 && j == 0) continue;
			numFlags += board_tiles[flatten_coords(i + tile_x, j + tile_y, board_size)].is_flagged;
		}
	}
	return numFlags;
}

void reveal_nearby(UINT8 x, UINT8 y) {
	for (INT16 i = -1; i < 2; i++) {
		for (INT16 j = -1; j < 2; j++) {
			if (!coords_in_board(x + i, y + j) ||  x == 0 && y == 0) continue;
			if (board_tiles[flatten_coords(i + x, j + y, board_size)].is_flagged) continue;
			UINT8 bombsNear = bombsNearTile(i + x, j + y);
			// Need to differentiate because click_tile reveals a lot
			if (bombsNear == 0) click_tile(i + x, j + y);
			else {
				board_tiles[flatten_coords(i + x, j + y, board_size)].is_revealed = 1;
				if (board_tiles[flatten_coords(i + x, j + y, board_size)].is_bomb) lose();
				reveal_tile(x + i - scroll_state[0], y + j - scroll_state[1], bombsNear); 
			}
		}
	}
}

void click_tile(UINT8 x, UINT8 y) {
	if (board_tiles[flatten_coords(x, y, board_size)].is_flagged) return;
	UINT8 bombsNear = bombsNearTile(x, y);
	if (bombsNear == 10) {
		lose();
	}
	if (board_tiles[flatten_coords(x, y, board_size)].is_revealed){
		if (bombsNear == 0) return;
		if (flagsNearTile(x, y) == bombsNear) reveal_nearby(x, y);
	}
	board_tiles[flatten_coords(x, y, board_size)].is_revealed = 1;  // Revealed tile, to avoid duplication code in auto-reveal which I will hopefully add
	reveal_tile(x - scroll_state[0], y - scroll_state[1], bombsNear);
	// Auto-open if blank
	if (bombsNear != 0) return;
	if (bombsNear != 0) return;
	for (INT16 i = -1; i < 2; i++) {
		for (INT16 j = -1; j < 2; j++) {
			if (!coords_in_board(x + i, y + j) ||  x == 0 && y == 0) continue;
			click_tile(i + x, j + y);
		}
	}
}

void number_to_chars(unsigned char *array, UINT8 arr_start, UINT8 arr_end, UINT32 num) {
	for (UINT8 neg_ind = 1; neg_ind <= arr_end - arr_start; neg_ind++) {
		array[arr_end - neg_ind] = 0x42 + num % 10;
		num = num / 10;
	}
}

void set_board_size(UINT8 new_size, UINT8 num_bombs) {
	start_time = time(0);
	board_size = new_size;
	bombs_num = num_bombs;
	flags_num = num_bombs;
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
	write_to_array(game_status, 4, "Play");
	write_to_array(time_passed, 5, "00000");
	write_to_array(flags_used, 8, "000//000");
	write_to_array(scroll_show, 5, "00/00");
	// Initialize cursor
	cursor[0] = 0;
	cursor[1] = 0;
	move_sprite_grid(cursor[0], cursor[1]);
	// Init scroll
	scroll_state[0] = 0;
	scroll_state[1] = 0;
	// updating flag string
	number_to_chars(flags_used, 5, 8, flags_num);
	
	board_tiles = malloc(board_size * board_size);
	for (UINT16 i=0; i < (UINT16)(board_size * board_size); i++){
		board_tiles[i].is_bomb = 0;
		board_tiles[i].is_revealed = 0;
		board_tiles[i].is_flagged = 0;
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

void win() {
	write_to_array(game_status, 4, "Win ");
	// game_status[0] = 0x69;
	// game_status[1] = 0x7B;
	// game_status[2] = 0x80;
	// game_status[3] = 0x32;

	input_enabled = 0;
}

void lose() {
	write_to_array(game_status, 4, "Lose");
	// , 0x81, 0x85, 0x77}
	// game_status[0] = 0x5E;
	// game_status[1] = 0x81;
	// game_status[2] = 0x85;
	// game_status[3] = 0x77;

	input_enabled = 0;
}


void flag_tile(UINT8 x, UINT8 y) {
	if (isFirstClick || board_tiles[flatten_coords(x, y, board_size)].is_revealed) return;
	BOOLEAN is_flagged = board_tiles[flatten_coords(x, y, board_size)].is_flagged;
	BOOLEAN is_bomb = board_tiles[flatten_coords(x, y, board_size)].is_bomb;
	if (is_flagged) {
		board_tiles[flatten_coords(x, y, board_size)].is_flagged = 0;
		reveal_tile(x - scroll_state[0], y - scroll_state[1], -1);
		flags_num++;
		if (is_bomb) flag_balance++;
	}
	else {
		if (flags_num <= 0) return;
		board_tiles[flatten_coords(x, y, board_size)].is_flagged = 1;
		reveal_tile(x - scroll_state[0], y - scroll_state[1], 11);
		flags_num--;
		if (is_bomb) flag_balance--;
		if (flag_balance == 0) {
			win();
		}
	}
}


void updateSwitches() {
	SHOW_BKG;
	SHOW_SPRITES;
}

void update_ui() {
	// Status
	set_bkg_tiles(13, 3, 4, 1, game_status);
	// time
	if (input_enabled) {
		UINT32 time_since = (UINT32)(time(0)) - start_time;
		number_to_chars(time_passed, 0, 5, time_since);
		set_bkg_tiles(13, 7, 5, 1, time_passed);
	}
	// Flags
	number_to_chars(flags_used, 0, 3, flags_num);
	set_bkg_tiles(13, 11, 4, 2, flags_used);
	// Scroll
	number_to_chars(scroll_show, 0, 2, scroll_state[0]);
	number_to_chars(scroll_show, 3, 5, scroll_state[1]);
	set_bkg_tiles(13, 15, 5, 1, scroll_show);
}


void move_select(INT8 y) {
	if (!select_menu_open) return;
	select_param += y;
	if (select_param > 3) select_param = 0;
	if (select_param < 0) select_param = 3;
	move_sprite(8, 32, 32 + select_param * 16);
}

void select() {
	if (select_menu_open == 0) {
		select_menu_open = 1;
		input_enabled = 0;
		set_bkg_tiles(2, 0, 8, 18, ui_ordered_data);
		unsigned char select_text[6];
		write_to_array(select_text, 6, "Easy  ");
		set_bkg_tiles(4, 2, 6, 1, select_text);
		write_to_array(select_text, 6, "Inter ");
		set_bkg_tiles(4, 4, 6, 1, select_text);
		write_to_array(select_text, 6, "Expert");
		set_bkg_tiles(4, 6, 6, 1, select_text);
		write_to_array(select_text, 6, "NiMare");
		set_bkg_tiles(4, 8, 6, 1, select_text);
		set_sprite_prop(0, S_PRIORITY);
		set_sprite_prop(1, S_PRIORITY);
		set_sprite_prop(2, S_PRIORITY);
		set_sprite_prop(3, S_PRIORITY);
		set_sprite_prop(8, 0);
		move_sprite(8, 32, 32);
		select_param = 0;

	}
	else {
		select_menu_open = 0;
		input_enabled = 1;
		set_sprite_prop(0, 0);
		set_sprite_prop(1, 0);
		set_sprite_prop(2, 0);
		set_sprite_prop(3, 0);
		move_sprite(8, 0, 0);
		draw_current_board();
	}

}


void main() {

	init();
	
	while(1) {
		
		checkInput();
		updateSwitches();
		wait_vbl_done();
		update_ui();
	}
	
}

void init() {
	
	DISPLAY_ON;		// Turn on the display
	NR52_REG = 0x8F;	// Turn on the sound
	NR51_REG = 0x11;	// Enable the sound channels
	NR50_REG = 0x77;	// Increase the volume to its max
	// Init map
	set_bkg_data(0, 49, mine_tile_sheet_data);	// Load Minetiles to memory
	set_bkg_data(50, 96, ChicagoFont_data);	// Load ChicagoFont to memory, 96 tiles
	set_bkg_data(146, 9, ui_background_data);	// Load ui background to memory, 96 tiles
	// Initialize cursor sprite
	UINT8 num_cursor_sprites = 8;
	set_sprite_data(0, num_cursor_sprites, sprites_data);
	set_sprite_data(num_cursor_sprites, 1, mode_selector_data);
	// Setting cursor sprites to sprite memory
	for (UINT8 i = 0; i < num_cursor_sprites + 1; i++) {
		set_sprite_tile(i, i);
	}
	set_sprite_prop(8, S_PRIORITY);
	// Init board
	set_board_size(10, 10);
	// Init UI
	// Background
	for (UINT8 j = 0; j < 144; j++) {
		ui_ordered_data[j] = ui_background_map[j] + 146;
	}
	set_bkg_tiles(12, 0, 8, 18, ui_ordered_data);
	// Text

	unsigned char status[6];
	write_to_array(status, 6, "Status");
	unsigned char time[4];
	write_to_array(time, 4, "Time");
	unsigned char flags[5];
	write_to_array(flags, 5, "Flags");
	unsigned char scroll[6];
	write_to_array(scroll, 6, "Scroll");
	// Status
	set_bkg_tiles(13, 1, 6, 1, status);
	set_bkg_tiles(13, 3, 4, 1, game_status);
	// time
	set_bkg_tiles(13, 5, 4, 1, time);
	set_bkg_tiles(13, 7, 5, 1, time_passed);
	// Flags
	set_bkg_tiles(13, 9, 5, 1, flags);
	set_bkg_tiles(13, 11, 4, 2, flags_used);
	// Scroll
	set_bkg_tiles(13, 14, 6, 1, scroll);
	set_bkg_tiles(13, 15, 5, 1, scroll_show);
}

void checkInput() {
	// Timed calls
	static UINT8 inputSlower = 0;
	inputSlower++;
	if (inputSlower % 30 == 0) anim_sprite(inputSlower % 60 / 30);
	if (joypad() & J_START) {
			if (!select_menu_open) set_board_size(board_size, bombs_num);
			else {  // Restarting with menu open
				select();
				switch (select_param)
				{
				case 0:  // Easy
					set_board_size(10, 10);
					break;
				case 1:  // Intermediate
					set_board_size(16, 40);
					break;
				case 2:  // Expert
					set_board_size(22, 99);
					break;
				case 3:  // Nightmare
					set_board_size(32, 200);
					break;
				default:  // Nothing
					break;
				}
			} 
	}
	if (joypad() & J_SELECT) {
		if (select_clicked) return;
		select_clicked = 1;
		select();
	}
	else select_clicked = 0;
	if (!moving_dir || (inputSlower - moving_dir) % 12 == 0) {
		if (joypad() & J_UP) {
			if (input_enabled) {
				move_cursor(0, -1, 1);
				moving_dir = inputSlower;	
			}

			if (select_menu_open) {
				move_select(-1);
				moving_dir = inputSlower;	
			}
		}
		if (joypad() & J_DOWN) {
			if (input_enabled) {
				move_cursor(0, 1, 1);
				moving_dir = inputSlower;
			}
			if (select_menu_open) {
				move_select(1);
				moving_dir = inputSlower;	
			}


		}
		if (!input_enabled) return;
		if (joypad() & J_LEFT) {
			move_cursor(-1, 0, 1);
			moving_dir = inputSlower;

		}
		if (joypad() & J_RIGHT) {
			move_cursor(1, 0, 1);
			moving_dir = inputSlower;
		}
	}
	// Not moving
	if (!(joypad() & J_RIGHT || joypad() & J_LEFT || joypad() & J_UP || joypad() & J_DOWN)) moving_dir = 0;
	if (inputSlower % 4 != 0) return;
	
    if (joypad() & J_A) {
		if (!a_clicked) {
			a_clicked = 1;
			if (isFirstClick) first_tile();
			else click_tile(cursor_board_x(), cursor_board_y());	
		}
    }
	else a_clicked = 0;
	if (joypad() & J_B) {
		if (!b_clicked) {
			b_clicked = 1;
			flag_tile(cursor_board_x(), cursor_board_y());
		}
	}
	else b_clicked = 0;
}

void anim_sprite(UINT8 state) {
	UINT8 offset = 0;
	if (state == 1) offset = 4;
	set_sprite_tile(0, offset);
	set_sprite_tile(1, offset + 1);
	set_sprite_tile(2, offset + 2);
	set_sprite_tile(3, offset + 3);
}
