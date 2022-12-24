target=	simavr_server
firm_src = ${wildcard atmega*.c}
firmware = ${firm_src:.c=.hex}
simavr = ../../

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

LDFLAGS += -lpthread -lutil

include ../Makefile.opengl

all: obj ${firmware} ${target}

include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/uart_pty.o
${board} : ${OBJ}/cpu.o
${board} : ${OBJ}/${target}.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.a *.axf ${target} *.vcd *.hex
