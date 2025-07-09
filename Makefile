bpftool := /home/jdLu/bpftool
name ?= trace_read_path
prog_bpf = $(name).bpf.c
prog_bpf_o = $(name).bpf.o
prog_bpf_h = $(name).skel.h
prog_usr = $(name).c
prog_usr_o = $(name)

bpf:
	clang -O2 -g -fno-builtin -target bpf -D__TARGET_ARCH_x86 \
	-I/home/jdLu/Diploma/linux-5.15.19/tool/lib/ \
	-c $(prog_bpf) -o $(prog_bpf_o)

bpf_skel: bpf
	$(bpftool) gen skeleton $(prog_bpf_o) > $(prog_bpf_h)

build: bpf_skel
	gcc -g -O2 -Wall -o $(prog_usr_o) $(prog_usr) -I. -lbpf -lelf -lz

run: build
	./$(prog_bpf_o)

clean:
	rm -f $(prog_bpf_o)
	rm -f $(prog_bpf_h)
	rm -f $(prog_usr_o)