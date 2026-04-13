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
    Filename:       i2c_slave_demo.c
    Revised:        $Date $
    Revision:       $Revision $


**************************************************************************************************/

/*********************************************************************
    INCLUDES
*/
#include "i2c_slave_demo.h"
#include "i2c_slave.h"
#include "i2c_s.h"
#include "OSAL.h"
#include "log.h"
#include "gpio.h"
#include "clock.h"
#include "pwrmgr.h"

#ifdef SLAVE_DMA_MODE
#include "dma.h"
#endif

extern void osalTimeUpdate( void );


static i2c_slaver_t m_i2c_slave = {0};
static uint8_t i2cslave_task_id = 0xFF;

#ifdef SLAVE_DMA_MODE
static uint8_t slave_tx_len = 128;

static uint8_t i2c_dma_tx_config(uint8_t i2cx,uint8_t* tx_buf,uint16_t tx_len,DMA_CH_t dma_ch,uint8_t dma_int)
{
    HAL_DMA_t ch_cfg;
    DMA_CH_CFG_t cfg;
    ch_cfg.dma_channel = dma_ch;
    ch_cfg.evt_handler = NULL;
    hal_dma_init_channel(ch_cfg);
    AP_I2C_TypeDef* I2cx = NULL;
    I2cx = (i2cx == IIC_Module0) ? AP_I2C0 : AP_I2C1;
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
    return retval;
}

int I2C_Slave_Write(void *pi2c, uint8 *data, uint8 len)
{
    uint8_t i2cx = (pi2c == AP_I2C0) ? IIC_Module0 : IIC_Module1;
    int ret = false;
    if (i2c_dma_tx_config(i2cx, data, len, DMA_CH_1, FALSE) == PPlus_SUCCESS)
    {
        hal_dma_stop_channel(DMA_CH_1);
        ret = hal_dma_start_channel(DMA_CH_1);
        ret = hal_dma_wait_channel_complete(DMA_CH_1);
        ret = (ret == PPlus_SUCCESS) ? true : false;
    }
    
    return ret;
}
#endif

static void I2C_Slave_Hdl(I2C_Evt_t* pev)
{
    AP_I2C_TypeDef* m_i2c = NULL;
    if(m_i2c_slave.slaver_pi2c == IIC_Module0)
        m_i2c = AP_I2C0;
    else if(m_i2c_slave.slaver_pi2c == IIC_Module1)
        m_i2c = AP_I2C1;
    
    if(pev->type & I2C_RD_REQ_Evt)
    {
        #ifdef SLAVE_DMA_MODE
        I2C_Slave_Write(m_i2c,m_i2c_slave.txbuf,slave_tx_len);
        Hal_INTR_SOURCE_Clear(m_i2c,I2C_RD_REQ_Evt);
        #else
        HAL_ENTER_CRITICAL_SECTION();
        m_i2c->IC_DATA_CMD = m_i2c_slave.txbuf[m_i2c_slave.tx_len];
        Hal_INTR_SOURCE_Clear(m_i2c,I2C_RD_REQ_Evt);
        HAL_EXIT_CRITICAL_SECTION();
        m_i2c_slave.tx_len++;
        #endif
        osal_start_timerEx(i2cslave_task_id, I2C_SLAVE_TIMEOUT, 3);
    }
    
    if(pev->type & I2C_RX_FULL_Evt)
    {
        HAL_ENTER_CRITICAL_SECTION();
        while(1)
        {
            if((m_i2c->IC_STATUS & BV(3)) == 0)
                break;
            m_i2c_slave.rxbuf[m_i2c_slave.rx_len] = m_i2c->IC_DATA_CMD&0xFF;
            m_i2c_slave.rx_len++;
        }
        Hal_INTR_SOURCE_Clear(m_i2c,I2C_RX_FULL_Evt);
        HAL_EXIT_CRITICAL_SECTION();
        osal_start_timerEx(i2cslave_task_id, I2C_SLAVE_TIMEOUT, 3);
    }
}


void I2c_slave_Demo_Init( uint8 task_id )
{
    i2cslave_task_id = task_id;
    m_i2c_slave.i2c_slave_param.AddressMode = I2C_ADDR_7bit;
    m_i2c_slave.i2c_slave_param.workmode = Slave;
    m_i2c_slave.i2c_slave_param.id = 0;
    m_i2c_slave.i2c_slave_param.Slave_Address = 0x50;
    m_i2c_slave.i2c_slave_param.SCL_PIN = P34;
    m_i2c_slave.i2c_slave_param.SDA_PIN = P33;
    m_i2c_slave.i2c_slave_param.Tx_FIFO_Len = 1;
    m_i2c_slave.i2c_slave_param.RX_FIFO_Len = 1;
    m_i2c_slave.i2c_slave_param.IRQ_Source = 0x864;
    m_i2c_slave.i2c_slave_param.evt_handler = I2C_Slave_Hdl;
    uint8_t ret =  Hal_I2C_Slave_Init(&m_i2c_slave.i2c_slave_param, &m_i2c_slave.slaver_pi2c);
    if(ret == PPlus_IIC_SUCCESS)
    {
        LOG("i2c slave init success\n");
    }
    else
    {
        LOG("i2c slave init fail:%02x\n", ret);
    }
    #ifdef SLAVE_DMA_MODE
    hal_dma_init();
    #endif
}


uint16 I2c_slave_ProcessEvent( uint8 task_id, uint16 events)
{
    if(events & I2C_SLAVE_TIMEOUT)
    {
        osal_set_event(i2cslave_task_id, I2C_SLAVE_READ_DATA_EVT);
        return (events ^ I2C_SLAVE_TIMEOUT);
    }
    
    if(events & I2C_SLAVE_WRITE_DATA_EVT)
    {
        
        return (events ^ I2C_SLAVE_WRITE_DATA_EVT);
    }
    
    if(events & I2C_SLAVE_READ_DATA_EVT)
    {
        
        osal_memcpy(m_i2c_slave.txbuf, m_i2c_slave.rxbuf, m_i2c_slave.rx_len);//reg address dont need send
        #ifdef SLAVE_DMA_MODE
        slave_tx_len = m_i2c_slave.rx_len;
        #endif
        m_i2c_slave.rx_len = 0;
        m_i2c_slave.tx_len = 0;   //clear next tx data len
        return (events ^ I2C_SLAVE_READ_DATA_EVT);
    }
    
    return 0;
}

