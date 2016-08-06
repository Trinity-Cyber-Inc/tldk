# Copyright (c) 2016 Intel Corporation.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

ifeq ($(GTEST_DIR),)
all:
	@echo "GTEST_DIR is not setup properly. Skipping gtest"

%::
	@echo "GTEST_DIR is not setup properly. Skipping gtest"

else

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

CUR_SUBDIR ?= .

# we must create the output dir first and recall the same Makefile
# from this directory
ifeq ($(NOT_FIRST_CALL),)

NOT_FIRST_CALL = 1
export NOT_FIRST_CALL


BDIR := $(RTE_OUTPUT)/build/$(CUR_SUBDIR)

all:
	$(Q)mkdir -p $(BDIR)
	$(Q)$(MAKE) -C $(BDIR) -f $(RTE_EXTMK) \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR) X=obj
	$(Q)$(MAKE) -C $(BDIR) -f $(RTE_EXTMK) \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR) X=app

%::
	$(Q)mkdir -p $(BDIR)
	$(Q)$(MAKE) -C $(BDIR) -f $(RTE_EXTMK) $@ \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR) X=obj
	$(Q)$(MAKE) -C $(BDIR) -f $(RTE_EXTMK) $@ \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR) X=app
else

#
# building object
#

ifeq ($(X),obj)
include $(TLDK_ROOT)/mk/tle.cpp-vars.mk

# binary name
OBJ = gtest-rfc.o

# all source are stored in SRCS-y
SRCS-y += main.cpp
SRCS-y += test_tle_dring.cpp
SRCS-y += test_tle_udp_ctx.cpp
SRCS-y += test_tle_udp_dev.cpp
SRCS-y += test_tle_udp_destroy.cpp
SRCS-y += test_tle_udp_event.cpp
SRCS-y += test_tle_udp_stream.cpp

CXXFLAGS += -std=c++11
CXXFLAGS += -I$(GTEST_DIR)/include
CXXFLAGS += -I$(GMOCK_DIR)/include
CXXFLAGS += -I$(TLDK_ROOT)/$(RTE_TARGET)/include

LDFLAGS += -lstdc++
LDFLAGS += -L$(GMOCK_DIR) -lgmock
LDLIBS += -ltle_udp -ltle_dring

include $(TLDK_ROOT)/mk/tle.cpp-obj.mk
endif

#
# building app
#

ifeq ($(X),app)

# binary name
APP = gtest-rfc

# all source are stored in SRCS-y

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += gtest-rfc.o

LDFLAGS += -lstdc++
LDFLAGS += -L$(GMOCK_DIR) -lgmock
LDLIBS += -L$(TLDK_ROOT)/$(RTE_TARGET)/lib
LDLIBS += -ltle_udp -ltle_dring

include $(TLDK_ROOT)/mk/tle.app.mk
endif

endif
endif