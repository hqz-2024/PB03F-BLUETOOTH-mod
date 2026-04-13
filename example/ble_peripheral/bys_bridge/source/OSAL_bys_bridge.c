#if (APP_CFG == 0)
#include "OSAL.h"
#include "OSAL_Tasks.h"

/* LL / HCI */
#include "ll.h"
#include "hci_tl.h"

#if defined(OSAL_CBTIMER_NUM_TASKS)
    #include "osal_cbTimer.h"
#endif

/* BLE 协议栈 */
#include "l2cap.h"
#include "gap.h"
#include "gapgattserver.h"
#include "gapbondmgr.h"
#include "gatt.h"
#include "gattservapp.h"
#include "peripheral.h"

/* 应用层 */
#include "bys_bridge.h"

/* ─── 任务处理函数表（顺序必须与 osalInitTasks 一致）─── */
__ATTR_SECTION_SRAM__ const pTaskEventHandlerFn tasksArr[] = {
    LL_ProcessEvent,                                        /* task 0 */
    HCI_ProcessEvent,                                       /* task 1 */
#if defined(OSAL_CBTIMER_NUM_TASKS)
    OSAL_CBTIMER_PROCESS_EVENT(osal_CbTimerProcessEvent),   /* task 2 */
#endif
    L2CAP_ProcessEvent,                                     /* task 3 */
    SM_ProcessEvent,                                        /* task 4 */
    GAP_ProcessEvent,                                       /* task 5 */
    GATT_ProcessEvent,                                      /* task 6 */
    GAPRole_ProcessEvent,                                   /* task 7 */
#if (DEF_GAPBOND_MGR_ENABLE == 1)
    GAPBondMgr_ProcessEvent,                                /* task 8 */
#endif
    GATTServApp_ProcessEvent,                               /* task 9 */
    BYS_Bridge_ProcessEvent,                                /* task 10：应用任务 */
};

__ATTR_SECTION_SRAM__ const uint8 tasksCnt =
    sizeof(tasksArr) / sizeof(tasksArr[0]);

uint16 *tasksEvents;

/* ─── 任务初始化（顺序必须与 tasksArr 完全一致）──── */
void osalInitTasks(void)
{
    uint8 taskID = 0;

    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, sizeof(uint16) * tasksCnt);

    LL_Init(taskID++);
    HCI_Init(taskID++);

#if defined(OSAL_CBTIMER_NUM_TASKS)
    osal_CbTimerInit(taskID);
    taskID += OSAL_CBTIMER_NUM_TASKS;
#endif

    L2CAP_Init(taskID++);
    SM_Init(taskID++);
    GAP_Init(taskID++);
    GATT_Init(taskID++);
    GAPRole_Init(taskID++);

#if (DEF_GAPBOND_MGR_ENABLE == 1)
    GAPBondMgr_Init(taskID++);
#endif

    GATTServApp_Init(taskID++);
    BYS_Bridge_Init(taskID++);  /* 应用任务，必须最后 */
}
#endif
