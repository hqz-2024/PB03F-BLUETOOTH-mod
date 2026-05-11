
/* OSAL */
#include "types.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"
//#include "OnBoard.h"
/* LL */
#include "ll.h"

/* HCI */
#include "hci_tl.h"

/* L2CAP */
#include "l2cap.h"

/* gap */
#include "gap.h"
#include "gapgattserver.h"

/* GATT */
#include "gatt.h"

#include "gattservapp.h"
int app_main(void)
{
    /* Initialize the operating system */
    osal_init_system();
    osal_pwrmgr_device( PWRMGR_BATTERY );
    /* Start OSAL */
    osal_start_system(); // No Return from here
    return 0;
}
/*********************************************************************
    GLOBAL VARIABLES
*/

// The order in this table must be identical to the task initialization calls below in osalInitTask.
#include "BYS_BLE_Manage.h"
#include "BYS_BLE_APP.h"
__ATTR_SECTION_SRAM__ const pTaskEventHandlerFn tasksArr[] ={
  LL_ProcessEvent,                                                  // task 0
  HCI_ProcessEvent,                                                 // task 1
  L2CAP_ProcessEvent,                                               // task 2
  GAP_ProcessEvent,                                                 // task 3
  GATT_ProcessEvent,                                                // task 4
  SM_ProcessEvent,                                                  // task 5
  GATTServApp_ProcessEvent,                                         // task 6
	BYS_BLE_Manage_ProcessEvent,                                      // task 7

	BYS_BLE_APP_ProcessEvent,                                         // task 8

};

__ATTR_SECTION_SRAM__ const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );
uint16* tasksEvents;

/*********************************************************************
    FUNCTIONS
 *********************************************************************/

/*********************************************************************
    @fn      osalInitTasks

    @brief   This function invokes the initialization function for each task.

    @param   void

    @return  none
*/
void osalInitTasks( void )
{
    uint8 taskID = 0;
    tasksEvents = (uint16*)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
    osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));
    /* LL Task */
    LL_Init( taskID++ );
    /* HCI Task */
    HCI_Init( taskID++ );
    /* L2CAP Task */
    L2CAP_Init( taskID++ );
    /* GAP Task */
    GAP_Init( taskID++ );
    /* GATT Task */
    GATT_Init( taskID++ );
    /* SM Task */
    SM_Init( taskID++ );
    GATTServApp_Init( taskID++ );
    /* Profiles */
	BYS_BLE_Manage_Init(taskID++);
//    GAPRole_Init( taskID++ );
    /* Application */
    BYS_BLE_APP_Init(taskID++);
}

