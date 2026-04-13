/**************************************************************************************************

    Phyplus Microelectronics Limited confidential and proprietary.
    All rights reserved.

    IMPORTANT: All rights of this software belong to Phyplus Microelectronics
    Limited ("Phyplus"). Your use of this Software is limited to those
    specific rights granted under  the terms of the business contract, the
    confidential agreement, the non-disclosure agreement and any other forms
    of agreements as a customer or a partner of Phyplus. You may not use this
    Software unless you agree to abide by the terms of these agreements.
    You acknowledge that the Software may not be modified, copied,
    distributed or disclosed unless embedded on a Phyplus Bluetooth Low Energy
    (BLE) integrated circuit, either as a product or is integrated into your
    products.  Other than for the aforementioned purposes, you may not use,
    reproduce, copy, prepare derivative works of, modify, distribute, perform,
    display or sell this Software and/or its documentation for any purposes.

    YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
    PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
    INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
    NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
    PHYPLUS OR ITS SUBSIDIARIES BE LIABLE OR OBLIGATED UNDER CONTRACT,
    NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
    LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
    INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
    OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
    OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
    (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

**************************************************************************************************/

/*******************************************************************************
    @file     flash.c
    @brief    Contains all functions support for flash driver
    @version  0.0
    @date     27. Nov. 2017
    @author   qing.han



*******************************************************************************/
#include "rom_sym_def.h"
#include <string.h>
#include "types.h"
#include "flash.h"
#include "log.h"
#include "pwrmgr.h"
#include "error.h"

#define SPIF_WAIT_IDLE_CYC                          (32)

#define SPIF_STATUS_WAIT_IDLE(n)                    \
    do                                              \
    {                                               \
        while((AP_SPIF->fcmd &0x02)==0x02);         \
        {                                           \
            volatile int delay_cycle = n;           \
            while (delay_cycle--){;}                \
        }                                           \
        while ((AP_SPIF->config & 0x80000000) == 0);\
    } while (0);


#define HAL_CACHE_ENTER_BYPASS_SECTION()  do{ \
        HAL_ENTER_CRITICAL_SECTION();\
        AP_CACHE->CTRL0 = 0x02; \
        AP_PCR->CACHE_RST = 0x02;\
        AP_PCR->CACHE_BYPASS = 1;    \
        HAL_EXIT_CRITICAL_SECTION();\
    }while(0);


#define HAL_CACHE_EXIT_BYPASS_SECTION()  do{ \
        HAL_ENTER_CRITICAL_SECTION();\
        AP_CACHE->CTRL0 = 0x00;\
        AP_PCR->CACHE_RST = 0x03;\
        AP_PCR->CACHE_BYPASS = 0;\
        HAL_EXIT_CRITICAL_SECTION();\
    }while(0);


static xflash_Ctx_t s_xflashCtx = {.rd_instr=XFRD_FCMD_READ_DUAL};

static uint32_t flash_ID = 0;

chipMAddr_t  g_chipMAddr;
extern void ll_patch_restore(uint8_t flg);
__ATTR_SECTION_SRAM__  static inline uint32_t spif_lock()
{
    HAL_ENTER_CRITICAL_SECTION();
    uint32_t vic_iser = NVIC->ISER[0];
    //mask all irq
    NVIC->ICER[0] = 0xFFFFFFFF;
    //enable ll irq and tim1 irq
    NVIC->ISER[0] = 0x100010;
    ll_patch_restore(0);//clean ll patch
    HAL_EXIT_CRITICAL_SECTION();
    return vic_iser;
}

__ATTR_SECTION_SRAM__  static inline void spif_unlock(uint32_t vic_iser)
{
    HAL_ENTER_CRITICAL_SECTION();
    NVIC->ISER[0] = vic_iser;
    ll_patch_restore(1);//restore ll patch
    HAL_EXIT_CRITICAL_SECTION();
}

