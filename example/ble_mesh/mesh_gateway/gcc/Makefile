BIN = ./output/meshgateway.bin
IHEX = ./output/meshgateway.ihex
ELF = ./output/meshgateway.elf
ASM = ./output/meshgateway.asm

CONFIG_PHY6222_PHY_MESH = 1

ROOT := ../../../..
PROJ_ROOT := ..

VPATH = $(PROJ_ROOT)
VPATH += $(PROJ_ROOT)/source
VPATH += $(PROJ_ROOT)/source/bleMesh

VPATH += $(ROOT)/components/inc
VPATH += $(ROOT)/components/ble/controller/include
VPATH += $(ROOT)/osal/include
VPATH += $(ROOT)/components/ble/include
VPATH += $(ROOT)/components/ble/hci
VPATH += $(ROOT)/components/ble/host
VPATH += $(ROOT)/components/profiles/SimpleProfile
VPATH += $(ROOT)/components/ethermind/mesh/export/cbtimer
VPATH += $(ROOT)/components/driver/common
VPATH += $(ROOT)/components/ethermind/mesh/export/include
VPATH += $(ROOT)/components/ethermind/utils/include
VPATH += $(ROOT)/components/ethermind/mesh/export/vendormodel
VPATH += $(ROOT)/components/ethermind/mesh/export/vendormodel/client
VPATH += $(ROOT)/components/common
VPATH += $(ROOT)/components/osal/snv
VPATH += $(ROOT)/components/ethermind/external/crypto/asm_ecdh_p256
VPATH += $(ROOT)/components/libraries/crc16
VPATH += $(ROOT)/components/driver/timer
VPATH += $(ROOT)/components/mesh/export/climodel
VPATH += $(ROOT)/components/ethermind/interfaces/crypto
VPATH += $(ROOT)/components/libraries/crypto/interfaces/crypto
VPATH += $(ROOT)/components/libraries/crypto/sha256
VPATH += $(ROOT)/components/libraries/crypto/aes
VPATH += $(ROOT)/components/libraries/crypto/aes/aes_cmac


include $(ROOT)/components/gcc/components.cflags
include $(ROOT)/components/gcc/components.mk


DEF += -DMAX_NUM_LL_CONN=1
DEF += -DGATT_MAX_NUM_CONN=MAX_NUM_LL_CONN+1
#DEF += -DCFG_MESH_FAST_PRO=1
DEF += -DHOST_CONFIG=0x0c
DEF += -DGATT_PVNR=0
DEF += -DMESH_HEAP=0

FLASH_LD = $(PROJ_ROOT)/gcc/meshgateway.ld

#OTA src
#DEF += -DPHY_SLB_OTA_ENABLE=1
#INC   += -I$(ROOT)/components/profiles/ppsp
VPATH += $(ROOT)/components/libraries/crc16
CSRCS += crc16.c
# CSRCS += sha256.c
CSRCS += aes_cmac.c
CSRCS += aes_cmac_pl.c
CSRCS += observer.c
# CSRCS += EXT_cbtimer.c
#CSRCS += ppsp_serv.c
#CSRCS += list_slst.c
#CSRCS += core_queu.c

#project src
SRC_RAW = main.c
SRC_RAW += appl_sample_mesh_gateway.c
SRC_RAW += OSAL_bleMesh.c
SRC_RAW += bleMesh.c
SRC_RAW += bleMesh_Main.c
SRC_RAW += dongleKey.c

CSRCS += EXT_cbtimer.c
CSRCS += appl_proxy.c
CSRCS += vendormodel_client.c
CSRCS += mesh_clients.c

STARTUP_RAW = phy6222_start.s

SRC_RAW += $(CSRCS)

INC += -I$(PROJ_ROOT)/source
INC += -I$(PROJ_ROOT)/source/bleMesh

