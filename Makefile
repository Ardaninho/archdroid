CC = aarch64-linux-gnu-gcc
CFLAGS = -static
OUT = build/fb_refresh
 
.PHONY: colour nocolour clean
 
colour:
	$(CC) fb_refresh_poscolour.c -O2 $(CFLAGS) -o $(OUT)
	chmod +x $(OUT)
 
nocolour:
	$(CC) fb_refresh_precolour.c $(CFLAGS) -o $(OUT)
	chmod +x $(OUT)

clean:
	rm $(OUT)