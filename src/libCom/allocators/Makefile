# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/allocators
INC += epicsAllocator.hpp
INC += epicsFreeListAllocator.hpp
INC += epicsMallocator.hpp
Com_SRCS += epicsMallocator.cpp
# Note: we need to use the deprecated "-std=c++0x" here since the g++ 4.4.7
# cross compiler for RTEMS used on Travis CI doesn't understand "-std=c++11".
epicsMallocator_CXXFLAGS += -std=c++0x
INC += epicsSTLAllocator.hpp