void hal_cache_tag_flush(void)
{
    HAL_ENTER_CRITICAL_SECTION();
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    volatile int dly = 8;

    if(cb==0)
    {
        AP_PCR->CACHE_BYPASS = 1;
    }

    AP_CACHE->CTRL0 = 0x02;

    while (dly--) {;};

    AP_CACHE->CTRL0 = 0x03;

    dly = 8;

    while (dly--) {;};

    AP_CACHE->CTRL0 = 0x00;

    if(cb==0)
    {
        AP_PCR->CACHE_BYPASS = 0;
    }

    HAL_EXIT_CRITICAL_SECTION();
}


static uint8_t _spif_read_status_reg_x(bool rdst_h)
{
    uint8_t status;

    if (rdst_h)
    {
        spif_cmd(FCMD_RDST_H, 0, 2, 0, 0, 0);
        SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
        spif_rddata(&status, 1);
    }
    else
    {
        spif_cmd(FCMD_RDST, 0, 2, 0, 0, 0);
        SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
        spif_rddata(&status, 1);
    }

    return status;
}

static int _spif_wait_nobusy_x(uint8_t flg, uint32_t tout_ns)
{
    uint8_t status;
    volatile int tout = (int )(tout_ns);

    for(; tout ; tout --)
    {
        status = _spif_read_status_reg_x(FALSE);

        if((status & flg) == 0)
            return PPlus_SUCCESS;

        //insert polling interval
        //5*32us
        WaitRTCCount(5);
    }

    return PPlus_ERR_BUSY;
}

static void hal_cache_init(void)
{
    volatile int dly=100;
    //clock gate
    hal_clk_gate_enable(MOD_HCLK_CACHE);
    hal_clk_gate_enable(MOD_PCLK_CACHE);
    HAL_ENTER_CRITICAL_SECTION();
    //cache rst ahp
    AP_PCR->CACHE_RST=0x02;

    while(dly--) {};

    AP_PCR->CACHE_RST=0x03;

    hal_cache_tag_flush();

    //cache enable
    AP_PCR->CACHE_BYPASS = 0;
    HAL_EXIT_CRITICAL_SECTION();
}

int hal_get_flash_info(void)
{
#if(FLASH_PROTECT_FEATURE == 0)
    spif_read_id(&flash_ID);
#endif
    return PPlus_SUCCESS;
}

#if(FLASH_PROTECT_FEATURE == 1)
static FLASH_PROTECT_INFO flash_protect_data =
{
    .bypass_flash_lock = FALSE,
    .module_ID = MAIN_INIT,
};

static void hal_flash_enable_wp(bool wp_en)
{
    if (wp_en == TRUE)
    {
        subWriteReg(&AP_SPIF->config, 14, 14, 1); // enable wp bit(14)
    }
    else
    {
        subWriteReg(&AP_SPIF->config, 14, 14, 0); // disable wp bit(14)
    }
}

int hal_flash_enable_lock(module_ID_t id)
{
    uint8_t ret = PPlus_ERR_BUSY;

    if ((flash_protect_data.module_ID == id) || (flash_protect_data.module_ID == MAIN_INIT))
    {
        flash_protect_data.bypass_flash_lock = FALSE;
        ret = hal_flash_lock();
        if (s_xflashCtx.rd_instr == XFRD_FCMD_READ_DUAL)
        {
            hal_flash_enable_wp(TRUE);
        }
    }

    return ret;
}

int hal_flash_disable_lock(module_ID_t id)
{
    uint8_t ret = PPlus_SUCCESS;
    ret = hal_flash_unlock();

    if (ret != PPlus_SUCCESS)
    {
        return ret;
    }

    flash_protect_data.module_ID = id;
    flash_protect_data.bypass_flash_lock = TRUE;
    return ret;
}

