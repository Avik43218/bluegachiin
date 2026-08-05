#!/bin/bash

echo -e "\n========== BLUEGACHIIN ==========\n"

if [[ "$1" == "-e" ]]; then

	read -p "Enter target image path: " TARGET_IMG_PATH
	read -p "Enter destination directory: " DEST_DIR
	read -p "Enter payload: " PAYLOAD

	cp "$TARGET_IMG_PATH" .
	IMG_TITLE=$(basename "$TARGET_IMG_PATH")

	make
	./bin/bg --encode "$IMG_TITLE" "$PAYLOAD" "$BG_SECRET_KEY"

	mv F_*.png "$DEST_DIR"
	rm "$IMG_TITLE"

elif [[ "$1" == "-d" ]]; then

	read -p "Enter path of image to decode: " TARGET_IMG_PATH

	cp "$TARGET_IMG_PATH" .
	IMG_TITLE=$(basename "$TARGET_IMG_PATH")

	make
	./bin/bg --decode "$IMG_TITLE" "$BG_SECRET_KEY"

	rm "$IMG_TITLE"

fi



