all: build
build: libckpt.c checkpoint.c readckpt.c restart.c
	gcc -shared -fPIC -o libckpt.so libckpt.c
	gcc checkpoint.c -o checkpoint.o
	gcc -g readckpt.c -o readckpt.o
	gcc -static \
        -Wl,-Ttext-segment=5000000 -Wl,-Tdata=5100000 -Wl,-Tbss=5200000 \
        -g3 -o restart restart.c
check: checkpoint.c
	make build
	./checkpoint.o &
	sleep 2 
	echo Checkpointing...
	kill -12 $$(pgrep -n checkpoint)
	sleep 2
	echo Restarting...
	pkill -9 checkpoint
	./restart
	

clean: 
	rm libckpt.so
	rm checkpoint.o
	rm myckpt
	rm readckpt.o
	rm restart
