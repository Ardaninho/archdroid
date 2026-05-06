CC = aarch64-linux-gnu-gcc
CFLAGS = -static
OUT = build/fb_refresh
 
.PHONY: fb_refresh clean
all: fb_refresh
 
fb_refresh:
	@echo "  CC     fb_refresh.c"
	@$(CC) fb_refresh.c $(CFLAGS) -o $(OUT) > /dev/null 2>&1
	@echo "  CHMOD  $(OUT)"
	@chmod +x $(OUT)
	@echo "Your aarch64 binary is ready in $(OUT)"

clean:
	@echo "  CLEAN  $(OUT)"
	@rm $(OUT)