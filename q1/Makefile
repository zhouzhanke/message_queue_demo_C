TARGET=function_integral_timing

SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

CFLAGS=-O2
LIBS=-lgsl -lblas -lm -lrt

all: $(TARGET)

clean:
	-rm $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	gcc -o $@  $^  $(LIBS)

$(OBJECTS): %.o: %.c $(HEADERS)
	gcc -c $(CFLAGS) $< -o $@  $(LIBS)
