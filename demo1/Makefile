CC = gcc
CFLAGS = -g -Wall
LIBS = -lurcu-cds -lurcu

output_exe_files = cds_lfq_dequeue
all: $(output_exe_files)
cds_lfq_dequeue: cds_lfq_dequeue.o
	$(LINK.o) -o $@ $^ $(LIBS)
cds_lfq_enqueue: cds_lfq_enqueue.o
	$(LINK.o) -o $@ $^ $(LIBS)
clean:
	$(RM) $(output_exe_files) *.o
.PHONY: all clean
