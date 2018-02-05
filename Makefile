all: httpd

httpd: httpd.c
	gcc -O2 -o httpd httpd.c

clean:
	rm httpd
