.PHONY: clean
	
msp: build msp.c
	cc -o build/msp -framework CoreFoundation -framework IOKit msp.c

build:
	mkdir build
			
clean:
	rm build/* 2>/dev/null  || true
	rmdir build 2>/dev/null || true
