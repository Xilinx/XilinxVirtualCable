######################################################################
# Copyright (c) 2020 Xilinx, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
######################################################################ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
CC=aarch64-linux-gnu-gcc



ifndef XVCMEM_VERSION
  XVCMEM_VERSION:=2020.1
endif

#CC = gcc

CFLAGS = -Wall

BINDIR = bin

TARGET = xvc_mem

OBJDIR := obj
OBJS := $(addprefix $(OBJDIR)/,xvc_mem.o xvcserver.o)

debug: DEBUG = -ggdb
debug: all

$(OBJDIR)/%.o : %.c
	@$(CC) -DXVCMEM_VERSION=\"$(XVCMEM_VERSION)\" $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(TARGET) $(OBJS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

clean: 
	@$(RM) -r $(OBJDIR)
	@$(RM) -r $(BINDIR)
