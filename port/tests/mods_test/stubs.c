/* What the test program leaves out of the game: its variable table, string
 * tables and function hooks (mods_test covers the disc files, not game data). */

#include "game_hooks.h"

const LainSymbol lain_symbols[1];
const int lain_symbol_count = 0;
char *g_node_text_strings[417];
char *g_node_label_strings[51];

int modrt_hook(const char *name, void *replacement, void **next) {
    (void)name;
    (void)replacement;
    (void)next;
    return -1;
}
