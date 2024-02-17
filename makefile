DEPS := fuse3 openssl gtest_main

test: rudefs rude-gtests
	./rude-gtests

%.o: %.cpp
	g++ --std=c++20 -c $< -o $@ `pkg-config --cflags $(DEPS)`

clean:
	rm -rfv src/*.o ./rude-gtests ./rudefs

rudefs: src/rudefs.o src/hash_file.o
	g++ $^ -o $@ `pkg-config --libs $(DEPS)`

rude-gtests: src/tests.o src/hash_file.o
	g++ $^ -o $@ `pkg-config --libs $(DEPS)`

rude-mnt:
	mkdir -p $@

rude-store:
	mkdir -p $@/root
	mkdir -p $@/hashmap


fg: rudefs rude-mnt rude-store
	echo "Mounting in foreground..."
	./rudefs -f --stingy --backing=rude-store $(shell pwd)/rude-mnt

fg-md5: rudefs rude-mnt rude-store
	echo "Mounting in foreground..."
	./rudefs -f --stingy --backing=rude-store --hashfn=md5 $(shell pwd)/rude-mnt

reset:
	rm -rfv rude-store
	$(MAKE) rude-store
	sync

mount: rudefs rude-mnt rude-store
	./rudefs --backing=$(shell pwd)/rude-store $(shell pwd)/rude-mnt
	ls rude-mnt

unmount:
	fusermount3 -u $(shell pwd)/rude-mnt
	ls rude-mnt
