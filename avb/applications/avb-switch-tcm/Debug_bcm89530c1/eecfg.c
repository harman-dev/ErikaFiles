#include "ee.h"


/***************************************************************************
 *
 * HW counter definition
 *
 **************************************************************************/
    const EE_oo_counter_hw_ROM_type EE_counter_hw_ROM[EE_COUNTER_HW_ROM_SIZE] = {
        {3125U}         /* SystemTimer */
    };




/***************************************************************************
 *
 * Stack definition for CORTEX Rx
 *
 **************************************************************************/
    #define STACK_TransmitThread_ID 1
    #define STACK_TransmitThread_SIZE 512 // size = 2048 bytes 
    #define STACK_ReceiveThread_ID 2
    #define STACK_ReceiveThread_SIZE 512 // size = 2048 bytes 
    #define STACK_ConfigSaveThread_ID 3
    #define STACK_ConfigSaveThread_SIZE 512 // size = 2048 bytes 
    #define STACK_EthIsrHandler_ID 4
    #define STACK_EthIsrHandler_SIZE 256 // size = 1024 bytes 
    #define STACK_MgmtCmdHandler_ID 5
    #define STACK_MgmtCmdHandler_SIZE 512 // size = 2048 bytes 
    #define STACK_LinkStateHandler_ID 6
    #define STACK_LinkStateHandler_SIZE 256 // size = 1024 bytes 
    #define STACK_TaskShell_ID 7
    #define STACK_TaskShell_SIZE 1024 // size = 4096 bytes 
    #define STACK_IRQ_SIZE 512 // size = 2048 bytes 

    int EE_stack_TransmitThread[STACK_TransmitThread_SIZE];
    int EE_stack_ReceiveThread[STACK_ReceiveThread_SIZE];
    int EE_stack_ConfigSaveThread[STACK_ConfigSaveThread_SIZE];
    int EE_stack_EthIsrHandler[STACK_EthIsrHandler_SIZE];
    int EE_stack_MgmtCmdHandler[STACK_MgmtCmdHandler_SIZE];
    int EE_stack_LinkStateHandler[STACK_LinkStateHandler_SIZE];
    int EE_stack_TaskShell[STACK_TaskShell_SIZE];
    int EE_stack_IRQ[STACK_IRQ_SIZE];	/* irq stack */

    const EE_UREG EE_std_thread_tos[EE_MAX_TASK+1] = {
        0,	 /* dummy*/
        STACK_TransmitThread_ID,	 /* TransmitThread*/
        STACK_ReceiveThread_ID,	 /* ReceiveThread*/
        STACK_ConfigSaveThread_ID,	 /* ConfigSaveThread*/
        STACK_EthIsrHandler_ID,	 /* EthIsrHandler*/
        STACK_MgmtCmdHandler_ID,	 /* MgmtCmdHandler*/
        STACK_LinkStateHandler_ID,	 /* LinkStateHandler*/
        STACK_TaskShell_ID 	 /* TaskShell*/
    };

    struct EE_TOS EE_std_system_tos[EE_STD_SYSTEM_TOS_SIZE] = {
        {0},	/* Task   (dummy) */
        {(EE_ADDR)(&EE_stack_TransmitThread[STACK_TransmitThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 0 (TransmitThread) */
        {(EE_ADDR)(&EE_stack_ReceiveThread[STACK_ReceiveThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 1 (ReceiveThread) */
        {(EE_ADDR)(&EE_stack_ConfigSaveThread[STACK_ConfigSaveThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 2 (ConfigSaveThread) */
        {(EE_ADDR)(&EE_stack_EthIsrHandler[STACK_EthIsrHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 3 (EthIsrHandler) */
        {(EE_ADDR)(&EE_stack_MgmtCmdHandler[STACK_MgmtCmdHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 4 (MgmtCmdHandler) */
        {(EE_ADDR)(&EE_stack_LinkStateHandler[STACK_LinkStateHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET])},	/* Task 5 (LinkStateHandler) */
        {(EE_ADDR)(&EE_stack_TaskShell[STACK_TaskShell_SIZE - CORTEX_RX_INIT_TOS_OFFSET])} 	/* Task 6 (TaskShell) */
    };

    EE_UREG EE_std_active_tos = 0U; /* dummy */

    /* stack used only by IRQ handlers */
    struct EE_TOS EE_std_IRQ_tos = {
        (EE_ADDR)(&EE_stack_IRQ[STACK_IRQ_SIZE - 1])
    };


    struct Brcm_Stacks Brcm_system_tos[EE_CORTEX_RX_SYSTEM_TOS_SIZE] = {
        {"irq",EE_stack_IRQ, STACK_IRQ_SIZE, STACK_IRQ_SIZE},
        {"0:TransmitThread",EE_stack_TransmitThread,STACK_TransmitThread_SIZE, STACK_TransmitThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 0 (TransmitThread) */
        {"1:ReceiveThread",EE_stack_ReceiveThread,STACK_ReceiveThread_SIZE, STACK_ReceiveThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 1 (ReceiveThread) */
        {"2:ConfigSaveThread",EE_stack_ConfigSaveThread,STACK_ConfigSaveThread_SIZE, STACK_ConfigSaveThread_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 2 (ConfigSaveThread) */
        {"3:EthIsrHandler",EE_stack_EthIsrHandler,STACK_EthIsrHandler_SIZE, STACK_EthIsrHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 3 (EthIsrHandler) */
        {"4:MgmtCmdHandler",EE_stack_MgmtCmdHandler,STACK_MgmtCmdHandler_SIZE, STACK_MgmtCmdHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 4 (MgmtCmdHandler) */
        {"5:LinkStateHandler",EE_stack_LinkStateHandler,STACK_LinkStateHandler_SIZE, STACK_LinkStateHandler_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 5 (LinkStateHandler) */
        {"6:TaskShell",EE_stack_TaskShell,STACK_TaskShell_SIZE, STACK_TaskShell_SIZE - CORTEX_RX_INIT_TOS_OFFSET},	/* Task 6 (TaskShell) */
        };




/***************************************************************************
 *
 * Kernel ( CPU 0 )
 *
 **************************************************************************/
    /* Definition of task's body */
    DeclareTask(TransmitThread);
    DeclareTask(ReceiveThread);
    DeclareTask(ConfigSaveThread);
    DeclareTask(EthIsrHandler);
    DeclareTask(MgmtCmdHandler);
    DeclareTask(LinkStateHandler);
    DeclareTask(TaskShell);

    const EE_THREAD_PTR EE_hal_thread_body[EE_MAX_TASK] = {
        &EE_oo_thread_stub,		 /* thread TransmitThread */
        &EE_oo_thread_stub,		 /* thread ReceiveThread */
        &EE_oo_thread_stub,		 /* thread ConfigSaveThread */
        &EE_oo_thread_stub,		 /* thread EthIsrHandler */
        &EE_oo_thread_stub,		 /* thread MgmtCmdHandler */
        &EE_oo_thread_stub,		 /* thread LinkStateHandler */
        &EE_oo_thread_stub 		 /* thread TaskShell */

    };

    EE_UINT32 EE_terminate_data[EE_MAX_TASK];

    /* ip of each thread body (ROM) */
    const EE_THREAD_PTR EE_terminate_real_th_body[EE_MAX_TASK] = {
        &FuncTransmitThread,
        &FuncReceiveThread,
        &FuncConfigSaveThread,
        &FuncEthIsrHandler,
        &FuncMgmtCmdHandler,
        &FuncLinkStateHandler,
        &FuncTaskShell
    };
    /* ready priority */
    const EE_TYPEPRIO EE_th_ready_prio[EE_MAX_TASK] = {
        0x8U,		 /* thread TransmitThread */
        0x10U,		 /* thread ReceiveThread */
        0x1U,		 /* thread ConfigSaveThread */
        0x20U,		 /* thread EthIsrHandler */
        0x2U,		 /* thread MgmtCmdHandler */
        0x4U,		 /* thread LinkStateHandler */
        0x2U 		 /* thread TaskShell */
    };

    const EE_TYPEPRIO EE_th_dispatch_prio[EE_MAX_TASK] = {
        0x8U,		 /* thread TransmitThread */
        0x10U,		 /* thread ReceiveThread */
        0x1U,		 /* thread ConfigSaveThread */
        0x20U,		 /* thread EthIsrHandler */
        0x2U,		 /* thread MgmtCmdHandler */
        0x4U,		 /* thread LinkStateHandler */
        0x2U 		 /* thread TaskShell */
    };

    /* thread status */
    EE_TYPESTATUS EE_th_status[EE_MAX_TASK] = {
        SUSPENDED,
        SUSPENDED,
        SUSPENDED,
        SUSPENDED,
        SUSPENDED,
        SUSPENDED,
        SUSPENDED
    };

    /* next thread */
    EE_TID EE_th_next[EE_MAX_TASK] = {
        EE_NIL,
        EE_NIL,
        EE_NIL,
        EE_NIL,
        EE_NIL,
        EE_NIL,
        EE_NIL
    };

    /* The first stacked task */
    EE_TID EE_stkfirst = EE_NIL;

    /* system ceiling */
    EE_TYPEPRIO EE_sys_ceiling= 0x0000U;

    /* The priority queues: (16 priorities maximum!) */
    EE_TYPEPAIR EE_rq_queues_head[EE_RQ_QUEUES_HEAD_SIZE] =
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    EE_TYPEPAIR EE_rq_queues_tail[EE_RQ_QUEUES_TAIL_SIZE] =
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    EE_TYPE_RQ_MASK EE_rq_bitmask = 0U;

    /* remaining nact: init= maximum pending activations of a Task */
    EE_TYPENACT EE_th_rnact[EE_MAX_TASK] = {
        1U,		 /* thread TransmitThread */
        1U,		 /* thread ReceiveThread */
        1U,		 /* thread ConfigSaveThread */
        1U,		 /* thread EthIsrHandler */
        1U,		 /* thread MgmtCmdHandler */
        1U,		 /* thread LinkStateHandler */
        1U		 /* thread TaskShell */
    };

    const EE_TYPENACT EE_th_rnact_max[EE_MAX_TASK] = {
        1U,		 /* thread TransmitThread */
        1U,		 /* thread ReceiveThread */
        1U,		 /* thread ConfigSaveThread */
        1U,		 /* thread EthIsrHandler */
        1U,		 /* thread MgmtCmdHandler */
        1U,		 /* thread LinkStateHandler */
        1U		 /* thread TaskShell */
    };

    EE_TYPEPRIO EE_rq_link[EE_MAX_TASK] =
        { 3U, 4U, 0U, 5U, 1U, 2U, 1U};

    /* The pairs that are enqueued into the priority queues (7 is the total number of task activations) */
    EE_TYPEPAIR EE_rq_pairs_next[EE_RQ_PAIRS_NEXT_SIZE] =
        { 1, 2, 3, 4, 5, 6, -1};

    /* no need to be initialized */
    EE_TID EE_rq_pairs_tid[EE_RQ_PAIRS_TID_SIZE];

    /* a list of unused pairs */
    EE_TYPEPAIR EE_rq_free = 0; /* pointer to a free pair */

    #ifndef __OO_NO_CHAINTASK__
        /* The next task to be activated after a ChainTask. initvalue=all EE_NIL */
        EE_TID EE_th_terminate_nextask[EE_MAX_TASK] = {
            EE_NIL,
            EE_NIL,
            EE_NIL,
            EE_NIL,
            EE_NIL,
            EE_NIL,
            EE_NIL
        };
    #endif



/***************************************************************************
 *
 * Event handling
 *
 **************************************************************************/
    EE_TYPEEVENTMASK EE_th_event_active[EE_MAX_TASK] =
        { 0U, 0U, 0U, 0U, 0U, 0U, 0U};    /* thread event already active */

    EE_TYPEEVENTMASK EE_th_event_waitmask[EE_MAX_TASK] =
        { 0U, 0U, 0U, 0U, 0U, 0U, 0U};    /* thread wait mask */

    EE_TYPEBOOL EE_th_waswaiting[EE_MAX_TASK] =
        { 0U, 0U, 0U, 0U, 0U, 0U, 0U};

    const EE_TYPEPRIO EE_th_is_extended[EE_MAX_TASK] =
        { 1U, 1U, 1U, 1U, 1U, 1U, 1U};



/***************************************************************************
 *
 * Mutex
 *
 **************************************************************************/
    /*
     * This is the last mutex that the task has locked. This array
     *    contains one entry for each task. Initvalue= all -1. at runtime,
     *    it points to the first item in the EE_resource_stack data structure.
    */
    EE_UREG EE_th_resource_last[EE_MAX_TASK] =
        { (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1};

    /*
     * This array is used to store a list of resources locked by a
     *    task. there is one entry for each resource, initvalue = -1. the
     *    list of resources locked by a task is ended by -1.
    */
    EE_UREG EE_resource_stack[EE_MAX_RESOURCE] =
        { (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1, (EE_UREG) -1};

    /*
     * Only in extended status or when using ORTI with resources; for
     *    each resource, a flag is allocated to see if the resource is locked or
     *    not.  Note that this information cannot be easily knew from the previous
     *    two data structures. initvalue=0
    */
    EE_TYPEBOOL EE_resource_locked[EE_MAX_RESOURCE] =
        { 0U, 0U, 0U, 0U, 0U, 0U};

    const EE_TYPEPRIO EE_resource_ceiling[EE_MAX_RESOURCE]= {
        0x10U,		/* resource AsyncNotificationResource */
        0x2U,		/* resource ConfigSaveResource */
        0x20U,		/* resource EthTxRingBufRes */
        0x10U,		/* resource IndirectAccessLock */
        0x8U,		/* resource MallocLock */
        0x10U 		/* resource TimeStampResource */
    };

    EE_TYPEPRIO EE_resource_oldceiling[EE_MAX_RESOURCE];



/***************************************************************************
 *
 * Counters
 *
 **************************************************************************/
    const EE_oo_counter_ROM_type EE_counter_ROM[EE_COUNTER_ROM_SIZE] = {
        {2147483647U, 1U, 1U}         /* SystemTimer */
    };

    EE_oo_counter_RAM_type       EE_counter_RAM[EE_MAX_COUNTER] = {
        {0U, (EE_TYPECOUNTEROBJECT)-1}
    };


/***************************************************************************
 *
 * Alarms
 *
 **************************************************************************/
    const EE_oo_alarm_ROM_type EE_alarm_ROM[EE_ALARM_ROM_SIZE] = {
        {0U}
    };


/***************************************************************************
 *
 * Counter Objects
 *
 **************************************************************************/
    const EE_oo_counter_object_ROM_type   EE_oo_counter_object_ROM[EE_COUNTER_OBJECTS_ROM_SIZE] = {

        {SystemTimer, TxAlarm, EE_ALARM }
    };

    EE_oo_counter_object_RAM_type EE_oo_counter_object_RAM[EE_COUNTER_OBJECTS_ROM_SIZE];


/***************************************************************************
 *
 * Alarms and Scheduling Tables actions
 *
 **************************************************************************/
    const EE_oo_action_ROM_type   EE_oo_action_ROM[EE_ACTION_ROM_SIZE] = {

        {EE_ACTION_EVENT   , TransmitThread, TransmitEvent, (EE_VOID_CALLBACK)NULL, (EE_TYPECOUNTER)-1 }
    };



/***************************************************************************
 *
 * AppMode
 *
 **************************************************************************/
    EE_TYPEAPPMODE EE_ApplicationMode;


/***************************************************************************
 *
 * Auto Start (TASK)
 *
 **************************************************************************/
    static const EE_TID EE_oo_autostart_task_mode_OSDEFAULTAPPMODE[EE_OO_AUTOSTART_TASK_MODE_OSDEFAULTAPPMODE_SIZE] = 
        { TransmitThread, LinkStateHandler, TaskShell };

    const struct EE_oo_autostart_task_type EE_oo_autostart_task_data[EE_MAX_APPMODE] = {
        { 3U, EE_oo_autostart_task_mode_OSDEFAULTAPPMODE}
    };


/***************************************************************************
 *
 * Auto Start (ALARM)
 *
 **************************************************************************/
    static const EE_TYPEALARM EE_oo_autostart_alarm_mode_OSDEFAULTAPPMODE[EE_OO_AUTOSTART_ALARM_MODE_OSDEFAULTAPPMODE_SIZE] =
        { TxAlarm };

    const struct EE_oo_autostart_alarm_type EE_oo_autostart_alarm_data[EE_MAX_APPMODE] = {
        { 1U, EE_oo_autostart_alarm_mode_OSDEFAULTAPPMODE}
    };


/***************************************************************************
 *
 * Init alarms parameters (ALARM)
 *
 **************************************************************************/

    const EE_TYPETICK EE_oo_autostart_alarm_increment[EE_MAX_ALARM] =
        {1U };

    const EE_TYPETICK EE_oo_autostart_alarm_cycle[EE_MAX_ALARM] =
        {5U };

