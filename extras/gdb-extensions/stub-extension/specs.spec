#	Copyright (C) 2026 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# This specs avoids all standard CRT stuff and default link script.

*link:
-nostdlib --emit-relocs --no-warn-rwx-segments -static --no-warn-execstack

*endfile:


*lib:


*libgcc:


*startfile:


