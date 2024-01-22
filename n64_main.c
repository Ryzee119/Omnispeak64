#include <libdragon.h>
#include "id_ca.h"
#include "id_fs.h"
#include "id_in.h"
#include "id_mm.h"
#include "id_rf.h"
#include "id_us.h"
#include "id_vl.h"
#include "ck_act.h"
#include "ck_cross.h"
#include "ck_def.h"
#include "ck_game.h"
#include "ck_play.h"
#include "ck4_ep.h"
#include "ck5_ep.h"
#include "ck6_ep.h"

void CK_InitGame();
void CK_DemoLoop();

extern CK_EpisodeDef *ck_currentEpisode;
extern IN_ControlType in_controlType;

typedef struct sram_files_t
{
    const char *name;
    uint32_t size;
    uint32_t offset; //Track position of the file cursor
} sram_files_t;
int sramfs_init(sram_files_t *files, int num_files);

#define MAX_SRAM_FILES 2
#ifdef EP4
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"OMNISPK.CFG", 2048, 0},
    {"SAVEGAM0.CK4", 131072 - 2048, 0},
};
#elif EP5
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"OMNISPK.CFG", 2048, 0},
    {"SAVEGAM0.CK5", 131072 - 2048, 0},
};
#elif EP6
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"OMNISPK.CFG", 2048, 0},
    {"SAVEGAM0.CK6", 131072 - 2048, 0},
};
#endif

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_ISVIEWER);
    dfs_init(DFS_DEFAULT_LOCATION);
    sramfs_init(sram_files, MAX_SRAM_FILES);
    timer_init();

    FS_Startup();
    MM_Startup();
    CFG_Startup();

#ifdef EP4
    ck_currentEpisode = &ck4_episode;
#elif EP5
    ck_currentEpisode = &ck5_episode;
#elif EP6
    ck_currentEpisode = &ck6_episode;
#else
    #error Error: EP4, EP5 or EP6 not defined.
#endif

    CK_InitGame();
    ck_currentEpisode->hasCreatureQuestion = false;


    in_controlType = IN_ctrl_Joystick1;

    while (1)
    {
        CK_DemoLoop();
        CK_ShutdownID();
    }
}
