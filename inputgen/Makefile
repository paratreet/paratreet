STRUCTURES_PATH = ../utility/structures
TARGET = plummer tipsyPlummer

all: $(TARGET) $(STRUCTURES_PATH)/libTipsy.a

OBJECTS = plummer.o tipsyPlummer.o gen_util.o

CPPFLAGS += -I$(STRUCTURES_PATH)

$(STRUCTURES_PATH)/libTipsy.a: $(STRUCTURES_PATH)/Makefile
	cd $(STRUCTURES_PATH); $(MAKE) libTipsy.a

$(STRUCTURES_PATH)/Makefile:
	cd $(STRUCTURES_PATH); ./configure

plummer.o: plummer.cpp
	g++ $(CPPFLAGS) -c plummer.cpp

tipsyPlummer.o: tipsyPlummer.cpp
	g++ $(CPPFLAGS) -g -O0 -c tipsyPlummer.cpp

gen_util.o: gen_util.cpp
	g++ $(CPPFLAGS) -c gen_util.cpp

plummer: plummer.o gen_util.o
	g++ $(CPPFLAGS) -o plummer plummer.o gen_util.o

tipsyPlummer: tipsyPlummer.o gen_util.o
	g++ $(CPPFLAGS) -g -O0 -o tipsyPlummer tipsyPlummer.o gen_util.o -lTipsy -L$(STRUCTURES_PATH)

clean:
	rm -rf $(OBJECTS) $(TARGET)

.PHONY: all clean
