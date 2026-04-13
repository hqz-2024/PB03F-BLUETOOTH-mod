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

/**************************************************************************************************
    Filename:       gpio_demo.c
    Revised:        $Date $
    Revision:       $Revision $


**************************************************************************************************/

/*********************************************************************
    INCLUDES
*/

#include "OSAL.h"
#include "i2c_dma_demo.h"
#include "log.h"
#include "gpio.h"
#include "clock.h"
#include "pwrmgr.h"
#include "error.h"
#include "key.h"
#include "flash.h"
#include "i2c.h"
#include "pwrmgr.h"
#include "error.h"
#include "dma.h"

#define I2C_MASTER_SDA P33
#define I2C_MASTER_CLK P34
#define slave_i2c_addr 0x50
#define REG_ADDR 0

static void* master_pi2c;
static uint8_t i2c_TaskID;
static uint8_t i2c_mode=I2C_MODE_TX;

#define DATA_LEN  255
static uint8 i2c_tx_buf[DATA_LEN];
static uint8 i2c_rx_buf[DATA_LEN];

__ATTR_SECTION_SRAM__ static void dma_chx_cb(DMA_CH_t ch)
{
    if(ch==DMA_CH_0)
    {
        // LOG("\ndma ch0 done!!!\n");

        if(i2c_mode==I2C_MODE_TX)
        {
            osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_READ_DATA_EVT, 5);
        }
        else if(i2c_mode==I2C_MODE_RX)
        {
            osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_RX_DATA_EVT, 5);
        }
    }
}

uint8_t i2c_dma_tx_config(i2c_dev_t i2cx,uint8_t* tx_buf,uint16_t tx_len,DMA_CH_t dma_ch,uint8_t dma_int)
{
    HAL_DMA_t ch_cfg;
    DMA_CH_CFG_t cfg;
    ch_cfg.dma_channel = dma_ch;
    ch_cfg.evt_handler = dma_chx_cb;
    hal_dma_init_channel(ch_cfg);
    AP_I2C_TypeDef* I2cx = NULL;
    I2cx = (i2cx == I2C_0) ? AP_I2C0 : AP_I2C1;
    I2cx->IC_DMA_CR &= 0x00;
    cfg.transf_size = tx_len;
    cfg.sinc = DMA_INC_INC;
    cfg.src_tr_width = DMA_WIDTH_BYTE;
    cfg.src_msize = DMA_BSIZE_1;
    cfg.src_addr = (uint32_t)tx_buf;
    cfg.dinc = DMA_INC_NCHG;
    cfg.dst_tr_width = DMA_WIDTH_BYTE;
    cfg.dst_msize = DMA_BSIZE_1;
    cfg.dst_addr =(uint32_t)&(I2cx->IC_DATA_CMD);
    cfg.enable_int = dma_int;
    I2cx->IC_DMA_CR |= 0x02;
    I2cx->IC_DMA_TDLR = 1;
    uint8_t retval = hal_dma_config_channel(dma_ch,&cfg);
    LOG("dam tx config ret is :%d\n",retval);
    return retval;
}
uint32_t i2c_cmd_tx_buf = 0x100;
uint8_t i2c_dma_tx_cmd_config(i2c_dev_t i2cx,uint16_t tx_len,DMA_CH_t dma_ch,uint8_t dma_int)
{
    //uint32_t tx_buf = 0x100;
    HAL_DMA_t ch_cfg;
    DMA_CH_CFG_t cfg;
    ch_cfg.dma_channel = dma_ch;
    ch_cfg.evt_handler = dma_chx_cb;
    hal_dma_init_channel(ch_cfg);
    AP_I2C_TypeDef* I2cx = NULL;
    I2cx = (i2cx == I2C_0) ? AP_I2C0 : AP_I2C1;
    //I2cx->IC_DMA_CR &= 0x00;
    cfg.transf_size = tx_len;
    cfg.sinc = DMA_INC_NCHG;
    cfg.src_tr_width = DMA_WIDTH_WORD;
    cfg.src_msize = DMA_BSIZE_1;
    cfg.src_addr = (uint32_t)&i2c_cmd_tx_buf;
    cfg.dinc = DMA_INC_NCHG;
    cfg.dst_tr_width = DMA_WIDTH_WORD;
    cfg.dst_msize = DMA_BSIZE_1;
    cfg.dst_addr =(uint32_t)&(I2cx->IC_DATA_CMD);
    cfg.enable_int = dma_int;
    I2cx->IC_DMA_CR |= 0x02;
    I2cx->IC_DMA_TDLR = 1;
    uint8_t retval = hal_dma_config_channel(dma_ch,&cfg);
    //LOG("dam tx config ret is :%d\n",retval);
    return retval;
}

