CC=gcc
FFMPEG_DIR=/usr/share/ffmpeg
CFLAGS+=-I$(FFMPEG_DIR)/include
CFLAGS+=-I/home/test/ffmpeg/ffmpeg-2.1.3
CFLAGS+=-I/usr/include
CFLAGS+=-g
CFLAGS+=-lz\
		-lm\
		-lbz2
LDFLAGS+=-L$(FFMPEG_DIR)/lib
LDFLAGS+=-lpthread\
	-lSDL\
	-lavformat\
	-lavcodec\
	-lavutil\
	-lavdevice\
	-lavfilter\
	-lswresample\
	-lswscale

	
ffmpeg_object=tutorial05_ok.c


myffmpeg:$(ffmpeg_object)
	$(CC) -o myffmpeg $(ffmpeg_object) $(LDFLAGS) $(CFLAGS)
#	gcc -o $@ $< -g -L/usr/share/ffmpeg/lib -lpthread -lSDL -lavformat -lavcodec -lavutil -lavdevice -lavfilter -lswresample -lswscale -lm -lz -I/usr/share/ffmpeg/include `sdl-config --cflags --libs`

clean:
	-rm myffmpeg
	-rm *.o