int hal_flash_write_status_register(uint16_t reg_data, bool high_en)
{
    if (AP_SPIF->config & BIT(14)) //if WP protection is enabled, SR registers cannot be modified.
        return PPlus_ERR_FATAL;

    uint32_t cs = spif_lock();
    uint8_t data[2];
    spif_cmd(FCMD_WREN, 0, 0, 0, 0, 0);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    data[0] = (uint8_t)(reg_data & 0xff);
    data[1] = (uint8_t)((reg_data & 0xff00) >> 8);
    if (high_en == FALSE)
    {
        spif_wrdata(data, 1);
        spif_cmd(FCMD_WRST, 0, 0, 1, 0, 0);
    }
    else
    {

        if ((flash_ID & 0xffffff) == P25Q16SU_ID)
        {
            spif_wrdata(&data[0], 1);
            spif_cmd(FCMD_WRST, 0, 0, 1, 0, 0);
            _spif_wait_nobusy_x(SFLG_WELWIP, SPIF_TIMEOUT);
            
            spif_cmd(FCMD_WREN, 0, 0, 0, 0, 0);
            _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
            spif_wrdata(&data[1], 1);
            spif_cmd(0x31, 0, 0, 1, 0, 0);
        }
        else
        {
            spif_wrdata(data, 2);
            spif_cmd(FCMD_WRST, 0, 0, 2, 0, 0);
        }
    }
    _spif_wait_nobusy_x(SFLG_WELWIP, SPIF_TIMEOUT);
    spif_unlock(cs);
    return PPlus_SUCCESS;
}

int hal_flash_lock(void)
{
    if (flash_ID == 0)
    {
        spif_read_id(&flash_ID);
    }
    if (flash_protect_data.bypass_flash_lock == TRUE)
    {
        return PPlus_ERR_FORBIDDEN;
    }
    bool srreg_high_en = FALSE;
    uint8_t sr_reg_h = 0;
    uint8_t flash_lock_status = hal_flash_get_lock_state();
    uint16_t flashlock_value = 0;
    switch (flash_ID & 0xffffff)
    {
        case XT25W02D_ID:
            if (flash_lock_status != FLASH_PROTECT_AREA_2BIT_0C)
            {
                flashlock_value = FLASH_PROTECT_AREA_2BIT_0C;
                srreg_high_en = FALSE;
            }
            else
            {
                return PPlus_SUCCESS;
            }
            break;
        case XT25W04D_ID: case GD25WD40CGIG_GJG_ID: case GD25WD80CGIG_ID: case GT25D20E_ID:
            if (flash_lock_status != FLASH_PROTECT_AREA_3BIT_1C)
            {
                flashlock_value = FLASH_PROTECT_AREA_3BIT_1C;
                srreg_high_en = FALSE;
            }
            else
            {
                return PPlus_SUCCESS;
            }
            break;
        case GT25Q40C_ID:
            sr_reg_h = hal_flash_get_high_lock_state();
            if ((flash_lock_status != FLASH_PROTECT_AREA_5BIT_5C) || ((sr_reg_h & SR_CMP_BIT) != 0))
            {
                flashlock_value = FLASH_PROTECT_AREA_5BIT_5C;
                srreg_high_en = TRUE;
                if ((sr_reg_h & SR_QE_BIT) != 0)
                {
                    flashlock_value |= SET_QE_BIT;
                }
            }
            else
            {
                return PPlus_SUCCESS;
            }
            break;
        case P25D22U_ID:
            if (flash_lock_status != FLASH_PROTECT_AREA_5BIT_7C)
            {
                flashlock_value = FLASH_PROTECT_AREA_5BIT_7C;
                srreg_high_en = FALSE;
            }
            else
            {
                return PPlus_SUCCESS;
            }
            break;
        case TH25D20UA_ID: case TH25D40HB_ID: case UC25HQ40IAG_ID: case TH25Q40UA_ID: case P25Q16SU_ID: case P25D40_ID:
            sr_reg_h = hal_flash_get_high_lock_state();
            if ((flash_lock_status != FLASH_PROTECT_AREA_5BIT_7C) || ((sr_reg_h & SR_CMP_BIT) != 0))
            {
                flashlock_value = FLASH_PROTECT_AREA_5BIT_7C;
                srreg_high_en = TRUE;
                if ((sr_reg_h & SR_QE_BIT) != 0)
                {
                    flashlock_value |= SET_QE_BIT;
                }
            }
            else
            {
                return PPlus_SUCCESS;
            }
            break;
        default:
            while (1); /**the current flash type cannot be found**/
    }
    if (((flash_ID & 0xffffff) != XT25W02D_ID) && ((flash_ID & 0xffffff) != XT25W04D_ID))
    {
        flashlock_value |= SR_SRP0_BIT;
    }
    if (s_xflashCtx.rd_instr == XFRD_FCMD_READ_DUAL)
    {
        hal_flash_enable_wp(FALSE);
    }
    hal_flash_write_status_register(flashlock_value, srreg_high_en);
    return PPlus_SUCCESS;
}

