GBDK_LOCATION=/lib/gbdk/
CC=$(GBDK_LOCATION)/bin/lcc

OUT_FOLDER=Build/
OUT_NAME=Minesweeper.gb

default:
	@mkdir -p $(OUT_FOLDER)
	@$(CC) -Wa-l -Wl-m -Wl-j -Wm-yc -DUSE_SFR_FOR_REG Minesweeper.c -o $(OUT_FOLDER)/$(OUT_NAME)

clean:
	@rm -rf $(OUT_FOLDER)