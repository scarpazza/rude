
%.o: %.c
	gcc -c $< -o $@ \
	`pkg-config fuse3   --cflags` \
	`pkg-config openssl --cflags`


rudefs: src/rudefs.o src/hash_file.o
	gcc $^ -o $@ \
	`pkg-config fuse3   --libs` \
	`pkg-config openssl --libs`

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
