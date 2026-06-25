#if (APP_CFG == 0)
#include "OSAL.h"
#include "OSAL_Tasks.h"

/* LL / HCI */
#include "ll.h"
#include "hci_tl.h"

#if defined(OSAL_CBTIMER_NUM_TASKS)
    #include "osal_cbTimer.h"
#endif

/* BLE */
#include "l2cap.h"
#include "gap.h"
#include "gapgattserver.h"
#include "gapbondmgr.h"
#include "gatt.h"
#include "gattservapp.h"
#include "peripheral.h"
#include "central.h"

/* app */
#include "remote_app.h"

__ATTR_SECTION_SRAM__ const pTaskEventHandlerFn tasksArr[] = {
    LL_ProcessEvent,
    HCI_ProcessEvent,
#if defined(OSAL_CBTIMER_NUM_TASKS)
    OSAL_CBTIMER_PROCESS_EVENT(osal_CbTimerProcessEvent),
#endif
    L2CAP_ProcessEvent,
    SM_ProcessEvent,
    GAP_ProcessEvent,
    GATT_ProcessEvent,
    GAPRole_ProcessEvent,           /* Peripheral (config mode) */
    GAPCentralRole_ProcessEvent,    /* Central   (normal mode) */
#if (DEF_GAPBOND_MGR_ENABLE == 1)
    GAPBondMgr_ProcessEvent,
#endif
    GATTServApp_ProcessEvent,
    Remote_ProcessEvent,
};

__ATTR_SECTION_SRAM__ const uint8 tasksCnt = sizeof(tasksArr) / sizeof(tasksArr[0]);
uint16 *tasksEvents;

void osalInitTasks(void)
{
    uint8 taskID = 0;
    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, sizeof(uint16) * tasksCnt);

    LL_Init(taskID++);
    HCI_Init(taskID++);
#if defined(OSAL_CBTIMER_NUM_TASKS)
    osal_CbTimerInit(taskID); taskID += OSAL_CBTIMER_NUM_TASKS;
#endif
    L2CAP_Init(taskID++);
    SM_Init(taskID++);
    GAP_Init(taskID++);
    GATT_Init(taskID++);
    GAPRole_Init(taskID++);
    GAPCentralRole_Init(taskID++);
#if (DEF_GAPBOND_MGR_ENABLE == 1)
    GAPBondMgr_Init(taskID++);
#endif
    GATTServApp_Init(taskID++);
    Remote_Init(taskID++);
}
#endif
