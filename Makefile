.PHONY: all debug release cleandeps clean run
debug: mapfab
release: mapfab
static: mapfab
all: mapfab
run: mapfab
	./mapfab

define compile
@echo -e '\033[32mCXX $@\033[0m'
$(CXX) $(CXXFLAGS) -c -o $@ $<
endef

define deps
@echo -e '\033[32mDEPS $@\033[0m'
$(CXX) $(CXXFLAGS) -MM -MP -MT '\
$(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(<:.cpp=.o)) \
$(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(<:.cpp=.d))\
' -o $@ $<
endef

##########################################################################

SRCDIR:=src
OBJDIR:=obj
IMGDIR:=img
INCS:=-I$(SRCDIR)

VERSION := "0.1"
GIT_COMMIT := "$(shell git describe --abbrev=8 --dirty --always)"

ifeq ($(ARCH),MINGW_CROSS)
override CXX:=x86_64-w64-mingw32-g++
endif

override CXXFLAGS+= \
  `wx-config --cxxflags` \
  -std=gnu++20 \
  -Wall \
  -Wextra \
  -Wno-unused-parameter \
  -Wno-narrowing \
  -Wno-missing-field-initializers \
  -fmax-errors=3 \
  -ftemplate-depth=100 \
  -pipe \
  -g \
  $(INCS) \
  -DVERSION=\"$(VERSION)\" \
  -DGIT_COMMIT=\"$(GIT_COMMIT)\"

debug: CXXFLAGS += -O0 -g
release: CXXFLAGS += -O3 -DNDEBUG -Wno-unused-variable
static: CXXFLAGS += -static -O3 -DNDEBUG

VPATH=$(SRCDIR)

LDLIBS:= `wx-config --libs`

SRCS:= \
main.cpp \
model.cpp \
grid_box.cpp \
palette.cpp \
metatiles.cpp \
level.cpp \
class.cpp \
chr.cpp \
convert.cpp \
lodepng/lodepng.cpp

IMGS:= \
dropper.png \
stamp.png \
select.png

OBJS := $(foreach o,$(SRCS),$(OBJDIR)/$(o:.cpp=.o))
DEPS := $(foreach o,$(SRCS),$(OBJDIR)/$(o:.cpp=.d))
DATA := $(foreach o,$(IMGS),$(SRCDIR)/$(o:.png=.png.inc))

mapfab: $(OBJS)
	echo 'LINK'
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS) 
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(DATA)
	$(compile)
$(OBJDIR)/%.d: $(SRCDIR)/%.cpp $(DATA)
	$(deps)
$(SRCDIR)/%.png.inc: $(IMGDIR)/%.png
	bin2c -o $@ $<

##########################################################################	

deps: $(DEPS)
	@echo 'dependencies created'

cleandeps:
	rm -f $(wildcard $(OBJDIR)/*.d)

clean: cleandeps
	rm -f $(wildcard $(OBJDIR)/*.o)
	rm -f mapfab

# Create directories:

$(info $(shell mkdir -p $(OBJDIR)/))
$(info $(shell mkdir -p $(OBJDIR)/catch))
$(info $(shell mkdir -p $(OBJDIR)/lodepng))

##########################################################################	

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif
