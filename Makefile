# GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug)
  Scheduler_config = debug
  SchedulerTest_config = debug
  Interpreter_config = debug
  Integration_config = debug
  ProcessTableTest_config = debug
endif
ifeq ($(config),release)
  Scheduler_config = release
  SchedulerTest_config = release
  Interpreter_config = release
  Integration_config = release
  ProcessTableTest_config = release
endif

PROJECTS := Scheduler SchedulerTest Interpreter Integration ProcessTableTest

.PHONY: all clean help $(PROJECTS) 

all: $(PROJECTS)

Scheduler:
ifneq (,$(Scheduler_config))
	@echo "==== Building Scheduler ($(Scheduler_config)) ===="
	@${MAKE} --no-print-directory -C . -f Scheduler.make config=$(Scheduler_config)
endif

SchedulerTest:
ifneq (,$(SchedulerTest_config))
	@echo "==== Building SchedulerTest ($(SchedulerTest_config)) ===="
	@${MAKE} --no-print-directory -C . -f SchedulerTest.make config=$(SchedulerTest_config)
endif

Interpreter: Scheduler
ifneq (,$(Interpreter_config))
	@echo "==== Building Interpreter ($(Interpreter_config)) ===="
	@${MAKE} --no-print-directory -C . -f Interpreter.make config=$(Interpreter_config)
endif

Integration: Scheduler
ifneq (,$(Integration_config))
	@echo "==== Building Integration ($(Integration_config)) ===="
	@${MAKE} --no-print-directory -C . -f Integration.make config=$(Integration_config)
endif

ProcessTableTest:
ifneq (,$(ProcessTableTest_config))
	@echo "==== Building ProcessTableTest ($(ProcessTableTest_config)) ===="
	@${MAKE} --no-print-directory -C . -f ProcessTableTest.make config=$(ProcessTableTest_config)
endif

clean:
	@${MAKE} --no-print-directory -C . -f Scheduler.make clean
	@${MAKE} --no-print-directory -C . -f SchedulerTest.make clean
	@${MAKE} --no-print-directory -C . -f Interpreter.make clean
	@${MAKE} --no-print-directory -C . -f Integration.make clean
	@${MAKE} --no-print-directory -C . -f ProcessTableTest.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   Scheduler"
	@echo "   SchedulerTest"
	@echo "   Interpreter"
	@echo "   Integration"
	@echo "   ProcessTableTest"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"