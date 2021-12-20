CXX ?= g++
CXXFLAGS += -std=c++11 -MP -MMD -Wall -g -O3 -fPIC
#LDFLAGS +=
CXXBUILD = $(CXX) $(CXXFLAGS) -MF $(patsubst %.cpp,dep/%.d,$<) -c -o $@ $<

OBJ := arch_decode.o sparse_mem.o simple_arch_state.o host_system.o

DEP  := $(addprefix dep/,$(OBJ:.o=.d))
OBJS := $(addprefix obj/,$(OBJ))

LIB := rvfun.a

### targets
all: dep obj $(LIB)

obj:
	@mkdir $@

dep:
	@mkdir $@

-include $(DEP) main.d

.PHONY: clean
clean::
	@rm -f $(OBJS) $(OLIB) driver.exe main.o

$(OBJS): obj/%.o: %.cpp
	@$(CXXBUILD)

$(LIB): $(OBJS)
	@$(CXX) -shared -g -o $@ $^

driver.exe: main.o $(LIB)
	@$(CXX) -g -o $@ $^ -Wl,-rpath='$${ORIGIN}'

dfg.exe: dfg.o $(LIB)
	@$(CXX) -g -o $@ $^ -Wl,-rpath='$${ORIGIN}'

run_dfg: dfg.exe
	./dfg.exe -f test.code.txt -p
	dot -Tsvg dfg.dot > dfg.svg