INC += -I$(ROOT)/components/gcc/CMSIS/device/phyplus
INC += -I$(ROOT)/components/ethermind/mesh/export/vendormodel/client
INC += -I$(ROOT)/components/ble/controller/include
INC += -I$(ROOT)/components/common
INC += -I$(ROOT)/components/driver/common
INC += -I$(ROOT)/components/osal/snv
INC += -I$(ROOT)/components/libraries/crc16
INC += -I$(ROOT)/components/mesh/export/climodel
INC += -I$(ROOT)/components/ethermind/interfaces/crypto
INC += -I$(ROOT)/components/driver/led_light
INC += -I$(ROOT)/components/libraries/crypto/interfaces/crypto
INC += -I$(ROOT)/components/ethermind/external/crypto/asm_ecdh_p256
INC += -I$(ROOT)/components/libraries/crypto/include
INC += -I$(ROOT)/components/ethermind/mesh/export/include
INC += -I$(ROOT)/components/libraries/crypto/aes
INC += -I$(ROOT)/components/libraries/crypto/asm_ecdh_p256
INC += -I$(ROOT)/components/libraries/crypto/aes/aes_cmac
INC += -I$(ROOT)/components/libraries/crypto/sha256
INC += -I$(ROOT)/components/libraries/crypto/interfaces/crypto
INC += -I$(ROOT)/components/ethermind/external/crypto/asm_ecdh_p256
INC += -I$(ROOT)/components/libraries/crypto/include
INC += -I$(ROOT)/components/ethermind/mesh/export/include
INC += -I$(ROOT)/components/libraries/crypto/aes
INC += -I$(ROOT)/components/libraries/crypto/asm_ecdh_p256
INC += -I$(ROOT)/components/libraries/crypto/aes/aes_cmac
INC += -I$(ROOT)/components/libraries/crypto/sha256
# INC += -I$(ROOT)/components/ethermind/mesh/export/cbtimer


CFLAGS += -Os
CFLAGS += -g3
CFLAGS += --specs=nosys.specs
CFLAGS += -mcpu=cortex-m0
#CFLAGS += -mfloat-abi=soft
CFLAGS += -mthumb
CFLAGS += -ffunction-sections
#CFLAGS += -mfpu=fpv4-sp-d16
CFLAGS += -Wall
#CFLAGS += --specs=rdimon.specs

LIBS += -lethermind_mesh_core
LIBS += -lethermind_mesh_models
LIBS += -lethermind_utils

#LFLAGS += -mcpu=cortex-m0
#LFLAGS += -mfloat-abi=soft
#LFLAGS += -mthumb
#LFLAGS += -mfpu=fpv4-sp-d16
LFLAGS += -Wl,--gc-sections
LFLAGS += -Wl,-Map=build.map
LFLAGS += $(ROOT)/misc/bb_rom_sym_m0.gdbsym
LFLAGS += $(LIBS_PATH)
LFLAGS += $(LIBS)

OBJ = $(SRC_RAW:%.c=%.o)
OBJ :=  $(notdir $(OBJ))

STARTUP_OBJ = $(STARTUP_RAW:%.s=%.o)
STARTUP_OBJ :=  $(notdir $(STARTUP_OBJ))



all:$(OBJ) $(STARTUP_OBJ)
	@echo $(STARTUP_OBJ)
	@echo $(OBJ)
	@echo $(BIN)
	
	mkdir -p output
	
	arm-none-eabi-gcc -o $(ELF) $(CFLAGS) $(OBJ) $(STARTUP_OBJ) $(LFLAGS) -T$(FLASH_LD)
	#arm-none-eabi-ld -o $(ELF) $(LFLAGS) $(OBJ) $(STARTUP_OBJ) -T$(FLASH_LD)  
	arm-none-eabi-objcopy -O binary -S $(ELF) $(BIN)
	arm-none-eabi-objcopy -O ihex $(ELF) $(IHEX)
	arm-none-eabi-objdump -s -S $(ELF) > $(ASM)
	
%.o:%.s
	arm-none-eabi-gcc -c $(CFLAGS) $< -o $@
	@echo

%.o:%.c
	arm-none-eabi-gcc -c $(CFLAGS) $(DEF) $(INC) $< -o $@
	@echo

.PHONY: clean
clean :
	@-rm  *.elf *.hex *.bin *.map
	-rm $(OBJ)
	-rm $(STARTUP_OBJ)
