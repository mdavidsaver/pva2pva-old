TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY += qsrv qsrvDemo

SHRLIB_VERSION ?= $(EPICS_QSRV_ABI_MAJOR_VERSION).$(EPICS_QSRV_ABI_MINOR_VERSION)

USR_CPPFLAGS += -I$(TOP)/common -I$(TOP)/p2pApp

qsrv_SRCS += pvif.cpp
qsrv_SRCS += qsrv.cpp
qsrv_SRCS += pdb.cpp
qsrv_SRCS += pdbsingle.cpp
#qsrv_SRCS += pvalink.cpp

ifdef BASE_3_16
qsrv_SRCS += pdbgroup.cpp
qsrv_SRCS += configparse.cpp
endif

qsrv_LIBS += pvAccess pvData
qsrv_LIBS += $(EPICS_BASE_IOC_LIBS)

qsrvDemo_SRCS += demo.cpp
qsrvDemo_SRCS += imagedemo.c

qsrvDemo_LIBS += $(EPICS_BASE_IOC_LIBS)

FINAL_LOCATION ?= $(shell $(PERL) $(TOOLS)/fullPathName.pl $(INSTALL_LOCATION))

USR_CPPFLAGS += -DFINAL_LOCATION="\"$(FINAL_LOCATION)\""

PROD_IOC += softIocPVA

softIocPVA_SRCS += softMain.cpp
softIocPVA_SRCS += softIocPVA_registerRecordDeviceDriver.cpp

softIocPVA_LIBS += qsrvDemo
softIocPVA_LIBS += qsrv
softIocPVA_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)
softIocPVA_LIBS += $(EPICS_BASE_IOC_LIBS)

DBD += softIocPVA.dbd
DBD += qsrv.dbd

softIocPVA_DBD += softIoc.dbd
softIocPVA_DBD += PVAServerRegister.dbd
softIocPVA_DBD += qsrv.dbd
softIocPVA_DBD += qsrvDemo.dbd

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

