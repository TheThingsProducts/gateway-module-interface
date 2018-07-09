APP=gateway-module-interface-test
CC=gcc
CPP=g++
CFLAGS=-Ilib/
CPPFLAGS=-Ilib/ -std=c++11
LIBS=-lpthread
DEPS = lib/gateway-module-interface.h
OBJ = linux/main.o lib/gateway-module-interface.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CPPFLAGS)

$(APP): $(OBJ)
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(APP) lib/*.o linux/*.o 
