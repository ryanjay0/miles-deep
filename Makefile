
#Created by Ryan Jay 30.10.16
#Covered by the GPL. v3 (see included LICENSE)


CUDA := -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcublas -lcudart -lcurand
#CUDNN := -lcudnn -DUSE_CUDNN
#CPU_ONLY := -DCPU_ONLY

appname := miles-deep
libcaffe := caffe/distribute/lib/libcaffe.a

CXX := g++
CXXFLAGS := -std=c++11 -Wno-sign-compare -Wall -pthread -fPIC -DNDEBUG -O2 -DUSE_OPENCV
INCLUDES := -I/usr/local/include -I. -I./caffe/distribute/include
LDLIBS := -L/usr/lib -L/usr/local/lib -L/usr/lib/x86_64-linux-gnu
LDFLAGS := -lm -lglog -lopencv_core -lopencv_highgui -lopencv_imgproc \
    -lstdc++ -lhdf5 -lhdf5_hl -lopenblas
CAFFE := -Wl,--whole-archive $(libcaffe) -Wl,--no-whole-archive 

S := /usr/lib/x86_64-linux-gnu
STATIC_LIBS := $S/libgflags.a $S/libboost_thread.a $S/libboost_system.a $S/libprotobuf.a

srcfiles := $(shell find . -name "*.cpp")
cores := $(shell grep -c ^processor /proc/cpuinfo)

all: $(appname)

$(appname): $(libcaffe) $(srcfiles)
	$(CXX) $(CXXFLAGS) -o $(appname) $(srcfiles) $(CAFFE) $(LDFLAGS) $(INCLUDES) \
	    $(CUDA) $(CUDNN) $(LDLIBS) $(STATIC_LIBS) 

clean: 
	rm -rf $(appname)

superclean: 
	rm -rf $(appname)
	make -C caffe clean

$(libcaffe):
	cp Makefile.caffe caffe/Makefile.config
	make -C caffe clean
	make -C caffe all -j $(cores)
	make -C caffe distribute
