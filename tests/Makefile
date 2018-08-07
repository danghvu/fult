CC = icc
CXX = icpc
LD = icc
CFLAGS += -Wall -Wextra -O3 -g3 -I../include -I$(HOME)/abt/include -I$(HOME)/qth/include
CXXFLAGS += -std=c++11 -Wall -Wextra -O3 -g3 -I../include 

CFLAGS += -DUSE_PAPI -I$(TACC_PAPI_INC)
LDFLAGS += ../libfult.a -pthread -L$(TACC_PAPI_LIB) -lpapi

TARGET = fib_flt fib_qth fib_clk

all: $(TARGET)

%.o: %.c ../libfult.a
	$(CC) $(CFLAGS) -c $^

%_x: %.cxx ../libfult.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%_qth: %_qth.o
	$(LD) -O3 $^ -o $@ $(LDFLAGS) -L$(HOME)/qth/lib -lqthread

%_clk: %_clk.o
	$(LD) -O3 $^ -o $@ $(LDFLAGS) -L$(HOME)/qth/lib -lqthread

%_abt: %_abt.o
	$(LD) -O3 $^ -o $@ $(LDFLAGS) -L$(HOME)/abt/lib -labt

%_flt: %_flt.o
	$(LD) -O3 $^ -o $@ $(LDFLAGS)

spawn: spawn.o
	$(LD) -O3 $^ -o $@ $(LDFLAGS)

clean:
	rm -rf *.o $(TARGET)