int hal_flash_unlock(void)
{
    if (flash_protect_data.bypass_flash_lock == TRUE)
    {
        return PPlus_ERR_FORBIDDEN;
    }
    uint32_t fcmd_wrdata = 0x00;
    if (s_xflashCtx.rd_instr == XFRD_FCMD_READ_DUAL)
    {
        hal_flash_enable_wp(FALSE);
    }
    switch (flash_ID & 0xffffff)
    {
        case XT25W02D_ID: case XT25W04D_ID:
            hal_flash_write_status_register(fcmd_wrdata, FALSE);
            break;
        case GD25WD40CGIG_GJG_ID: case GD25WD80CGIG_ID: case P25D22U_ID: case GT25D20E_ID:
            hal_flash_write_status_register(fcmd_wrdata, FALSE);
            if (s_xflashCtx.rd_instr == XFRD_FCMD_READ_DUAL)
            {
                hal_flash_enable_wp(TRUE);
            }
            break;
        case GT25Q40C_ID: case TH25D20UA_ID: case TH25D40HB_ID: case UC25HQ40IAG_ID: case TH25Q40UA_ID: case P25Q16SU_ID: case P25D40_ID:
            if ((hal_flash_get_high_lock_state() & SR_QE_BIT) != 0)
            {
                fcmd_wrdata |= SET_QE_BIT;
            }
            hal_flash_write_status_register(fcmd_wrdata, TRUE);
            if (s_xflashCtx.rd_instr == XFRD_FCMD_READ_DUAL)
            {
                hal_flash_enable_wp(TRUE);
            }
            break;
        default:
            break;
    }
    return PPlus_SUCCESS;
}

uint8_t hal_flash_get_high_lock_state(void)
{
    uint32_t cs = spif_lock();
    uint8_t status;
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    status = _spif_read_status_reg_x(TRUE);
    spif_unlock(cs);
    return status;
}

uint8_t hal_flash_get_lock_state(void)
{
    uint32_t cs = spif_lock();
    uint8_t status;
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    status = _spif_read_status_reg_x(FALSE);
    spif_unlock(cs);
    return status;
}

int hal_remove_fct_mode(uint32_t sramaddr)
{
    uint8_t s_fct_mode = *(volatile uint8_t *)0x1fff0800;
    if (s_fct_mode == 1)
    {
        if ((sramaddr & 0xffff0000) == SRAM0_BASE_ADDRESS)
        {
            memset((uint8_t *)sramaddr, 0xff, 0x1000);
        }
        else
        {
            return PPlus_ERR_INVALID_PARAM;
        }
        if (flash_ID == 0)
        {
            spif_read_id(&flash_ID);
        }
        AP_PCR->CACHE_BYPASS = 1;
        memcpy((uint8_t *)sramaddr, (uint8_t *)0x11002000, 0x1000);
        memset((uint8_t *)(sramaddr+0x800), 0xff, 4);
        hal_flash_erase_sector(0x11002000);
        hal_flash_write_by_dma(0x11002000, (uint8_t *)sramaddr, 0x1000);
        hal_system_soft_reset();
    }
    return PPlus_SUCCESS;
}
#endif

