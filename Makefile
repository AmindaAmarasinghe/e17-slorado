CC       = cc
CXX		 = c++

LIBTORCH_DIR ?= thirdparty/torch/libtorch
CPPFLAGS += -I slow5lib/include/ \
			-I src/ \
			-I $(LIBTORCH_DIR)/include/torch/csrc/api/include \
			-I $(LIBTORCH_DIR)/include -I thirdparty/ \
			-I thirdparty/tomlc99/
CFLAGS	+= 	-g -Wall -O2
CXXFLAGS   += -g -Wall -O2  -std=c++14
LIBS    +=  -Wl,-rpath,$(LIBTORCH_DIR)/lib \
			-Wl,--no-as-needed,"$(LIBTORCH_DIR)/lib/libtorch_cpu.so"  \
			-Wl,--no-as-needed,"$(LIBTORCH_DIR)/lib/libtorch.so"  \
			-Wl,--as-needed $(LIBTORCH_DIR)/lib/libc10.so
LDFLAGS  += $(LIBS) -lz -lm -lpthread -lstdc++fs
BUILD_DIR = build

ifeq ($(zstd),1)
LDFLAGS		+= -lzstd
endif

# https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
ifeq ($(cxx11_abi),) #  cxx11_abi not defined
CXXFLAGS		+= -D_GLIBCXX_USE_CXX11_ABI=0
endif

# change the tool name to what you want
BINARY = slorado_cpu

OBJ = $(BUILD_DIR)/main.o \
      $(BUILD_DIR)/slorado.o \
      $(BUILD_DIR)/basecaller_main.o \
	  $(BUILD_DIR)/basecall.o \
      $(BUILD_DIR)/thread.o \
	  $(BUILD_DIR)/misc.o \
	  $(BUILD_DIR)/error.o \
	  $(BUILD_DIR)/signal_prep.o \
	  $(BUILD_DIR)/writer.o \
	  $(BUILD_DIR)/beam_search.o \
	  $(BUILD_DIR)/CPUDecoder.o \
	  $(BUILD_DIR)/fast_hash.o \
	  $(BUILD_DIR)/CRFModel.o \
	  $(BUILD_DIR)/stitch.o \
	  $(BUILD_DIR)/tensor_utils.o \
	  $(BUILD_DIR)/toml.o \


# add more objects here if needed

VERSION = `git describe --tags`

# make asan=1 enables address sanitiser
ifdef asan
	CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
	CFLAGS += -fsanitize=address -fno-omit-frame-pointer
	LDFLAGS += -fsanitize=address -fno-omit-frame-pointer
endif

# make accel=1 enables the acceelerator (CUDA,OpenCL,FPGA etc if implemented)
ifdef cuda
	CUDA_ROOT = /usr/local/cuda
	CUDA_LIB ?= $(CUDA_ROOT)/lib64
	CUDA_INC ?= $(CUDA_ROOT)/include
    CPPFLAGS += -DUSE_GPU=1 -I $(CUDA_INC)
	OBJ += $(BUILD_DIR)/GPUDecoder.o
	LIBS += -Wl,--as-needed -lpthread -Wl,--no-as-needed,"$(LIBTORCH_DIR)/lib/libtorch_cuda.so" -Wl,--as-needed,"$(LIBTORCH_DIR)/lib/libc10_cuda.so"
ifdef koi
	CPPFLAGS += -DUSE_KOI=1 -I thirdparty/koi_lib/include
	LDFLAGS += thirdparty/koi_lib/lib/libkoi.a -L $(CUDA_LIB)/ -lcudart_static -lrt -ldl
endif
	LDFLAGS +=  -L $(CUDA_LIB)/ -lcudart_static -lrt -ldl
# required for CPUDecoder
endif

CPPFLAGS += -DREMOVE_FIXED_BEAM_STAYS=1

.PHONY: clean distclean test

$(BINARY): $(OBJ) slow5lib/lib/libslow5.a
	$(CXX) $(CXXFLAGS) $(OBJ) slow5lib/lib/libslow5.a $(LDFLAGS) -o $@

$(BUILD_DIR)/main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/slorado.o: src/slorado.cpp src/misc.h src/error.h src/slorado.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/basecall.o: src/basecall.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/basecaller_main.o: src/basecaller_main.cpp src/error.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/thread.o: src/thread.cpp src/slorado.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/misc.o: src/misc.cpp src/misc.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/error.o: src/error.cpp src/error.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/signal_prep.o: src/signal_prep.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/writer.o: src/writer.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/beam_search.o: src/decode/beam_search.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/CPUDecoder.o: src/decode/CPUDecoder.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/GPUDecoder.o: src/decode/GPUDecoder.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/fast_hash.o: src/decode/fast_hash.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/CRFModel.o: src/nn/CRFModel.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/stitch.o: src/utils/stitch.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/tensor_utils.o: src/utils/tensor_utils.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/toml.o: thirdparty/tomlc99/toml.c
	$(CC) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

# follow the main.o above and add more objects here if needed

slow5lib/lib/libslow5.a:
	$(MAKE) -C slow5lib zstd=$(zstd) no_simd=$(no_simd) zstd_local=$(zstd_local) lib/libslow5.a

clean:
	rm -rf $(BINARY) $(BUILD_DIR)/*.o
	make -C slow5lib clean

# Delete all gitignored files (but not directories)
distclean: clean
	git clean -f -X
	rm -rf $(BUILD_DIR)/* autom4te.cache

# make test with run a simple test
test: $(BINARY)
	./test/test.sh

# make mem with run a simple memory test using valgrind
mem: $(BINARY)
	./test/mem.sh mem