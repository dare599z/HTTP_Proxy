cc=g++
cflags=-std=c++11 -Iinclude
pkgc=`pkg-config --libs libevent`

all: HTTP_Proxy

tmp/Proxy.o: src/Proxy.cpp
	$(cc) $(cflags) -c -o $@ $<

tmp/HTTP_Proxy.o: src/HTTP_Proxy.cpp
	$(cc) $(cflags) -c -o $@ $<

HTTP_Proxy: tmp/HTTP_Proxy.o tmp/Proxy.o
	$(cc) -o bin/proxy $(cflags) $(pkgc) $^