#ifdef XFLASH_HIGH_SPEED
static void hw_spif_config_high_speed(sysclk_t ref_clk)
{
    volatile uint32_t tmp = AP_SPIF->config;
    tmp =  (tmp & (~ (0xf << 19))) | (0 << 19);
    AP_SPIF->config = tmp;
    subWriteReg(&AP_SPIF->rddata_capture, 4, 1, 2);
}
#endif

static void hw_spif_cache_config(void)
{
    extern volatile sysclk_t g_system_clk;
    sysclk_t spif_ref_clk = (g_system_clk > SYS_CLK_XTAL_16M) ? SYS_CLK_DLL_96M : g_system_clk;

    if(s_xflashCtx.rd_instr == XFRD_FCMD_READ_QUAD)
    {
        spif_config(spif_ref_clk,/*div*/1,s_xflashCtx.rd_instr,0,1);
        #if(FLASH_PROTECT_FEATURE == 1)
        hal_flash_enable_lock(MAIN_INIT);
        #endif
    }
    else
    {
        spif_config(spif_ref_clk,/*div*/1,s_xflashCtx.rd_instr,0,0);
        #if (FLASH_PROTECT_FEATURE == 1)
        hal_flash_enable_wp(TRUE);
        #endif
    }
    #ifdef XFLASH_HIGH_SPEED
    hw_spif_config_high_speed(spif_ref_clk);
    #endif
    AP_SPIF->wr_completion_ctrl=0xff010005;//set longest polling interval
    AP_SPIF->low_wr_protection = 0;
    AP_SPIF->up_wr_protection = 0x10;
    AP_SPIF->wr_protection = 0x2;
    NVIC_DisableIRQ(SPIF_IRQn);
    NVIC_SetPriority((IRQn_Type)SPIF_IRQn, IRQ_PRIO_HAL);
    hal_cache_init();
    hal_get_flash_info();
}

extern spif_wakeup_hdl  spifWakeupHdl;
void spif_wakeup_hdl_register( spif_wakeup_hdl pfnHdl )
{
    spifWakeupHdl = pfnHdl;
}

int hal_spif_cache_init(xflash_Ctx_t cfg)
{
    memset(&(s_xflashCtx), 0, sizeof(s_xflashCtx));
    memcpy(&(s_xflashCtx), &cfg, sizeof(s_xflashCtx));
    hw_spif_cache_config();
    spif_wakeup_hdl_register(hw_spif_cache_config); /*315 add: call spif wakeup handler before set TMER1*/
    // hal_pwrmgr_register(MOD_SPIF, NULL,  hw_spif_cache_config);
    return PPlus_SUCCESS;
}


int hal_flash_read(uint32_t addr, uint8_t* data, uint32_t size)
{
    uint32_t cs = spif_lock();
    volatile uint8_t* u8_spif_addr = (volatile uint8_t*)((addr & 0x7ffff) | FLASH_BASE_ADDR);
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    uint32_t remap;

    if((addr & 0xffffff) > 0x7ffff)
    {
        remap = addr & 0xf80000;

        if(remap)
        {
            AP_SPIF->remap = remap;
            AP_SPIF->config |= 0x10000;
        }
    }

    //read flash addr direct access
    //bypass cache
    if(cb == 0)
    {
        HAL_CACHE_ENTER_BYPASS_SECTION();
    }

    for(int i=0; i<size; i++)
        data[i]=u8_spif_addr[i];

    //bypass cache
    if(cb == 0)
    {
        HAL_CACHE_EXIT_BYPASS_SECTION();
    }

    if(((addr & 0xffffff) > 0x7ffff) && remap)
    {
        AP_SPIF->remap = 0;
        AP_SPIF->config &= ~0x10000ul;
    }

    spif_unlock(cs);
    return PPlus_SUCCESS;
}

