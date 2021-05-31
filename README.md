# Minesweeper Gameboy
This is a port/demake of the microsoft game Minesweeper. The game works both for the Gameboy and the Gameboy colors, with enhanced colors for the latter.

While I do have a Gameboy (Color) I wasn't able to test it on official hardware due to a lack of ability to load it to a cartridge. The rom file was tested on [BGB](https://bgb.bircd.org/) and [Emuclius](https://emulicious.net/).

# Gameplay (from Wikipedia)
In Minesweeper, mines (that resemble naval mines in the classic theme) are scattered throughout a board, which is divided into cells. Cells have three states: uncovered, covered and flagged. A covered cell is blank and clickable, while an uncovered cell is exposed. Flagged cells are those marked by the player to indicate a potential mine location.

A player left-clicks a cell to uncover it. If a player uncovers a mined cell, the game ends, as there is only 1 life per game. Otherwise, the uncovered cells displays either a number, indicating the quantity of mines diagonally and/or adjacent to it, or a blank tile (or "0"), and all adjacent non-mined cells will automatically be uncovered. Right-clicking on a cell will flag it, causing a flag to appear on it. Flagged cells are still considered covered, and a player can click on them to uncover them, although typically they must first be unflagged with an additional right-click.

The first click in any game will never be a mine.

To win the game, players must uncover all non-mine cells, at which point, the timer is stopped. Flagging all the mined cells is not required.


# GB Modifications (rules)
Instead of using the mouse and left/right clicks the player uses the d-pad and the A and B buttons to navigate.
- A is used to open tiles
- B is used to flag and unflag tiles
- All mines need to be flagged in order to win and no more
- Start is used to restart game
- Select opens a difficulty select screen

WARNING: While large board sizes are available the Gameboy takes a lot of time to place the bombs, it might freeze for around a minute and a half while placing the bombs, but should then work clearly.

# Images
## GBC
![GameStartEasy](Images/GBC/GameStartEasy.bmp)
![GameMidEasy](Images/GBC/GameMidEasy.bmp)
![GameExpertEnd](Images/GBC/GameExpertEnd.bmp)
![GameExpertEnd](Images/GBC/SelectMenu.bmp)
## GB
![GameStartEasy](Images/GB/GameStartEasy.bmp)
![GameMidEasy](Images/GB/GameMidEasy.bmp)
![GameExpertEnd](Images/GB/GameExpertEnd.bmp)
![GameExpertEnd](Images/GB/SelectMenu.bmp)

# Bug reports and support
Feel free to submit all bug reports and code changes ideas and requests to the corresponding tabs in GitHub. 

# License
This project is licensed under the Apache license 2.0.

# Attribution
Game play section provided by Wikipedia under the "Creative Commons Attribution-ShareAlike" License. This is not endorsed by Wikipedia or its writers.