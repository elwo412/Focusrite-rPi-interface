CC=gcc
CFLAGS=`pkg-config --cflags glib-2.0 gio-2.0 libpulse libpulse-mainloop-glib`
LIBS=`pkg-config --libs glib-2.0 gio-2.0 libpulse libpulse-mainloop-glib`


make:
	$(CC) $(CFLAGS) -g -Wall main.c -o blt-daemon $(LIBS)

clean:
	rm blt-daemon
