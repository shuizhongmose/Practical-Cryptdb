MYSRC := $(shell pwd)/mysql-src
MYBUILD := $(MYSRC)/build
RPATH := 1

CXX := g++-4.8

# install mysql 5.5.60 on ubuntu 16.04 mannully in /usr/local/mysql
UBUNTU_VERSION := $(shell lsb_release -rs)

ifeq ($(UBUNTU_VERSION), 12.04)
    MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin
else ifeq ($(UBUNTU_VERSION), 16.04)
    MYSQL_PLUGIN_DIR := /usr/local/mysql/lib/plugin
else ifeq ($(UBUNTU_VERSION), 20.04)
    MYSQL_PLUGIN_DIR := /usr/local/mysql/lib/plugin
else
    $(warning Unsupported Ubuntu version, defaulting MYSQL_PLUGIN_DIR to /usr/lib/mysql/plugin)
    MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin
endif


OBJDIR	 := obj
TOP	 := $(shell echo $${PWD-`pwd`})
#CXX	 := g++
AR	 := ar
## -g -O0 -> -O2
CXXFLAGS := -g -O0 -fno-strict-aliasing -fno-rtti -fwrapv -fPIC \
	    -Wall -Wpointer-arith -Wendif-labels -Wformat=2  \
	    -Wextra -Wmissing-noreturn -Wwrite-strings -Wno-unused-parameter \
	    -Wno-deprecated \
	    -Wmissing-declarations -Woverloaded-virtual  \
	    -Wunreachable-code -D_GNU_SOURCE -std=c++0x -I$(TOP)
LDFLAGS  := -L$(TOP)/$(OBJDIR) -L/usr/local/lib -Wl,--no-undefined


## Use RPATH only for debug builds; set RPATH=1 in config.mk.
ifeq ($(RPATH),1)
LDRPATH	 := -Wl,-rpath=$(TOP)/$(OBJDIR) -Wl,-rpath=$(TOP)
endif

CXXFLAGS += -I$(MYBUILD)/include \
	    -I$(MYSRC)/include \
	    -I$(MYSRC)/sql \
	    -I$(MYSRC)/regex \
	    -I$(MYBUILD)/sql \
		-I/usr/local/include \
	    -DHAVE_CONFIG_H -DMYSQL_SERVER -DEMBEDDED_LIBRARY -DDBUG_OFF \
	    -DMYSQL_BUILD_DIR=\"$(MYBUILD)\"
# LDFLAGS	 += -lpthread -lrt -ldl -lcrypt -lreadline
# glibc > 2.17 版本中不需要显示链接librt
LDFLAGS	 += -lpthread -ldl -lcrypt -lreadline

## To be populated by Makefrag files

OBJDIRS	:=

.PHONY: all
all:

.PHONY: install
install:

.PHONY: clean
clean:
	rm -rf $(OBJDIR) mtl

.PHONY: doc
doc:
	doxygen CryptDBdoxgen

.PHONY: whitespace
whitespace:
	find . -name '*.cc' -o -name '*.hh' -type f -exec sed -i 's/ *$//' '{}' ';'

.PHONY: always
always:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o

$(OBJDIR)/%.o: %.cc
	@mkdir -p $(@D)
	$(CXX) -MD $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(OBJDIR)/%.cc
	@mkdir -p $(@D)
	$(CXX) -MD $(CXXFLAGS) -c $< -o $@

mtl/%:$(OBJDIR)/debug/%.o
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)  -L/$(MYBUILD)/libmysqld -lmysqld -laio -lz -ldl -lm -lcrypt -lpthread  -lcryptdb -ledbcrypto -ledbutil -ledbparser -lntl -lcrypto




include crypto/Makefrag
include parser/Makefrag
include main/Makefrag
include util/Makefrag
include udf/Makefrag
include mysqlproxy/Makefrag
include debug/Makefrag


$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	perl mergedep.pl $@ $^
	echo "after merge"
-include $(OBJDIR)/.deps

