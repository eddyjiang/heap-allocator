# A simple makefile for building a program composed of C source files.

# gcc optimization flags
implicit.o: CFLAGS += -O2
explicit.o: CFLAGS += -O2

ALLOCATORS = bump implicit explicit
PROGRAMS = $(ALLOCATORS:%=test_%)
MY_PROGRAMS = $(ALLOCATORS:%=my_optional_program_%)

# This auto-commits changes on a successful make and if the tool_run environment variable is not set.
all:: $(PROGRAMS) $(MY_PROGRAMS)
	@retval=$$?;\
	if [ -z "$$tool_run" ]; then\
		if [ $$retval -eq 0 ]; then\
		    git diff --quiet --exit-code;\
			retval=$$?;\
			if [ $$retval -eq 1 ]; then\
				git log tools/create | grep 'Author' -m 1 | cut -d : -f 2 | cut -c 2- | xargs -I{} git commit -a -m "successful make." --author={} --quiet;\
				git push --quiet;\
				fi;\
		fi;\
	fi

CC = gcc
CFLAGS = -g3 -std=gnu99 -Wall $$warnflags -fcf-protection=none -fno-pic -no-pie
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
LDFLAGS =
LDLIBS =

$(PROGRAMS): test_%:%.o segment.c test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): my_optional_program_%:my_optional_program.c %.o segment.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	@rm -f $(PROGRAMS) $(MY_PROGRAMS) *.o callgrind.out.*

.PHONY: clean all

.INTERMEDIATE: $(ALLOCATORS:%=%.o)
