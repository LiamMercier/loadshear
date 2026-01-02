FLAGS = --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--initial-memory=131072

HEADERFLAG = -Wl,--export=handle_header

all: tcp-single-session-heartbeat.wasm tcp-single-session-parsing.wasm

tcp-single-session-heartbeat.wasm: tcp-single-session-heartbeat.cpp
	clang $(FLAGS) $(HEADERFLAG) -o $@ $<

tcp-single-session-parsing.wasm: tcp-single-session-parsing.c
	clang $(FLAGS) -o $@ $<

strip:
	for f in *.wasm; do wasm-strip "$$f"; done
