# Makefile for TipsyModule

BOOST_INCLUDE = /usr/include
BOOST_LIB = /usr/lib
PYTHON_INCLUDE = /usr/include/python2.3

INCLUDES = -I../structures -I$(BOOST_INCLUDE) -I$(PYTHON_INCLUDE)

CXXFLAGS= -fPIC $(INCLUDES)

OBJS = TipsyModule.o \
	export_CodeUnits.o \
	export_OrientedBox.o \
	export_Sphere.o \
	export_TipsyParticles.o \
	export_TipsyReader.o \
	export_TipsyFile.o \
	export_SS.o

TipsyFile.so: $(OBJS)
	$(CXX) -shared -o TipsyFile.so $(OBJS) -L$(BOOST_LIB) -lboost_python

all: TipsyFile.so
