export PATH := $(PATH):$(HOME)/local/sdcc/bin

MCU  = stm8s103k3
ARCH = stm8

F_CPU   ?= 16000000
TARGET  ?= main.ihx

LIBDIR   = 

SRCS    := $(wildcard *.c $(LIBDIR)/*.c)
ASRCS   := $(wildcard *.s $(LIBDIR)/*.s)

#OBJS     = $(SRCS:.c=.rel)
#OBJS    += $(ASRCS:.s=.rel)

OBJ_DIR  = obj
OBJS 	:= $(patsubst %.c,$(OBJ_DIR)/%.rel,$(SRCS))
OBJS 	+= $(patsubst %.s,$(OBJ_DIR)/%.rel,$(ASRCS))

CC       = sdcc
LD       = sdld
AS       = sdasstm8
OBJCOPY  = sdobjcopy
ASFLAGS  = -plosgff
CFLAGS   = -m$(ARCH) -p$(MCU) --std-sdcc11
CFLAGS  += -DF_CPU=$(F_CPU)UL -I. -I$(LIBDIR)
CFLAGS  += --stack-auto --noinduction --use-non-free
## Disable lospre (workaround for bug 2673)
#CFLAGS  += --nolospre
## Extra optimization rules - use with care
#CFLAGS  += --peep-file $(LIBDIR)/util/extra.def
LDFLAGS  = -m$(ARCH) -l$(ARCH) --out-fmt-ihx
MKDIR_P  = mkdir -p

all: directories $(TARGET) size hex

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.rel: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(OBJ_DIR)/%.rel: %.s
	$(AS) $(ASFLAGS) $<

directories: ${OBJ_DIR}

hex: $(TARGET)
	packihx $(TARGET) > main.hex

${OBJ_DIR}:
	${MKDIR_P} ${OBJ_DIR}

size:
	@$(OBJCOPY) -I ihex --output-target=binary $(TARGET) $(TARGET).bin
	@echo "----------"
	@stat -L -f "Image size: %z bytes." $(TARGET).bin

flash: $(TARGET)
	stm8flash -c stlinkv2 -p $(MCU) -w $(TARGET)

serial: $(TARGET)
	stm8gal -p /dev/ttyUSB0 -w $(TARGET)

clean:
	rm -f *.map *.ihx *.lk *.cdb *.bin *.hex $(OBJ_DIR)/*.asm $(OBJ_DIR)/*.rel $(OBJ_DIR)/*.o $(OBJ_DIR)/*.sym $(OBJ_DIR)/*.lst $(OBJ_DIR)/*.rst

.PHONY: clean all flash directories
