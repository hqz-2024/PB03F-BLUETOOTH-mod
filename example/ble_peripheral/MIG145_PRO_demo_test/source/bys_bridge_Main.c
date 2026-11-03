#if (APP_CFG == 0)
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"

/* app_main：硬件初始化完成后由 main.c 调用 */
int app_main(void)
{
    osal_init_system();
    osal_pwrmgr_device(PWRMGR_BATTERY);
    osal_start_system(); /* 不返回 */
    return 0;
}
#endif
