#include "bsp_board.h"

static bsp_board_t bsp_board;

bsp_board_t *bsp_board_get_instance(void)
{
    return &bsp_board;
}