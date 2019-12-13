CXX ?= g++
CXXFLAGS += -std=c++11 -MP -MMD -Wall -g -O3
#LDFLAGS +=
CXXBUILD = $(CXX) $(CXXFLAGS) -MF $(patsubst %.cpp,dep/%.d,$<) -c -o $@ $<

OBJ := arch_decode.o sparse_mem.o

DEP  := $(addprefix dep/,$(OBJ:.o=.d))
OBJS := $(addprefix obj/,$(OBJ))

### targets
all: dep obj

obj:
	@mkdir $@

dep:
	@mkdir $@

-include $(DEP)

.PHONY: clean
clean::
	@rm -f $(OBJS) $(OLIB) driver.exe main.o

$(OBJS): obj/%.o: %.cpp
	@$(CXXBUILD)

driver.exe: main.o $(OBJS)
	@$(CXX) -o $@ $^

