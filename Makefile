# Ax-Proxy Makefile
INCLUDES=-I include
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss

OBJS = \
	release/main.o \
	release/proxy.o \
	release/dns.o

all: host

internal: prepare
	@echo "  CC    src/main.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/main.c -o release/main.o
	@echo "  CC    src/proxy.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/proxy.c -o release/proxy.o
	@echo "  CC    src/dns.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/dns.c -o release/dns.o
	@echo "  LD    release/axproxy"
	@$(LD) -o release/axproxy $(OBJS) $(LDFLAGS)

prepare:
	@mkdir -p release

hostHTTPS:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes -DENABLE_HTTPS' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax'

hostS5:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes -DENABLE_SOCKS5' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax'

host:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes -DENABLE_HTTPS -DENABLE_SOCKS5' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax'

install:
	@cp -v release/axproxy /usr/bin/axproxy

uninstall:
	@rm -fv /usr/bin/axproxy

indent:
	@indent $(INDENT_FLAGS) ./*/*.h
	@indent $(INDENT_FLAGS) ./*/*.c
	@rm -rf ./*/*~

clean:
	@echo "  CLEAN ."
	@rm -rf release

analysis:
	@scan-build make
	@cppcheck --force */*.h
	@cppcheck --force */*.c

gendoc:
	@doxygen aux/doxygen.conf
