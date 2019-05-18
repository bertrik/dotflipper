#ifdef __cplusplus
extern "C" {
#endif



#define BOARD_WIDTH 10
#define BOARD_HEIGHT 32

#define PIECE_BLOCKS 5

#include "Arduino.h"

uint8_t dot_screen[BOARD_WIDTH][BOARD_HEIGHT];
uint8_t dot_screen_old[BOARD_WIDTH][BOARD_HEIGHT];
byte ch_out[7][5];

uint8_t screen[BOARD_WIDTH][BOARD_HEIGHT];
uint8_t board[BOARD_WIDTH][BOARD_HEIGHT];
int mPosX, mPosY;               // Position of the piece that is falling down
int mPiece, mRotation;          // Kind and rotation the piece that is falling down

int deleted_lines;
int deleted_line_num;
void flipdot(uint16_t x, uint16_t y, bool color);

void clear_screen ();
void game_InitGame();
void board_InitBoard();

void game_DrawPiece (int pX, int pY, int pPiece, int pRotation);
void game_DrawBoard ();
bool board_IsPossibleMovement (int pX, int pY, int pPiece, int pRotation);
void board_StorePiece(int pX, int pY, int pPiece, int pRotation);
void board_DeletePossibleLines ();
bool board_IsGameOver();
void game_CreateNewPiece();
void fill_screen(bool pattern);
void update_screen(uint8_t new_screen[][BOARD_HEIGHT]);
void display_word(char str[6]);
void convert_to_arr(char letter);
void refresh_screen();

int piece_GetBlockType (int pPiece, int pRotation, int pX, int pY);

#ifdef __cplusplus
}
#endif
