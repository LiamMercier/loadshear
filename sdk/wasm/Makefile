# Copyright (c) 2026 Liam Mercier
#
# This file is part of Loadshear.
#
# Loadshear is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License Version 3.0
# as published by the Free Software Foundation.
#
# Loadshear is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
# for more details.
#
# You should have received a copy of the GNU General Public License v3.0
# along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

FLAGS = --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--initial-memory=131072

HEADERFLAG = -Wl,--export=handle_header

all: tcp-single-session-heartbeat.wasm tcp-single-session-parsing.wasm

tcp-single-session-heartbeat.wasm: tcp-single-session-heartbeat.cpp
	clang $(FLAGS) $(HEADERFLAG) -o $@ $<

tcp-single-session-parsing.wasm: tcp-single-session-parsing.c
	clang $(FLAGS) -o $@ $<

strip:
	for f in *.wasm; do wasm-strip "$$f"; done