uint8_t i2c_dma_rx_config(i2c_dev_t i2cx,uint8_t* rx_buf,uint16_t rx_len,DMA_CH_t dma_ch,uint8_t dma_int)
{
    HAL_DMA_t ch_cfg;
    DMA_CH_CFG_t cfg;
    ch_cfg.dma_channel = dma_ch;
    ch_cfg.evt_handler = dma_chx_cb;
    hal_dma_init_channel(ch_cfg);
    AP_I2C_TypeDef* I2cx = NULL;
    I2cx = (i2cx == I2C_0) ? AP_I2C0 : AP_I2C1;
    I2cx->IC_DMA_CR  &= 0x02;
    cfg.transf_size = rx_len;
    cfg.sinc = DMA_INC_NCHG;
    cfg.src_tr_width = DMA_WIDTH_BYTE;
    cfg.src_msize = DMA_BSIZE_1;
    cfg.src_addr = (uint32_t)&(I2cx->IC_DATA_CMD);
    cfg.dinc = DMA_INC_INC;
    cfg.dst_tr_width = DMA_WIDTH_BYTE;
    cfg.dst_msize = DMA_BSIZE_1;
    cfg.dst_addr =(uint32_t)rx_buf;
    cfg.enable_int = dma_int;
    I2cx->IC_DMA_CR |= 0x01;
    I2cx->IC_DMA_TDLR = 0;
    uint8_t retval = hal_dma_config_channel(dma_ch,&cfg);
    LOG("dam rx config ret is :%d\n",retval);
    return retval;
}
int I2CWrite(void* pi2c,  uint8* data,uint8 len,uint8 slave_addr)
{
    i2c_dev_t i2cx= (pi2c == AP_I2C0) ? I2C_0 :I2C_1;
    int ret = false;
    hal_i2c_addr_update(pi2c,slave_addr);
    hal_dma_deinit();
    hal_dma_init();
    if (i2c_dma_tx_config(i2cx, data, len, DMA_CH_1, FALSE) == PPlus_SUCCESS)
    {
        // hal_i2c_send(master_pi2c, (uint8*)&reg_addr,1);           //����24c02�豸��ַ--д
        hal_dma_stop_channel(DMA_CH_1);
        ret = hal_dma_start_channel(DMA_CH_1);
        LOG("tx start ret is %d\n", ret);

        ret = hal_dma_wait_channel_complete(DMA_CH_1);
        ret = (ret == PPlus_SUCCESS) ? true : false;
    }

    return ret;
}

 int I2CRead(void* pi2c,  uint8* data,uint8 len,uint8 slave_addr)
{
    i2c_dev_t i2cx= (pi2c == AP_I2C0) ? I2C_0 :I2C_1;
    int ret = true;
    //ret=hal_i2c_read_x(pi2c,slave_addr,data,len);
	
	hal_dma_deinit();
    hal_dma_init();
	if(i2c_dma_rx_config(i2cx,data,len,DMA_CH_2,FALSE)==PPlus_SUCCESS)
	{
		i2c_mode=I2C_MODE_RX;
		hal_i2c_addr_update(pi2c,slave_addr);   		    
		hal_dma_stop_channel(DMA_CH_2);
		ret=hal_dma_start_channel(DMA_CH_2);

     #if 0   
        extern void hal_master_send_read_cmd(void* pi2c, uint8_t len);
        hal_master_send_read_cmd(pi2c,len);
    #else

        if (i2c_dma_tx_cmd_config(i2cx, len, DMA_CH_1, FALSE) == PPlus_SUCCESS)
        {
            // hal_i2c_send(master_pi2c, (uint8*)&reg_addr,1);           //����24c02�豸��ַ--д
            hal_dma_stop_channel(DMA_CH_1);
            // AP_I2C_TypeDef* pi2cdev = (AP_I2C_TypeDef*)pi2c;
            // I2C_READ_CMD(pi2cdev);
            // I2C_READ_CMD(pi2cdev);
            // I2C_READ_CMD(pi2cdev);
     
            ret = hal_dma_start_channel(DMA_CH_1);
            //LOG("tx start ret is %d\n", ret);

            ret = hal_dma_wait_channel_complete(DMA_CH_1);
            //LOG("wt ch1 %d\n", ret);
            ret = (ret == PPlus_SUCCESS) ? true : false;
        }
    #endif
    
        ret = hal_dma_wait_channel_complete(DMA_CH_2);
        //LOG("wt ch2 %d\n", ret);
        ret=(ret==PPlus_SUCCESS)?true:false;
    
		//LOG("rx start ret is %d\n",ret);
        //LOG("r s %d\n",ret);// !!!!! important to as delay function
        //WaitUs(900);
        //WaitMs(2);
	}

    return ret;
}
void I2c_Dma_Demo_Init(uint8 task_id)
{
    i2c_TaskID = task_id;
    LOG("i2c demo start...\n");
    hal_dma_deinit();
    hal_dma_init();
    hal_gpio_pin_init(I2C_MASTER_SDA,IE);
    hal_gpio_pin_init(I2C_MASTER_CLK,IE);
    hal_gpio_pull_set(I2C_MASTER_SDA,STRONG_PULL_UP);
    hal_gpio_pull_set(I2C_MASTER_CLK,STRONG_PULL_UP);
    hal_i2c_pin_init(I2C_0, I2C_MASTER_SDA, I2C_MASTER_CLK);
    //master_pi2c=hal_i2c_init(I2C_0,I2C_CLOCK_400K);
     master_pi2c=hal_i2c_init_clk(I2C_0,1000000);

    for (uint8_t i = 0; i < DATA_LEN; i++)
    {
        i2c_tx_buf[i] = i;
    }

    if(master_pi2c==NULL)
    {
        LOG("I2C master init fail\n");
    }
    else
    {
        LOG("I2C master init OK\n");
        osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_WRITE_DATA_EVT, 10);
    }
}
static uint8_t i2c_tx_buf_len = 128;
uint16 I2c_Dma_ProcessEvent( uint8 task_id, uint16 events )
{
    if(task_id != i2c_TaskID)
    {
        return 0;
    }

    if( events & KEY_I2C_DMA_WRITE_DATA_EVT)
    {
        LOG("KEY_I2C_WRITE_DATA_EVT\n");
        i2c_tx_buf_len++;
        i2c_tx_buf_len = i2c_tx_buf_len==0 ? 2:i2c_tx_buf_len;
        
        I2CWrite(master_pi2c,i2c_tx_buf,i2c_tx_buf_len,slave_i2c_addr);
        osal_set_event(i2c_TaskID, KEY_I2C_DMA_READ_DATA_EVT);
        //osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_READ_DATA_EVT, 5);

        return (events ^ KEY_I2C_DMA_WRITE_DATA_EVT);
    }

    if( events & KEY_I2C_DMA_READ_DATA_EVT)
    {
        LOG("KEY_I2C_READ_DATA_EVT\n");

        // if(i2c_dma_rx_config(I2C_0,i2c_rx_buf,sizeof(i2c_rx_buf),DMA_CH_0,TRUE)==PPlus_SUCCESS)
        // {
        //     i2c_mode=I2C_MODE_RX;
        //     hal_i2c_addr_update(master_pi2c,slave_i2c_addr);    //����24c02�豸��ַ--д
        //     uint8_t reg_addr=REG_ADDR;
        //     hal_i2c_send(master_pi2c, (uint8*)&reg_addr,1);           //����24c02�豸��ַ--д
        //     hal_dma_stop_channel(DMA_CH_0);
        //     uint8_t ret=hal_dma_start_channel(DMA_CH_0);
        //     extern void hal_master_send_read_cmd(void* pi2c, uint8_t len);
        //     hal_master_send_read_cmd(master_pi2c,DATA_LEN);
        //     LOG("rx start ret is %d\n",ret);
        // }
        I2CRead(master_pi2c,i2c_rx_buf,i2c_tx_buf_len,slave_i2c_addr);
        osal_set_event(i2c_TaskID, KEY_I2C_DMA_RX_DATA_EVT);
        //osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_RX_DATA_EVT,20);

        return (events ^ KEY_I2C_DMA_READ_DATA_EVT);
    }

    if( events & KEY_I2C_DMA_RX_DATA_EVT)
    {
        LOG("I2C_RX_data=[");

        for(uint8 i=0; i<i2c_tx_buf_len; i++)
        {
            LOG("0x%x,",i2c_rx_buf[i]);
        }

        LOG("]\n");

        if (osal_memcmp(i2c_rx_buf, i2c_tx_buf, i2c_tx_buf_len) == true)
            LOG("\nIIC TEST PASS!\n");
        else
            LOG("\nIIC TEST FAIL!\n");

        osal_memset(i2c_rx_buf,0,sizeof(i2c_rx_buf));
        osal_start_timerEx(i2c_TaskID, KEY_I2C_DMA_WRITE_DATA_EVT, 1000);
        return (events ^ KEY_I2C_DMA_RX_DATA_EVT);
    }

    return 0;
}

/*********************************************************************
*********************************************************************/
