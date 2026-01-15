all:
	bun x tsgo
	bun run src/index.ts

esbuild:
	esbuild --format=esm --external:pak:wifi --bundle src/index.ts

x:
	cat dist/index.js

all3:
	bun run index.ts

all2:
	esbuild src/index.ts