int hal_flash_write(uint32_t addr, uint8_t* data, uint32_t size)
{
    uint8_t retval;
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_unlock();
    #endif
    uint32_t cs = spif_lock();
    HAL_CACHE_ENTER_BYPASS_SECTION();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    retval = spif_write(addr,data,size);
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    HAL_CACHE_EXIT_BYPASS_SECTION();
    spif_unlock(cs);
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_lock();
    #endif
    return retval;
}

typedef struct{
 uint8_t state;
 uint32_t size;
 
 bool w_protect;
 
 uint8_t clk_div;
 
 uint32_t qspi_en;
 uint32_t rd_instr;
 uint32_t wr_instr;
 uint32_t remap;
 
 /*device id*/
 uint8_t mid;
 uint8_t mtype; //mempry type
 uint8_t mcapc; //memory capcity
 
}spif_ctx_t;
extern spif_ctx_t s_spif_ctx;
static void dma_cfg_ch(uint32_t srcaddr, uint32_t dstaddr, uint32_t cnt)
{
    AP_DMA_INT->ClearTfr = ((1UL<<3)&0xFF);
    AP_DMA_CH_CFG(3)->SAR = srcaddr;
    AP_DMA_CH_CFG(3)->DAR = dstaddr;
    AP_DMA_CH_CFG(3)->LLP = 0;

    AP_DMA_CH_CFG(3)->CTL = BIT(25) | 1;                  //int_en 1, DST_TR_WIDTH 1byte, SRC_TR_WIDTH 1byte, DINC increment, SINC increment, m2m
    AP_DMA_CH_CFG(3)->CTL_H = cnt;              //BLOCK_TS: 1byte
    AP_DMA_CH_CFG(3)->CFG = 0;


    AP_DMA_CH_CFG(3)->CFG_H = 4;                //dst per = 0, src per = 0

    AP_DMA_MISC->DmaCfgReg = 1;                 //dma enable
    AP_DMA_MISC->ChEnReg = 0x808;               //channel enable

    while(1){
        if(AP_DMA_INT->RawTfr & BIT(3))
            break;
    }

    //clear int status
    AP_DMA_INT->ClearTfr = ((1UL<<3)&0xFF);
    
    AP_DMA_MISC->ChEnReg = 0x800;
}

static int spif_write_dma(uint32_t addr, uint8_t* data, uint32_t size)
{
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    uint32_t remap = addr &0xf80000;
    uint32_t spif_ad = ((addr&0x7ffff)|SPIF_BASE_ADDR);
    uint32_t ram_ad = (uint32_t)data;
    uint32_t len = 0;
        
    if(s_spif_ctx.w_protect)
        return PPlus_ERR_ACCESS_REJECTED;

    //bypass cache
    if(cb == 0)
        AP_PCR->CACHE_BYPASS = 1;

    //remap
    if(remap)
    {
        AP_SPIF->remap = remap;
        AP_SPIF->config |= 0x10000;
    }

    //spif_write_protect(FALSE);
    AP_SPIF->wr_protection = 0;
    
    while(size)
    {
        len = size > 128 ? 128 :size;
        dma_cfg_ch(ram_ad, spif_ad, len);
        size -= len;
        ram_ad += len;
        spif_ad += len;
    }

    if(remap)
    {
        AP_SPIF->remap = 0;
        AP_SPIF->config &= ~0x10000ul;
    }

    //_spif_write_protect(TRUE);
    AP_SPIF->wr_protection = 2;

    if(cb == 0)
        AP_PCR->CACHE_BYPASS = 0;
    return PPlus_SUCCESS;
}

int hal_flash_write_by_dma(uint32_t addr, uint8_t* data, uint32_t size)
{
    uint8_t retval;
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_unlock();
    #endif

    uint32_t cs = spif_lock();
    HAL_CACHE_ENTER_BYPASS_SECTION();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    retval = spif_write_dma(addr,data,size);
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    HAL_CACHE_EXIT_BYPASS_SECTION();
    spif_unlock(cs);

    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_lock();
    #endif
    return retval;
}

