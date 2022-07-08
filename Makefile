# AxProxy Makefile
INCLUDES=-I include -I lib
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss

OBJS = \
	bin/startup.o \
	bin/util.o \
	bin/proxy.o \
	bin/nscache.o \
	bin/dns.o

all: host

up:
	@cp -pv ../proxy-util/util.h include/util.h
	@cp -pv ../proxy-util/util.c src/util.c

internal: prepare
	@echo "  CC    src/startup.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/startup.c -o bin/startup.o
	@echo "  CC    src/util.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/util.c -o bin/util.o
	@echo "  CC    src/proxy.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/proxy.c -o bin/proxy.o
	@echo "  CC    src/nscache.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/nscache.c -o bin/nscache.o
	@echo "  CC    lib/dns.c"
	@$(CC) $(CFLAGS) $(INCLUDES) lib/dns.c -o bin/dns.o
	@echo "  LD    bin/axproxy"
	@$(LD) -o bin/axproxy $(OBJS) $(LDFLAGS)

prepare:
	@mkdir -p bin

host:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O2 -ffunction-sections -fdata-sections -Wstrict-prototypes' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax'

mipsel:
	@make internal \
		CC=mips-unknown-linux-gnu-gcc \
		LD=mips-unknown-linux-gnu-gcc \
		CFLAGS='-c $(MIPSEL_CFLAGS) -I $(ESLIB_INC) -O2 -EL' \
		LDFLAGS='$(MIPSEL_LDFLAGS) -L $(ESLIB_DIR) -les-mipsel -EL'

mipseb:
	@make internal \
		CC=mips-unknown-linux-gnu-gcc \
		LD=mips-unknown-linux-gnu-gcc \
		CFLAGS='-c $(MIPSEB_CFLAGS) -I $(ESLIB_INC) -O2 -EB' \
		LDFLAGS='$(MIPSEB_LDFLAGS) -L $(ESLIB_DIR) -les-mipseb -EB'

arm:
	@make internal \
		CC=arm-linux-gnueabi-gcc \
		LD=arm-linux-gnueabi-gcc \
		CFLAGS='-c $(ARM_CFLAGS) -I $(ESLIB_INC) -O2' \
		LDFLAGS='$(ARM_LDFLAGS) -L $(ESLIB_DIR) -les-arm'

install:
	@cp -v bin/axproxy /usr/bin/axproxy

uninstall:
	@rm -fv /usr/bin/axproxy

post:
	@echo "  STRIP axproxy"
	@sstrip bin/axproxy
	@echo "  UPX   axproxy"
	@upx bin/axproxy
	@echo "  LCK   axproxy"
	@perl -pi -e 's/UPX!/EsNf/g' bin/axproxy
	@echo "  AEM   axproxy"
	@nogdb bin/axproxy

post2:
	@echo "  STRIP axproxy"
	@sstrip bin/axproxy
	@echo "  AEM   axproxy"
	@nogdb bin/axproxy

indent:
	@indent $(INDENT_FLAGS) ./*/*.h
	@indent $(INDENT_FLAGS) ./*/*.c
	@rm -rf ./*/*~

clean:
	@echo "  CLEAN ."
	@rm -rf bin

analysis:
	@scan-build make
	@cppcheck --force */*.h
	@cppcheck --force */*.c

gendoc:
	@doxygen aux/doxygen.conf
