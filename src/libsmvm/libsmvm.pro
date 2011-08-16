#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

include(../../vm.pri)

TEMPLATE = lib
TARGET = smvm
DESTDIR = ../../lib/

LIBS += -Wl,-rpath-link=../../lib -L../../lib -lsmvmi -lsme

SOURCES += \
    vm_internal_core.c \
    vm_internal_helpers.c

OTHER_FILES += \
    vm.h \
    vm_internal_core.h \
    vm_internal_helpers.h