int hal_flash_erase_sector(unsigned int addr)
{
    uint8_t retval;
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_unlock();
    #endif
    uint32_t cs = spif_lock();
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    HAL_CACHE_ENTER_BYPASS_SECTION();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    retval = spif_erase_sector(addr);
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WELWIP, SPIF_TIMEOUT);
    HAL_CACHE_EXIT_BYPASS_SECTION();

    if(cb == 0)
    {
        hal_cache_tag_flush();
    }

    spif_unlock(cs);
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_lock();
    #endif
    return retval;
}

int hal_flash_erase_block64(unsigned int addr)
{
    uint8_t retval;
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_unlock();
    #endif
    uint32_t cs = spif_lock();
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    HAL_CACHE_ENTER_BYPASS_SECTION();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    retval = spif_erase_block64(addr);
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WELWIP, SPIF_TIMEOUT);
    HAL_CACHE_EXIT_BYPASS_SECTION();

    if(cb == 0)
    {
        hal_cache_tag_flush();
    }

    spif_unlock(cs);
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_lock();
    #endif
    return retval;
}

int hal_flash_erase_all(void)
{
    uint8_t retval;
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_unlock();
    #endif
    uint32_t cs = spif_lock();
    uint32_t cb = AP_PCR->CACHE_BYPASS;
    HAL_CACHE_ENTER_BYPASS_SECTION();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WIP, SPIF_TIMEOUT);
    retval = spif_erase_all();
    SPIF_STATUS_WAIT_IDLE(SPIF_WAIT_IDLE_CYC);
    _spif_wait_nobusy_x(SFLG_WELWIP, SPIF_TIMEOUT);
    HAL_CACHE_EXIT_BYPASS_SECTION();

    if(cb == 0)
    {
        hal_cache_tag_flush();
    }

    spif_unlock(cs);
    #if(FLASH_PROTECT_FEATURE == 1)
    hal_flash_lock();
    #endif
    return retval;
}

int flash_write_word(unsigned int offset, uint32_t  value)
{
    uint32_t temp = value;
    offset &= 0x00ffffff;
    return (hal_flash_write_by_dma (offset, (uint8_t*) &temp, 4));
}

CHIP_ID_STATUS_e read_chip_mAddr(void)
{
    CHIP_ID_STATUS_e ret = CHIP_ID_UNCHECK;
    uint8_t b;

    for(int i=0; i<CHIP_MADDR_LEN; i++)
    {
        ret = chip_id_one_bit_hot_convter(&b, read_reg(CHIP_MADDR_FLASH_ADDRESS+(i<<2)));

        if(ret==CHIP_ID_VALID)
        {
            g_chipMAddr.mAddr[CHIP_MADDR_LEN-1-i]=b;
        }
        else
        {
            if(i>0 && ret==CHIP_ID_EMPTY)
            {
                ret =CHIP_ID_INVALID;
            }

            return ret;
        }
    }

    return ret;
}

void check_chip_mAddr(void)
{
    //chip id check
    for(int i=0; i<CHIP_MADDR_LEN; i++)
    {
        g_chipMAddr.mAddr[i]=0xff;
    }

    g_chipMAddr.chipMAddrStatus=read_chip_mAddr();
}

void LOG_CHIP_MADDR(void)
{
    LOG("\n");

    if(g_chipMAddr.chipMAddrStatus==CHIP_ID_EMPTY)
    {
        LOG("[CHIP_MADDR EMPTY]\n");
    }
    else if(g_chipMAddr.chipMAddrStatus==CHIP_ID_INVALID)
    {
        LOG("[CHIP_MADDR INVALID]\n");
    }
    else if(g_chipMAddr.chipMAddrStatus==CHIP_ID_VALID)
    {
        LOG("[CHIP_MADDR VALID]\n");

        for(int i=0; i<CHIP_MADDR_LEN; i++)
        {
            LOG("%02x",g_chipMAddr.mAddr[i]);
        }

        LOG("\n");
    }
    else
    {
        LOG("[CHIP_MADDR UNCHECKED]\n");
    }
}

