

rudefs: src/rudefs.c
	gcc src/rudefs.c -o rudefs `pkg-config fuse3 --cflags --libs`

rude-mnt:
	mkdir -p $@

rude-store:
	mkdir -p $@/root
	echo "This is a test file" > $@/root/samplefile
	mkdir -p $@/hashmap


fg: rudefs rude-mnt rude-store
	echo "Mounting in foreground..."
	./rudefs -f --backing=$(shell pwd)/rude-store $(shell pwd)/rude-mnt

mount: rudefs rude-mnt rude-store
	./rudefs --backing=$(shell pwd)/rude-store $(shell pwd)/rude-mnt
	ls rude-mnt

unmount:
	fusermount3 -u $(shell pwd)/rude-mnt
	ls rude-mnt

